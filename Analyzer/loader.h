/* Handles the GStreamer code for creating a pipeline, opening
 * file and retrieving audio/video data. */

#ifndef _TVG_LOADER_H_
#define _TVG_LOADER_H_

#include <gst/gst.h>

typedef struct _loader_t loader_t;

/* Load the given video file and setup a pipeline to parse it. */
loader_t *loader_open(const gchar *filename, GError **error);

/* Release all resources associated to the state. */
void loader_close(loader_t *state);

/* Get descriptive name of the video demuxer element */
const gchar *loader_get_demux(loader_t *state);

/* Get descriptive name of the video decompressor element */
const gchar *loader_get_video_decoder(loader_t *state);

/* Get descriptive name of the audio decompressor element */
const gchar *loader_get_audio_decoder(loader_t *state);



#endif
