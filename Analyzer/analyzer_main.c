#include <stdio.h>
#include <unistd.h>
#include <gst/gst.h>
#include "loader.h"
#include "layout.h"
#include "lipsync.h"

typedef struct {
  const char *filename;
  GArray *markers; /* marker_t, detected markers */
  int num_frames;
  GArray *lipsync_markers; /* lipsync_marker_t, detected beeps */
  GArray *frame_data; /* char*, detected marker states in frames */
} main_state_t;

/* First pass through the input video:
 * - Count number of frames
 * - Detect the location of markers in video
 */
bool first_pass(main_state_t *main_state, GError **error)
{
  int width, height, stride;
  layout_t *layout_state;
  loader_t *loader_state;
  
  *error = NULL;
  loader_state = loader_open(main_state->filename, error);
  if (*error != NULL)
    return false;
  
  loader_get_resolution(loader_state, &width, &height, &stride);
  
  printf("File %s:\n", main_state->filename); 
  printf("    Demuxer:          %s\n", loader_get_demux(loader_state));
  printf("    Video codec:      %s\n", loader_get_video_decoder(loader_state));
  printf("    Audio codec:      %s\n", loader_get_audio_decoder(loader_state));
  printf("    Video resolution: %dx%d\n", width, height);
  
  layout_state = layout_create(width, height);
  
  int num_frames = 0;
  GstBuffer *audio_buf, *video_buf;
  GstClockTime video_end_time = 0, audio_end_time = 0;
  while (loader_get_buffer(loader_state, &audio_buf, &video_buf, error))
  {
    if (video_buf != NULL)
    {
      num_frames++;
      
      if (isatty(1))
      {
        printf("[%5d]  \r", num_frames);
        fflush(stdout);
      }
      
      {
        GstMapInfo mapinfo;
        gst_buffer_map(video_buf, &mapinfo, GST_MAP_READ);
        layout_process(layout_state, mapinfo.data, stride);
        gst_buffer_unmap(video_buf, &mapinfo);
      }
      
      video_end_time = video_buf->pts + video_buf->duration;
      gst_buffer_unref(video_buf);
    }
    
    if (audio_buf != NULL)
    {
      audio_end_time = audio_buf->pts + audio_buf->duration;
      gst_buffer_unref(audio_buf);
    }
    
    if (*error != NULL)
      return false;
  }
  
  main_state->num_frames = num_frames;
  printf("    Number of frames: %d (total)\n", num_frames);
  printf("    Video length:     %0.3f s\n", (float)video_end_time / GST_SECOND);
  printf("    Audio length:     %0.3f s\n", (float)audio_end_time / GST_SECOND);
  
  {
    GArray *markers = layout_fetch(layout_state);
    main_state->markers = markers;
    
    size_t i;
    printf("    Markers found:    %d\n", markers->len);
    for (i = 0; i < markers->len; i++)
    {
      marker_t *marker = &g_array_index(markers, marker_t, i);
      printf("      %3d: at (%4d,%4d) size %dx%d type %s\n", (int)i,
             marker->x1, marker->y1,
             marker->x2 - marker->x1 + 1, marker->y2 - marker->y1 + 1,
             marker->is_rgb ? "RGB" : "BW");
    }
  }
  
  loader_close(loader_state);
  layout_free(layout_state);
  return true;
}

/* Second pass through the input video:
 * - Read the states of video markers
 * - Detect audio markers
 */
bool second_pass(main_state_t *main_state, GError **error)
{
  int width, height, stride;
  loader_t *loader_state;
  lipsync_t *lipsync_state;
  
  *error = NULL;
  loader_state = loader_open(main_state->filename, error);
  if (*error != NULL)
    return false;
  
  loader_get_resolution(loader_state, &width, &height, &stride);
  
  lipsync_state = lipsync_create(44100);
  main_state->frame_data = g_array_new(false, false, sizeof(char*));
  
  int num_frames = 0;
  GstBuffer *audio_buf, *video_buf;
  while (loader_get_buffer(loader_state, &audio_buf, &video_buf, error))
  {
    if (video_buf != NULL)
    {
      num_frames++;
      
      if (isatty(1))
      {
        printf("[%5d/%5d]  \r", num_frames, main_state->num_frames);
        fflush(stdout);
      }
      
      {
        GstMapInfo mapinfo;
        gst_buffer_map(video_buf, &mapinfo, GST_MAP_READ);
        
        char *states = layout_read_markers(main_state->markers, mapinfo.data, stride);
        g_array_append_val(main_state->frame_data, states);        
        
        gst_buffer_unmap(video_buf, &mapinfo);
      }
      
      gst_buffer_unref(video_buf);
    }
    
    if (audio_buf != NULL)
    {
      {
        GstMapInfo mapinfo;
        gst_buffer_map(audio_buf, &mapinfo, GST_MAP_READ);
        
        lipsync_process(lipsync_state, (const int16_t*)mapinfo.data, mapinfo.size / 2);
        
        gst_buffer_unmap(audio_buf, &mapinfo);
      }
      gst_buffer_unref(audio_buf);
    }
    
    if (*error != NULL)
      return false;
  }
  
  main_state->lipsync_markers = lipsync_fetch(lipsync_state);
  lipsync_free(
  loader_close(loader_state);
  return true;
}

int main(int argc, char *argv[])
{
  /* Initialize gstreamer. Will handle any gst-specific commandline options. */
  gst_init(&argc, &argv);
  
  /* There should be only one remaining argument. */
  if (argc != 2)
  {
    fprintf(stderr, "Usage: %s [gst options] <video file>\n", argv[0]);
    return 1;
  }
  
  {
    main_state_t main_state = {0};
    GError *error = NULL;
    
    main_state.filename = argv[1];
    
    if (!first_pass(&main_state, &error))
    {
      fprintf(stderr, "%s\n", error->message);
      g_error_free(error);
      return 2;
    }
    
    if (!second_pass(&main_state, &error))
    {
      fprintf(stderr, "%s\n", error->message);
      g_error_free(error);
      return 3;
    }
  }
  
  return 0;
}