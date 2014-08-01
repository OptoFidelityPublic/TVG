/* Lipsync audio marker detection.
 * Uses a simple DFT transform at specific frequencies to keep
 * a rolling window value of the DFT. The thresholds are hardcoded
 * to 1/4 the dynamic range. This should be fine for TVG generated
 * videos.
 */

#ifndef _TVG_LIPSYNC_DETECTOR_H_
#define _TVG_LIPSYNC_DETECTOR_H_

#include <stdint.h>
#include <glib.h>

#define TVG_LIPSYNC_FREQ1 547
#define TVG_LIPSYNC_FREQ2 1823
#define TVG_LIPSYNC_THRESHOLD 1000
#define TVG_LIPSYNC_HYSTERESIS 100
#define TVG_LIPSYNC_BUFFER_LENGTH 500

typedef struct _lipsync_t lipsync_t;

typedef struct {
  int start_sample;
  int end_sample;
} lipsync_marker_t;

/* Create a new context for lipsync detector */
lipsync_t *lipsync_create(int samplerate);

/* Release all resources associated with the context */
void lipsync_free(lipsync_t *lipsync);

/* Feed a new buffer of audio data */
void lipsync_process(lipsync_t *lipsync, const int16_t *data, size_t num_samples);

/* Fetch a list of all the lipsync markers detected so far.
 * Returns a new reference to internal array of lipsync_marker_t structures. */
GArray *lipsync_fetch(lipsync_t *lipsync);


#endif
