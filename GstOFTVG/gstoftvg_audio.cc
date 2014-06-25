/*
 * OptoFidelity Test Video Generator
 * Copyright (C) 2011 OptoFidelity <info@optofidelity.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-oftvg_audio
 *
 * A source element that generates beeps as instructed. Cannot be used
 * stand-alone.
 */


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include "gstoftvg_audio.hh"
#include <math.h>

/* Debug category to use */
GST_DEBUG_CATEGORY_EXTERN(gst_oftvg_debug);
#define GST_CAT_DEFAULT gst_oftvg_debug

/* Template for the source pin.
 * We only support a single format, audioconvert shall convert the data
 * to any other format that is needed.
 */
#define SAMPLERATE 44100
#define SAMPLERATE_STR "44100"
static GstStaticPadTemplate sink_template =
GST_STATIC_PAD_TEMPLATE (
  "sink",
  GST_PAD_SINK,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS (
    "audio/x-raw, "
    "format = (string) \"" GST_AUDIO_NE(S16) "\", "
    "rate = (int) " SAMPLERATE_STR ", "
    "layout = (string) interleaved, "
    "channels = (int) 2;"
  )
);
static GstStaticPadTemplate src_template =
GST_STATIC_PAD_TEMPLATE (
  "src",
  GST_PAD_SRC,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS (
    "audio/x-raw, "
    "format = (string) \"" GST_AUDIO_NE(S16) "\", "
    "rate = (int) " SAMPLERATE_STR ", "
    "layout = (string) interleaved, "
    "channels = (int) 2;"
  )
);

/* Definition of the GObject subtype. */
static void gst_oftvg_audio_class_init(GstOFTVG_AudioClass* klass);
static void gst_oftvg_audio_init(GstOFTVG_Audio* filter);
G_DEFINE_TYPE (GstOFTVG_Audio, gst_oftvg_audio, GST_TYPE_BASE_TRANSFORM);

/* Prototypes for the overridden methods */
static gboolean gst_oftvg_audio_start(GstBaseTransform* object);
static GstFlowReturn gst_oftvg_audio_transform_ip (GstBaseTransform *base, GstBuffer *buf);

/* Initializer for the class type */
static void gst_oftvg_audio_class_init(GstOFTVG_AudioClass* klass)
{
  /* GstBaseTransform method overrides */
  {
    GstBaseTransformClass* btrans = GST_BASE_TRANSFORM_CLASS(klass);
    
    btrans->start        = GST_DEBUG_FUNCPTR(gst_oftvg_audio_start);
    btrans->transform_ip = GST_DEBUG_FUNCPTR(gst_oftvg_audio_transform_ip);
  }
  
  /* Element metadata */
  {
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    
    gst_element_class_set_metadata (element_class,
      "OptoFidelity audio marker generator",
      "Source/Audio",
      "Generates lipsync marker beeps",
      "OptoFidelity <info@optofidelity.com>");
    
    gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));
    
    gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_template));
  }
}

/* Initializer for class instances */
static void gst_oftvg_audio_init(GstOFTVG_Audio* filter)
{
  filter->queue = g_async_queue_new();
}

static gboolean gst_oftvg_audio_start(GstBaseTransform* object)
{
  GstOFTVG_Audio *filter = GST_OFTVG_AUDIO(object);
  filter->current = NULL;
  filter->phase = 0;
  return TRUE;
}

/* Type of the structures passed through the queue */
typedef struct _beep_t {
  GstClockTime start; /* Start of the beep */
  GstClockTime end;   /* End of the beep. If end == start, generate just silence. */
} beep_t;

/* Generate a beep with specified start and end time. Add silence between previous time and start. */
void gst_oftvg_audio_generate_beep(GstOFTVG_Audio* element, GstClockTime start, GstClockTime end)
{
  beep_t *entry = (beep_t*)g_malloc(sizeof(beep_t));
  entry->start = start;
  entry->end = end;
  g_async_queue_push(element->queue, entry);
}

/* Generate silence up to the end time */
void gst_oftvg_audio_generate_silence(GstOFTVG_Audio* element, GstClockTime end)
{
  beep_t *entry = (beep_t*)g_malloc(sizeof(beep_t));
  entry->start = end;
  entry->end = end;
  g_async_queue_push(element->queue, entry);
}

/* Generate EOS event on the audio source */
void gst_oftvg_audio_end_stream(GstOFTVG_Audio* element)
{
  beep_t *entry = (beep_t*)g_malloc(sizeof(beep_t));
  entry->start = G_MAXINT64;
  entry->end = G_MAXINT64;
  g_async_queue_push(element->queue, entry);
}

/* Add the beep sound on top of existing audio in the buffer
 * start: index of first sample to modify
 * end:   index of last sample to modify
 * phase: keeps track of the sine wave phase between buffers
 */
