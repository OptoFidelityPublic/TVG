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
 * SECTION:element-oftvg_video
 *
 * OFTVG (OptoFidelity Test Video Generator) is a filter that adds overlay
 * frame id and synchronization markings to each frame. This element processes
 * the video stream.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v videotestsrc ! oftvg location=layout.bmp ! autovideosink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include "gstoftvg_video.hh"
#include "gstoftvg_video_process.hh"

/* Debug category to use */
GST_DEBUG_CATEGORY_EXTERN(gst_oftvg_debug);
#define GST_CAT_DEFAULT gst_oftvg_debug

/* Identifier numbers for signals emitted by this element */
enum
{
  SIGNAL_LIPSYNC_GENERATED,
  SIGNAL_VIDEO_PROCESSED_UPTO,
  SIGNAL_VIDEO_END_OF_STREAM,
  LAST_SIGNAL
};
static guint gstoftvg_video_signals[LAST_SIGNAL] = { 0 };

/* Identifier numbers for properties */
enum
{
  PROP_0,
#define PROP_STR(up,name,desc,def) PROP_ ## up,
#define PROP_INT(up,name,desc,def) PROP_ ## up,
#define PROP_BOOL(up,name,desc,def) PROP_ ## up,
GSTOFTVG_VIDEO_PROPERTIES
#undef PROP_STR
#undef PROP_INT
#undef PROP_BOOL
};

/* Templates for the sink and source pins.
 *
 * The sink pin supports any most raw video formats.
 * The source pin will have the same format as the sink at runtime.
 */
static GstStaticPadTemplate sink_template =
GST_STATIC_PAD_TEMPLATE (
  "sink",
  GST_PAD_SINK,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS(
    GST_VIDEO_CAPS_MAKE("AYUV") ";"
    GST_VIDEO_CAPS_MAKE("Y444") ";"
    GST_VIDEO_CAPS_MAKE("Y42B") ";"
    GST_VIDEO_CAPS_MAKE("I420") ";"
    GST_VIDEO_CAPS_MAKE("YV12") ";"
    GST_VIDEO_CAPS_MAKE("Y41B") ";"
    GST_VIDEO_CAPS_MAKE("YUY2") ";"
    GST_VIDEO_CAPS_MAKE("YVYU") ";"
    GST_VIDEO_CAPS_MAKE("UYVY") ";"
    GST_VIDEO_CAPS_MAKE("RGB")  ";"
    GST_VIDEO_CAPS_MAKE("RGBx") ";"
    GST_VIDEO_CAPS_MAKE("xRGB") ";"
    GST_VIDEO_CAPS_MAKE("BGR")  ";"
    GST_VIDEO_CAPS_MAKE("BGRx") ";"
    GST_VIDEO_CAPS_MAKE("xBGR") ";"
  )
);

static GstStaticPadTemplate src_template =
GST_STATIC_PAD_TEMPLATE (
  "src",
  GST_PAD_SRC,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS ("ANY")
);

/* Definition of the GObject subtype. We inherit from GstBaseTransform, which
 * does most of the events and caps negotiation for us. */
static void gst_oftvg_video_class_init(GstOFTVG_VideoClass* klass);
static void gst_oftvg_video_init(GstOFTVG_Video* filter);
G_DEFINE_TYPE (GstOFTVG_Video, gst_oftvg_video, GST_TYPE_BASE_TRANSFORM);

/* Prototypes for the overridden methods */
static void gst_oftvg_video_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_oftvg_video_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static gboolean gst_oftvg_video_start(GstBaseTransform* btrans);
static gboolean gst_oftvg_video_stop(GstBaseTransform* btrans);
static gboolean gst_oftvg_video_sink_event(GstBaseTransform *object, GstEvent *event);
static gboolean gst_oftvg_video_set_caps(GstBaseTransform* btrans, GstCaps* incaps, GstCaps* outcaps);
static GstFlowReturn gst_oftvg_video_transform_ip (GstBaseTransform * base, GstBuffer * outbuf);

