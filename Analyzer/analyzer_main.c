#include <stdio.h>
#include <gst/gst.h>
#include "loader.h"

bool first_pass(const char *filename, GError **error)
{
  *error = NULL;
  loader_t *loader_state = loader_open(filename, error);
  
  if (*error != NULL)
    return false;
  
  printf("File %s:\n", filename); 
  printf("    Demuxer:          %s\n", loader_get_demux(loader_state));
  printf("    Video codec:      %s\n", loader_get_video_decoder(loader_state));
  printf("    Audio codec:      %s\n", loader_get_audio_decoder(loader_state));
  
  int num_frames = 0;
  GstBuffer *audio_buf, *video_buf;
  while (loader_get_buffer(loader_state, &audio_buf, &video_buf, error))
  {
    if (video_buf != NULL)
    {
      num_frames++;
      printf("[%5d]  \r", num_frames);
      fflush(stdout);
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