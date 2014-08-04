#include <stdio.h>
#include <unistd.h>
#include <gst/gst.h>
#include "loader.h"
#include "layout.h"
#include "lipsync.h"
#include "markertype.h"

GST_DEBUG_CATEGORY(tvg_analyzer_debug);
#define GST_CAT_DEFAULT tvg_analyzer_debug

typedef struct {
  const char *filename;
  GArray *markers; /* marker_t, detected markers */
  int num_frames;
  GArray *lipsync_markers; /* lipsync_marker_t, detected beeps */
  GArray *frame_data; /* char*, detected marker states in frames */
  GArray *frame_times; /* GstClockTime */
  int rgb6_marker_index; /* Index of the RGB6 marker */
  int samplerate; /* Audio samplerate */
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
  float framerate;
  
  *error = NULL;
  loader_state = loader_open(main_state->filename, error);
  if (*error != NULL)
    return false;
  
  loader_get_resolution(loader_state, &width, &height, &stride);
  main_state->samplerate = loader_get_samplerate(loader_state);
  framerate = loader_get_framerate(loader_state);
  
  printf("File %s:\n", main_state->filename); 
  printf("    Demuxer:          %s\n", loader_get_demux(loader_state));
  printf("    Video codec:      %s\n", loader_get_video_decoder(loader_state));
  printf("    Audio codec:      %s\n", loader_get_audio_decoder(loader_state));
  printf("    Video resolution: %dx%d\n", width, height);
  
  if (framerate != 0)
    printf("    Framerate:        %0.2f\n", framerate);
  else
    printf("    Framerate:        variable\n");
  
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
        GST_INFO("Processing frame %d\n", num_frames);
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
  
  main_state->markers = layout_fetch(layout_state);
  printf("    Markers found:    %d\n", main_state->markers->len);
  
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
  
  lipsync_state = lipsync_create(main_state->samplerate);
  main_state->frame_data = g_array_new(false, false, sizeof(char*));
  main_state->frame_times = g_array_new(false, false, sizeof(GstClockTime));
  
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
        g_array_append_val(main_state->frame_times, video_buf->pts);
        
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
  lipsync_free(lipsync_state);
  loader_close(loader_state);
  return true;
}

/* Print the collected information about markers */
static void print_marker_info(main_state_t *main_state)
{
  videoinfo_t *videoinfo = markertype_analyze(main_state->frame_data);
  size_t i;
  
  main_state->rgb6_marker_index = -1;
  for (i = 0; i < main_state->markers->len; i++)
  {
    marker_t *marker = &g_array_index(main_state->markers, marker_t, i);
    markerinfo_t *info = &videoinfo->markerinfo[i];
    
    printf("      Marker %3d:     at (%4d,%4d) size %3dx%-3d crc %08x", (int)i,
            marker->x1, marker->y1,
            marker->x2 - marker->x1 + 1, marker->y2 - marker->y1 + 1,
            marker->crc
          );
    
    if (info->type == TVG_MARKER_SYNCMARK)
      printf(" type SYNCMARK interval %d\n", info->interval);
    else if (info->type == TVG_MARKER_FRAMEID)
      printf(" type FRAMEID interval %d\n", info->interval);
    else if (info->type == TVG_MARKER_RGB6)
      printf(" type RGB6\n");
    else
      printf(" type UNKNOWN\n");
    
    if (info->type == TVG_MARKER_RGB6)
      main_state->rgb6_marker_index = i;
  }
  
  printf("    Video structure:\n");
  printf("      Header:       %5d frames\n", videoinfo->num_header_frames);
  printf("      Locator:      %5d frames\n", videoinfo->num_locator_frames);
  printf("      Content:      %5d frames\n", videoinfo->num_content_frames);
  printf("      Trailer:      %5d frames\n", videoinfo->num_trailer_frames);
  
  markertype_free(videoinfo);
}

/* Calculate statistics about lipsync markers */
static void print_lipsync_info(main_state_t *main_state)
{
  size_t beep_index = 0;
  size_t frame_index = 0;
  int video_markers = 0;
  float min_lipsync = 0;
  float max_lipsync = 0;
  for (frame_index = 0; frame_index < main_state->frame_data->len; frame_index++)
  {
    char c = g_array_index(main_state->frame_data, char*, frame_index)
              [main_state->rgb6_marker_index];
    if (c == 'k')
    {
      if (beep_index < main_state->lipsync_markers->len)
      {
        /* Lipsync frame, compare to matching lipsync beep */
        GstClockTimeDiff frame_time = g_array_index(main_state->frame_times,
                                                    GstClockTime, frame_index);
        lipsync_marker_t beep = g_array_index(main_state->lipsync_markers,
                                              lipsync_marker_t, beep_index++);
        GstClockTimeDiff beep_start = beep.start_sample * GST_SECOND / main_state->samplerate;
        
        float delta = (float)(beep_start - frame_time) / GST_SECOND;
        
        if (video_markers == 0)
        {
          min_lipsync = delta;
          max_lipsync = delta;
        }
        else
        {
          if (delta < min_lipsync) min_lipsync = delta;
          if (delta > max_lipsync) max_lipsync = delta;
        }
      }
      
      video_markers++;
    }
  }
  
  printf("    Lipsync:          %d audio markers, %d video markers\n",
         main_state->lipsync_markers->len, video_markers);
  
  if (main_state->lipsync_markers->len > 0 && video_markers > 0)
  {
    printf("    Audio delay:      min %0.1f ms, max %0.1f ms\n",
           1000 * min_lipsync, 1000 * max_lipsync);
  }
}

static void save_details(main_state_t *main_state)
{
  size_t frame_index, lipsync_index = 0;
  FILE *f;
  
  f = fopen("frames.txt", "w");
  
  if (!f)
    return;
  
  for (frame_index = 0; frame_index < main_state->frame_data->len; frame_index++)
  {
    GstClockTime frame_time = g_array_index(main_state->frame_times, GstClockTime, frame_index);
    char * frame_data = g_array_index(main_state->frame_data, char*, frame_index);
    
    if (lipsync_index < main_state->lipsync_markers->len)
    {
      lipsync_marker_t beep = g_array_index(main_state->lipsync_markers, lipsync_marker_t, lipsync_index);
      GstClockTime beep_start = beep.start_sample * GST_SECOND / main_state->samplerate;
      GstClockTime beep_end = beep.end_sample * GST_SECOND / main_state->samplerate;
      
      if (beep_start <= frame_time)
      {
        fprintf(f, "AUDIO: %8d %5d\n",
                (int)(beep_start / GST_USECOND),
                (int)((beep_end - beep_start) / GST_USECOND)
              );
        lipsync_index++;
      }
    }
    
    
    fprintf(f, "VIDEO: %8d %s\n", (int)(frame_time / GST_USECOND), frame_data);    
  }
  
  fclose(f);
  
  printf("    Saved frame data to frames.txt\n");
}

int main(int argc, char *argv[])
{
  /* Initialize gstreamer. Will handle any gst-specific commandline options. */
  gst_init(&argc, &argv);
  GST_DEBUG_CATEGORY_INIT (tvg_analyzer_debug, "tvg_analyzer", 0, "OF TVG Video Analyzer");
  
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
    
    print_marker_info(&main_state);
    print_lipsync_info(&main_state);
    save_details(&main_state);
  }
  
  return 0;
}