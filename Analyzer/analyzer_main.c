#include <stdio.h>
#include <gst/gst.h>
#include "loader.h"

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
  
  GError *error = NULL;
  loader_t *loader_state = loader_open(argv[1], &error);
  
  if (error != NULL)
  {
    fprintf(stderr, "%s\n", error->message);
    return 2;
  }
  
  printf("File %s:\n", argv[1]); 
  printf("    Demuxer:          %s\n", loader_get_demux(loader_state));
  printf("    Video codec:      %s\n", loader_get_video_decoder(loader_state));
  printf("    Audio codec:      %s\n", loader_get_audio_decoder(loader_state));
  
  
  loader_close(loader_state);
  
  return 0;
}