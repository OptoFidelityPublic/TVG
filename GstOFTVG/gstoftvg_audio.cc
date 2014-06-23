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
static GstStaticPadTemplate src_template =
GST_STATIC_PAD_TEMPLATE (
  "src",
  GST_PAD_SRC,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS (
    "audio/x-raw, "
    "format = (string) \"" GST_AUDIO_NE(S16) "\", "
    "rate = (int) " SAMPLERATE_STR ", "
    "channels = (int) 1;"
  )
);

/* Definition of the GObject subtype. */
static void gst_oftvg_audio_class_init(GstOFTVG_AudioClass* klass);
static void gst_oftvg_audio_init(GstOFTVG_Audio* filter);
G_DEFINE_TYPE (GstOFTVG_Audio, gst_oftvg_audio, GST_TYPE_PUSH_SRC);

/* Prototypes for the overridden methods */
static gboolean gst_oftvg_audio_start(GstBaseSrc* object);
GstFlowReturn gst_oftvg_audio_create(GstPushSrc *src, GstBuffer **buf);

/* Initializer for the class type */
static void gst_oftvg_audio_class_init(GstOFTVG_AudioClass* klass)
{
  /* GstPushSrc method overrides */
  {  
    GstPushSrcClass *gstpushsrc_class = (GstPushSrcClass *) klass;
    gstpushsrc_class->create = GST_DEBUG_FUNCPTR(gst_oftvg_audio_create);
  }
  
  /* GstBaseSrc method overrides */
  {
    GstBaseSrcClass *gstbasesrc_class = (GstBaseSrcClass *) klass;
    gstbasesrc_class->start = GST_DEBUG_FUNCPTR(gst_oftvg_audio_start);
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
  }
}

/* Initializer for class instances */
static void gst_oftvg_audio_init(GstOFTVG_Audio* filter)
{
  gst_base_src_set_format(GST_BASE_SRC(filter), GST_FORMAT_TIME);
  gst_base_src_set_live(GST_BASE_SRC(filter), FALSE);

  filter->queue = g_async_queue_new();
}

static gboolean gst_oftvg_audio_start(GstBaseSrc* object)
{
  GstOFTVG_Audio *filter = GST_OFTVG_AUDIO(object);
  filter->timestamp = 0;
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

static GstBuffer *create_buffer(int silent_samples, int beep_samples)
{
  int size = (silent_samples + beep_samples) * sizeof(gint16);
  gint16 *data = (gint16*)g_malloc0(size);
  
  for (int i = 0; i < beep_samples; i++)
  {
    float v = 0;
    v += sin(2 * M_PI * 547 * i / SAMPLERATE);
    v += sin(2 * M_PI * 1823 * i / SAMPLERATE);
    data[i + silent_samples] = (gint16)(16384 * v);
  }
  
  return gst_buffer_new_wrapped(data, size);;
}

/* Create a new buffer (called by the GstPushSrc base class) */
GstFlowReturn gst_oftvg_audio_create(GstPushSrc *src, GstBuffer **buf)
{
  GstOFTVG_Audio *filter = GST_OFTVG_AUDIO(src);
  beep_t *entry;
  
  GstClockTime min_time = GST_SECOND / SAMPLERATE;
  do {
    entry = (beep_t*) g_async_queue_pop(filter->queue);
  } while (entry->end <= filter->timestamp + min_time);
  
  if (entry->start == entry->end)
  {
    GST_DEBUG("Silence up to %" GST_TIME_FORMAT ", prev time %" GST_TIME_FORMAT,
            GST_TIME_ARGS(entry->start), GST_TIME_ARGS(filter->timestamp));
  }
  else
  {
    GST_DEBUG("Beep: %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT ", prev time %" GST_TIME_FORMAT,
              GST_TIME_ARGS(entry->start), GST_TIME_ARGS(entry->end),
              GST_TIME_ARGS(filter->timestamp));
  }
  
  /* G_MAXINT64 marks the end of stream. */
  if (entry->start >= G_MAXINT64)
  {
    return GST_FLOW_EOS;
  }
  
  /* Count the number of silent and beep samples needed */
  GstClockTime delta = (entry->start - filter->timestamp);
  GstClockTime length = (entry->end - entry->start);
  int silent_samples = delta * SAMPLERATE / GST_SECOND;
  int beep_samples = length * SAMPLERATE / GST_SECOND;
  
  if (silent_samples == 0 && beep_samples == 0)
  {
    GST_ERROR("Zero sample count");
    silent_samples = 1;
  }
  
  /* Create the buffer */
  *buf = create_buffer(silent_samples, beep_samples);
  
  /* Set timestamps for the buffer */
  GST_BUFFER_DURATION(*buf) = (silent_samples + beep_samples) * GST_SECOND / SAMPLERATE;
  GST_BUFFER_TIMESTAMP(*buf) = filter->timestamp;
  filter->timestamp += GST_BUFFER_DURATION(*buf);
  
  g_free(entry);
  
  return GST_FLOW_OK;
}