static void add_beep(GstBuffer *buffer, int start, int end, int *phase, int num_channels)
{
  /* Map the buffer data to memory */
  GstMapInfo mapinfo;
  if (!gst_buffer_map(buffer, &mapinfo, GST_MAP_WRITE))
  {
    GST_ERROR("Could not map buffer");
    return;
  }
  
  gint16 *data = (gint16*)mapinfo.data;
  
  for (int i = start; i < end; i++)
  {
    float s = 0;
    int p = *phase;
    *phase += 1;
    s += sin(2 * M_PI * 547 * p / SAMPLERATE);
    s += sin(2 * M_PI * 1823 * p / SAMPLERATE);
    
    /* Add to existing data at about 75% volume */
    for (int j = 0; j < num_channels; j++)
    {
      int index = i * num_channels + j;
      float v = 16384 * s + data[index];
      if (v > 32767) v = 32767;
      if (v < -32767) v = -32767;
      data[index] = (gint16)v;
    }
  }
  
  gst_buffer_unmap(buffer, &mapinfo);
}

/* Modify the audio buffer (called by the GstBaseTransform base class) */
GstFlowReturn gst_oftvg_audio_transform_ip(GstBaseTransform *src, GstBuffer *buf)
{
  GstOFTVG_Audio *filter = GST_OFTVG_AUDIO(src);
  int offset = 0;
  int num_channels = 2;
  int buflen = gst_buffer_get_size(buf) / sizeof(gint16) / num_channels;
  bool end_of_stream = false;
  
  GST_DEBUG("Incoming buffer: %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT " (%d samples)",
            GST_TIME_ARGS(GST_BUFFER_PTS(buf)),
            GST_TIME_ARGS(GST_BUFFER_PTS(buf) + GST_BUFFER_DURATION(buf)),
            buflen);
  
  /* Repeat until the whole buffer has been processed */
  while (offset < buflen && !end_of_stream)
  {
    /* Calculate timestamp at this offset */
    GstClockTime start_time = GST_BUFFER_PTS(buf) + GST_SECOND * offset / SAMPLERATE;
    
    /* Fetch a new beep entry if needed */
    int min_len = GST_SECOND / SAMPLERATE;
    if (filter->current == NULL || filter->current->end <= start_time + min_len)
    {
      if (filter->current != NULL)
      {
        GST_DEBUG("Releasing beep that ends at %" GST_TIME_FORMAT ", "
                  "waiting for one at %" GST_TIME_FORMAT,
                  GST_TIME_ARGS(filter->current->end),
                  GST_TIME_ARGS(start_time + min_len));
        g_free(filter->current);
      }
        
      filter->current = (beep_t*) g_async_queue_pop(filter->queue);
      filter->phase = 0;
      
      if (filter->current->start >= G_MAXINT64)
      {
        /* G_MAXINT64 tells us that the video stream has ended */
        GST_DEBUG("End of audio stream");
        end_of_stream = true;
      }
      else if (filter->current->start == filter->current->end)
      {
        GST_DEBUG("Silence up to %" GST_TIME_FORMAT, GST_TIME_ARGS(filter->current->start));
      }
      else
      {
        GST_DEBUG("Beep: %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT,
                  GST_TIME_ARGS(filter->current->start),
                  GST_TIME_ARGS(filter->current->end));
      }
    }
    
    /* Figure out at which offset the beep starts */
    GstClockTimeDiff delta = (filter->current->start - start_time);
    int start_offset = offset + delta * SAMPLERATE / GST_SECOND;
    
    /* Figure out how many samples long the beep is */
    GstClockTimeDiff length = (filter->current->end - filter->current->start);
    int num_samples = length * SAMPLERATE / GST_SECOND;
    
    /* Check if the beep overlaps this buffer */
    if (start_offset < buflen)
    {
      /* Check if it started in previous buffer */
      if (start_offset < 0)
      {
        num_samples += start_offset;
        start_offset = 0;
      }
      
      /* Check if it extends beyond current buffer */
      if (start_offset + num_samples > buflen)
      {
        num_samples = buflen - start_offset;
      }
        
      /* Process the part of the beep that is inside current buffer */
      if (num_samples != 0)
      {
        GST_DEBUG("Processing beep at offset %d, length %d, phase %d",
                  start_offset, num_samples, filter->phase);
        add_beep(buf, start_offset, start_offset + num_samples, &filter->phase, num_channels);
      }
    }
    
    /* Update the offset for while loop condition */
    offset = start_offset + num_samples;
  }
  
  if (end_of_stream)
  {
    g_free(filter->current);
    filter->current = NULL;
    return GST_FLOW_EOS;
  }
  else
  {
    GST_DEBUG("Buffer done");
    return GST_FLOW_OK;
  }
}