/* Initializer for the class type */
static void gst_oftvg_video_class_init (GstOFTVG_VideoClass * klass)
{
  /* GObject method overrides */
  {  
    GObjectClass *gobject_class = (GObjectClass *) klass;
    
    gobject_class->set_property = gst_oftvg_video_set_property;
    gobject_class->get_property = gst_oftvg_video_get_property;
  }
  
  /* GstBaseTransform method overrides */
  {
    GstBaseTransformClass* btrans = GST_BASE_TRANSFORM_CLASS(klass);
    
    btrans->transform_ip = GST_DEBUG_FUNCPTR(gst_oftvg_video_transform_ip);
    btrans->set_caps     = GST_DEBUG_FUNCPTR(gst_oftvg_video_set_caps);
    btrans->start        = GST_DEBUG_FUNCPTR(gst_oftvg_video_start);
    btrans->stop         = GST_DEBUG_FUNCPTR(gst_oftvg_video_stop);
    btrans->sink_event   = GST_DEBUG_FUNCPTR(gst_oftvg_video_sink_event);
  }
  
  /* Element metadata */
  {
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    
    gst_element_class_set_metadata (element_class,
      "OptoFidelity test video generator",
      "Filter/Editor/Video",
      "Adds color markers to video stream",
      "OptoFidelity <info@optofidelity.com>");
    
    gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));
    gst_element_class_add_pad_template (element_class,
	gst_static_pad_template_get (&sink_template));
  }
  
  /* Signals */
  {
    gstoftvg_video_signals[SIGNAL_LIPSYNC_GENERATED] = g_signal_new (
      "lipsync-generated", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET(GstOFTVG_VideoClass, signal_lipsync_generated), NULL, NULL, NULL, G_TYPE_NONE,
      2, G_TYPE_UINT64, G_TYPE_UINT64);
    
    gstoftvg_video_signals[SIGNAL_VIDEO_PROCESSED_UPTO] = g_signal_new (
      "video-processed-upto", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET(GstOFTVG_VideoClass, signal_video_processed_upto), NULL, NULL, NULL, G_TYPE_NONE,
      1, G_TYPE_UINT64);
    
    gstoftvg_video_signals[SIGNAL_VIDEO_END_OF_STREAM] = g_signal_new (
      "video-end-of-stream", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET(GstOFTVG_VideoClass, signal_video_processed_upto), NULL, NULL, NULL, G_TYPE_NONE, 0);
  }
  
  /* Element properties (generated from X-macros in gstoftvg_video.hh) */
  {
    GObjectClass *gobject_class = (GObjectClass *) klass;
    
#define PROP_STR(up,name,desc,def) \
  g_object_class_install_property(gobject_class, PROP_ ## up, \
    g_param_spec_string(#name, #name, desc, def, (GParamFlags)(G_PARAM_READWRITE)) \
  );
#define PROP_INT(up,name,desc,def) \
  g_object_class_install_property(gobject_class, PROP_ ## up, \
    g_param_spec_int(#name, #name, desc, G_MININT, G_MAXINT, def, (GParamFlags)(G_PARAM_READWRITE)) \
  );
#define PROP_BOOL(up,name,desc,def) \
  g_object_class_install_property(gobject_class, PROP_ ## up, \
    g_param_spec_boolean(#name, #name, desc, def, (GParamFlags)(G_PARAM_READWRITE)) \
  );
GSTOFTVG_VIDEO_PROPERTIES
#undef PROP_STR
#undef PROP_INT
#undef PROP_BOOL
  }
}

/* Initializer for class instances */
static void gst_oftvg_video_init (GstOFTVG_Video *filter)
{
  /* Set all properties to default values */
#define PROP_STR(up,name,desc,def) filter->name = g_strdup(def);
#define PROP_INT(up,name,desc,def) filter->name = def;
#define PROP_BOOL(up,name,desc,def) filter->name = def;
GSTOFTVG_VIDEO_PROPERTIES
#undef PROP_STR
#undef PROP_INT
#undef PROP_BOOL
}

/* Property setting */
static void gst_oftvg_video_set_property (GObject *object, guint prop_id,
                                          const GValue *value, GParamSpec *pspec)
{
  GstOFTVG_Video *filter = GST_OFTVG_VIDEO(object);

  switch (prop_id) {
#define PROP_STR(up,name,desc,def)  case PROP_ ## up: g_free(filter->name); filter->name = g_value_dup_string(value); break;
#define PROP_INT(up,name,desc,def)  case PROP_ ## up: filter->name = g_value_get_int(value); break;
#define PROP_BOOL(up,name,desc,def) case PROP_ ## up: filter->name = g_value_get_boolean(value); break;
GSTOFTVG_VIDEO_PROPERTIES
#undef PROP_STR
#undef PROP_INT
#undef PROP_BOOL

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* Property getting */
static void gst_oftvg_video_get_property (GObject *object, guint prop_id,
                                          GValue *value, GParamSpec *pspec)
{
  GstOFTVG_Video *filter = GST_OFTVG_VIDEO(object);

  switch (prop_id) {
#define PROP_STR(up,name,desc,def)  case PROP_ ## up: g_value_set_string(value, filter->name); break;
#define PROP_INT(up,name,desc,def)  case PROP_ ## up: g_value_set_int(value, filter->name); break;
#define PROP_BOOL(up,name,desc,def) case PROP_ ## up: g_value_set_boolean(value, filter->name); break;
GSTOFTVG_VIDEO_PROPERTIES
#undef PROP_STR
#undef PROP_INT
#undef PROP_BOOL

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* Called when the pipeline is starting up */
static gboolean gst_oftvg_video_start(GstBaseTransform* object)
{
  GstOFTVG_Video *filter = GST_OFTVG_VIDEO(object);
  filter->frame_counter = 0;
  filter->last_state_change = 0;
  filter->end_of_video = G_MAXINT64;
  filter->progress_timestamp = 0;
  filter->lipsync_timestamp = 0;
  filter->process = new OFTVG_Video_Process();
  
  if (g_strcmp0(filter->calibration, "off") == 0)
  {
    filter->state = STATE_VIDEO;
  }
  else
  {
    filter->state = STATE_PRECALIBRATION_WHITE;
  }
  
  return true;
}

/* Called when the pipeline is stopping */
static gboolean gst_oftvg_video_stop(GstBaseTransform* object)
{
  GstOFTVG_Video *filter = GST_OFTVG_VIDEO(object);
  delete filter->process;
  filter->process = NULL;
  
  return true;
}

/* Store information about the pin caps when they become available */
static gboolean gst_oftvg_video_set_caps(GstBaseTransform* object, GstCaps* incaps, GstCaps* outcaps)
{
  GstOFTVG_Video *filter = GST_OFTVG_VIDEO(object);
  (void)outcaps; /* unused */
  
  if (!filter->process->init_caps(incaps))
  {
    GST_ELEMENT_ERROR(filter, STREAM, FORMAT, ("Failed to apply caps"), (NULL));
    return false;
  }
  
  if (!filter->process->init_custom_sequence(filter->sequence))
  {
    GST_ELEMENT_ERROR(filter, RESOURCE, NOT_FOUND,
                      ("Failed to load custom sequence %s", filter->sequence), (NULL));
    return false;
  }
  
  if (!filter->process->init_layout(filter->location))
  {
    GST_ELEMENT_ERROR(filter, RESOURCE, NOT_FOUND,
                      ("Failed to load layout %s", filter->location), (NULL));
    return false;
  }
  
  return true;
}

/* Events on the sink pin */
static gboolean gst_oftvg_video_sink_event(GstBaseTransform *object, GstEvent *event)
{
  GstOFTVG_Video *filter = GST_OFTVG_VIDEO(object);
  
  if (GST_EVENT_TYPE(event) == GST_EVENT_SEGMENT)
  {
    /* Take note of the end time of the video, if known */
    const GstSegment *segment;
    gst_event_parse_segment(event, &segment);
    
    if (segment->format == GST_FORMAT_TIME)
    {
      if (segment->stop > 0)
      {
        filter->end_of_video = segment->stop;
      }
    }
  }
  else if (GST_EVENT_TYPE(event) == GST_EVENT_EOS)
  {
    /* If post-calibration was requested, make sure that it was done. */
    if (filter->state != STATE_END && g_strcmp0(filter->calibration, "both") == 0)
    {
      GST_ELEMENT_WARNING(filter, STREAM, FAILED,
                          ("Stream ended unexpectedly, is num_buffers too large?"
                           " (num_buffers = %d, stream contains %d frames)",
                           filter->num_buffers, filter->frame_counter), (NULL));
    }
    
    g_signal_emit(filter, gstoftvg_video_signals[SIGNAL_VIDEO_END_OF_STREAM], 0);
  }
  
  return gst_pad_push_event (filter->element.srcpad, event);
}

/* Process a single video frame in-place */
static GstFlowReturn gst_oftvg_video_transform_ip(GstBaseTransform* object, GstBuffer *buf)
{
  GstOFTVG_Video *filter = GST_OFTVG_VIDEO(object);
  GstClockTime buffer_end_time = GST_BUFFER_PTS(buf) + GST_BUFFER_DURATION(buf);
  state_t prev_state = filter->state;
  
  GST_DEBUG("Video buffer: %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT "\n",
              GST_TIME_ARGS(GST_BUFFER_PTS(buf)),
              GST_TIME_ARGS(GST_BUFFER_PTS(buf) + GST_BUFFER_DURATION(buf)));
  
  if (!filter->silent && filter->state == STATE_VIDEO)
  {
    /* Show progress once a second */
    if (buffer_end_time / GST_SECOND - filter->progress_timestamp / GST_SECOND != 0)
    {
      float progress = 0;
      if (filter->num_buffers > 0)
      {
        progress = 100.0f * filter->frame_counter / filter->num_buffers;
      }
      else
      {
        progress = 100.0f * buffer_end_time / filter->end_of_video;
      }
      
      filter->progress_timestamp = buffer_end_time;
      g_print("Progress: %4.1f%% (%d seconds) complete\n", progress,
              (int)((buffer_end_time - filter->last_state_change) / GST_SECOND));
    }
  }
  
  /* State machine for the calibration/video parts */
  if (filter->state == STATE_PRECALIBRATION_WHITE)
  {
    filter->process->process_calibration_white(buf);
    
    if (buffer_end_time >= 4 * GST_SECOND)
    {
      filter->state = STATE_PRECALIBRATION_MARKS;
    }
  }
  else if (filter->state == STATE_PRECALIBRATION_MARKS)
  {
    filter->process->process_calibration_marks(buf);
    
    if (buffer_end_time >= 5 * GST_SECOND)
    {
      if (g_strcmp0(filter->calibration, "only") == 0)
      {
        GST_DEBUG("Calibration=only is done");
        filter->state = STATE_END;
      }
      else
      {
        GST_DEBUG("Precalibration is done");
        filter->state = STATE_VIDEO;
      }
    }
  }
  else if (filter->state == STATE_VIDEO)
  {
    OFTVG::FrameFlags flags = OFTVG::FRAMEFLAGS_NONE;
    
    /* Generate lipsync frames at defined intervals */
    if (filter->lipsync > 0
        && (filter->lipsync_timestamp == 0
            || buffer_end_time >= filter->lipsync_timestamp + GST_MSECOND * filter->lipsync)
       )
    {
      GST_DEBUG("Generating lipsync at %" GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_PTS(buf)));
      flags = OFTVG::FRAMEFLAGS_LIPSYNC;
      filter->lipsync_timestamp = buffer_end_time;
      
      g_signal_emit(filter, gstoftvg_video_signals[SIGNAL_LIPSYNC_GENERATED], 0,
                    GST_BUFFER_PTS(buf), buffer_end_time);
    }
    
    filter->process->process_frame(buf, filter->frame_counter, flags);
    filter->frame_counter++;
    
    if (filter->num_buffers > 0)
    {
      /* Easy case: a fixed number of buffers */
      if (filter->frame_counter >= filter->num_buffers)
      {
        if (g_strcmp0(filter->calibration, "both") == 0)
        {
          GST_DEBUG("Given number of frames processed, going into postcalibration");
          filter->state = STATE_POSTCALIBRATION;
        }
        else
        {
          GST_DEBUG("Given number of frames processed, video ends");
          filter->state = STATE_END;
        }
      }
    }
    else
    {
      /* Otherwise try to stop earlier to leave enough time for postcalibration */
      if (buffer_end_time + 6 * GST_SECOND >= filter->end_of_video)
      {
        if (g_strcmp0(filter->calibration, "both") == 0)
        {
          GST_DEBUG("Close to end of video, going into postcalibration");
          filter->state = STATE_POSTCALIBRATION;
        }
      }
    }
  }
  else if (filter->state == STATE_POSTCALIBRATION)
  {
    filter->process->process_calibration_white(buf);
    
    if (buffer_end_time - filter->last_state_change >= 5 * GST_SECOND)
    {
      filter->state = STATE_END;
    }
  }
  else if (filter->state == STATE_END)
  {
    GST_DEBUG("End of video");
    g_signal_emit(filter, gstoftvg_video_signals[SIGNAL_VIDEO_END_OF_STREAM], 0);
    
    /* Note that the current buffer will not be passed forward when we return EOS */
    return GST_FLOW_EOS;
  }
  
  if (filter->state != prev_state)
  {
    GST_DEBUG("Changing to state %d from state %d", filter->state, prev_state);
    filter->last_state_change = buffer_end_time;
  }
  
  /* Report the timestamp of the frame that we just processed. */
  g_signal_emit(filter, gstoftvg_video_signals[SIGNAL_VIDEO_PROCESSED_UPTO], 0, buffer_end_time);
  
  return GST_FLOW_OK;
}




