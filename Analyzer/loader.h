/* Handles the GStreamer code for creating a pipeline, opening
 * file and retrieving audio/video data. */

#ifndef _TVG_LOADER_H_
#define _TVG_LOADER_H_

#include <gst/gst.h>
#include <stdbool.h>

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

/* Get resolution of video frames */
void loader_get_resolution(loader_t *state, int *width, int *height);

/* Retrieve video/audio buffers. Atleast one of the returned pointers
 * is non-NULL after the call, *except* on end-of-stream when it returns false. */
bool loader_get_buffer(loader_t *state, GstBuffer **audio_buf,
                       GstBuffer **video_buf, GError **error);


#endif
