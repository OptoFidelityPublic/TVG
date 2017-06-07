#include "lipsync.h"
#include <stdbool.h>
#include <math.h>
#include <complex.h>

struct _lipsync_t
{
  int samplerate;
  int sample_index;
  int16_t *past_samples;
  GArray *detected_markers;
  
  float complex freq_1_dft;
  float complex freq_2_dft;
  
  bool beep_is_on;
  lipsync_marker_t current_marker;
  float max_value_during_beep;
};

lipsync_t *lipsync_create(int samplerate)
{
  lipsync_t *lipsync = g_malloc0(sizeof(lipsync_t));
  
  lipsync->samplerate = samplerate;
  lipsync->sample_index = 0;
  lipsync->past_samples = g_malloc0(sizeof(int16_t) * TVG_LIPSYNC_BUFFER_LENGTH);
  lipsync->detected_markers = g_array_new(false, false, sizeof(lipsync_marker_t));
  
  return lipsync;
}

void lipsync_free(lipsync_t *lipsync)
{
  g_free(lipsync->past_samples); lipsync->past_samples = NULL;
  g_array_unref(lipsync->detected_markers); lipsync->detected_markers = NULL;
  g_free(lipsync);
}

static float complex dft_term(lipsync_t *lipsync, int index, int16_t sample, int freq)
{
  return sample * cexpf(-I * 2 * M_PI * index * freq / lipsync->samplerate);
}

static void add_sample(lipsync_t *lipsync, int16_t sample)
{
  int i = lipsync->sample_index % TVG_LIPSYNC_BUFFER_LENGTH;
  int index = lipsync->sample_index;
  int old_index = lipsync->sample_index - TVG_LIPSYNC_BUFFER_LENGTH;
  int16_t old_sample = lipsync->past_samples[i];
  
  lipsync->freq_1_dft -= dft_term(lipsync, old_index, old_sample, TVG_LIPSYNC_FREQ1);
  lipsync->freq_2_dft -= dft_term(lipsync, old_index, old_sample, TVG_LIPSYNC_FREQ2);
  
  lipsync->past_samples[i] = sample;
  lipsync->sample_index++;
  
  lipsync->freq_1_dft += dft_term(lipsync, index, sample, TVG_LIPSYNC_FREQ1);
  lipsync->freq_2_dft += dft_term(lipsync, index, sample, TVG_LIPSYNC_FREQ2);
}

void lipsync_process(lipsync_t *lipsync, GstClockTime buf_start_time, int samplerate, const int16_t *data, size_t num_samples)
{
  const int start_threshold = TVG_LIPSYNC_THRESHOLD + TVG_LIPSYNC_HYSTERESIS;
  const int end_threshold = TVG_LIPSYNC_THRESHOLD - TVG_LIPSYNC_HYSTERESIS;
  
  int buf_start_sample = lipsync->sample_index;
  
  size_t i;
  for (i = 0; i < num_samples; i++)
  {
    add_sample(lipsync, data[i]);
    
    float value = cabs(lipsync->freq_1_dft) * cabs(lipsync->freq_2_dft);
    value = sqrtf(value) / TVG_LIPSYNC_BUFFER_LENGTH;
    
    if (!lipsync->beep_is_on && value > start_threshold)
    {
      /* Start of beep */
      lipsync->beep_is_on = true;
      lipsync->current_marker.start_sample = lipsync->sample_index;
      lipsync->max_value_during_beep = value;
    }
    else if (lipsync->beep_is_on)
    {
      if (value > lipsync->max_value_during_beep)
      {
        lipsync->max_value_during_beep = value;
      }
      
      if (value < end_threshold)
      {
        /* End of beep */
        lipsync->beep_is_on = false;
        lipsync->current_marker.end_sample = lipsync->sample_index;
        
        /* Compensate for the filter length */
        int max_value = lipsync->max_value_during_beep;
        lipsync->current_marker.start_sample -= TVG_LIPSYNC_BUFFER_LENGTH
                                                * start_threshold / max_value;
        lipsync->current_marker.end_sample -= TVG_LIPSYNC_BUFFER_LENGTH
                                              * (max_value - end_threshold) / max_value;
        
        if (lipsync->current_marker.start_sample < 0)
          lipsync->current_marker.start_sample = 0;
        
        /* Calculate the beep start time based on the buffer timestamp. This
         * allows it to work accurately even when there are some discontinuities
         * or time drift in the audio stream. */
        lipsync->current_marker.start_time = buf_start_time + (float)(lipsync->current_marker.start_sample - buf_start_sample) * GST_SECOND / samplerate;
        
        g_array_append_val(lipsync->detected_markers, lipsync->current_marker);
      }
    }
  }
}

GArray *lipsync_fetch(lipsync_t *lipsync)
{
  return g_array_ref(lipsync->detected_markers);
}
