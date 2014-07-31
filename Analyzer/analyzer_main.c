#include <stdio.h>
#include <gst/gst.h>
#include "loader.h"
#include "layout.h"

bool first_pass(const char *filename, GError **error)
{
  int width, height, stride;
  layout_t *layout_state;
  loader_t *loader_state;
  
  *error = NULL;
  loader_state = loader_open(filename, error);
  if (*error != NULL)
    return false;
  
  loader_get_resolution(loader_state, &width, &height, &stride);
  
  printf("File %s:\n", filename); 
  printf("    Demuxer:          %s\n", loader_get_demux(loader_state));
  printf("    Video codec:      %s\n", loader_get_video_decoder(loader_state));
  printf("    Audio codec:      %s\n", loader_get_audio_decoder(loader_state));
  printf("    Video resolution: %dx%d\n", width, height);
  
  layout_state = layout_create(width, height);
  
  int num_frames = 0;
  GstBuffer *audio_buf, *video_buf;
  while (loader_get_buffer(loader_state, &audio_buf, &video_buf, error))
  {
    if (video_buf != NULL)
    {
      num_frames++;
      printf("[%5d]  \r", num_frames);
      fflush(stdout);
      
      {
        GstMapInfo mapinfo;
        gst_buffer_map(video_buf, &mapinfo, GST_MAP_READ);
        layout_process(layout_state, mapinfo.data, stride);
        gst_buffer_unmap(video_buf, &mapinfo);
      }
      
      gst_buffer_unref(video_buf);
    }
    
    if (audio_buf != NULL)
    {
      gst_buffer_unref(audio_buf);
    }
    
    if (*error != NULL)
      return false;
  }
  
  printf("    Number of frames: %d (total)\n", num_frames);
  
  {
    GArray *markers = layout_fetch(layout_state);
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
    g_array_unref(markers);
  }
  
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
    GError *error = NULL;
    if (!first_pass(argv[1], &error))
    {
      fprintf(stderr, "%s\n", error->message);
      g_error_free(error);
      return 2;
    }
  }
  
  return 0;
}