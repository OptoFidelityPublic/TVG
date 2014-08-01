/* Analyzes the video marker data to detect the marker type. */

#ifndef _TVG_MARKERTYPE_H_
#define _TVG_MARKERTYPE_H_

#include <glib.h>

typedef enum {
  TVG_MARKER_UNKNOWN = 0,
  TVG_MARKER_SYNCMARK = 1,
  TVG_MARKER_FRAMEID = 2,
  TVG_MARKER_RGB6 = 3
} markertype_t;

typedef struct {
  markertype_t type;
  int interval; /* Change interval. 1 = every frame, 2 = every second frame etc. */
} markerinfo_t;

typedef struct {
  int num_header_frames;
  int num_locator_frames;
  int num_content_frames;
  int num_trailer_frames;
  markerinfo_t *markerinfo;
  int num_markers;
} videoinfo_t;

/* Detect the type of each marker in the frame data.
 * frame_data: Array of char* entries with marker colors. */
videoinfo_t *markertype_analyze(GArray *frame_data);

/* Release the allocated arrays */
void markertype_free(videoinfo_t *videoinfo);

#endif
