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
 * SECTION:element-oftvg
 *
 * OFTVG (OptoFidelity Test Video Generator) is a filter that adds overlay
 * frame id and synchronization markings to each frame.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! oftvg ! autovideosink
 * ]|
 * </refsect2>
 */

/**
 * This file includes the interface between the filter implementation
 * and the Gstreamer plugin API. Also timing for benchmarking purposes
 * is implemented here.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <new>

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>

// for timemeasure.h
#define DO_TIMING 1

#include "gstoftvg.hh"
#include "gstoftvg_pixbuf.hh"
#include "timemeasure.h"

using namespace OFTVG;

#if defined(_MSC_VER)
#define Restrict __restrict
#else
#define Restrict restrict
#endif

GST_DEBUG_CATEGORY_STATIC (gst_oftvg_debug);
#define GST_CAT_DEFAULT gst_oftvg_debug

static const gchar* const DEFAULT_LAYOUT_LOCATION =
  "../layout/test-layout-1920x1080-c.bmp";

static const gchar* const CALIBRATION_OFF = "off";
static const gchar* const CALIBRATION_ONLY = "only";
static const gchar* const CALIBRATION_PREPEND = "prepend";
static const gchar* const DEFAULT_CALIBRATION = CALIBRATION_OFF;
static const bool DEFAULT_CALIBRATION_PREPEND = false;
static const int DEFAULT_REPEAT = 1;
static const int DEFAULT_NUM_BUFFERS = -1;

/// The timestamps where each calibration phase ends.
static const int numCalibrationTimestamps = 2;
static const GstClockTime calibrationTimestamps[numCalibrationTimestamps] =
  { 8 * GST_SECOND, 10 * GST_SECOND };

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_CALIBRATION,
  PROP_NUMBUF,
  PROP_LOCATION,
  PROP_REPEAT,
  PROP_SILENT,
};

/* the capabilities of the inputs and outputs.
 *
 */
static GstStaticPadTemplate sink_template =
GST_STATIC_PAD_TEMPLATE (
  "sink",
  GST_PAD_SINK,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS(
    GST_VIDEO_CAPS_YUV("AYUV") ";"
    GST_VIDEO_CAPS_YUV("Y444") ";"
    GST_VIDEO_CAPS_YUV("Y42B") ";"
    GST_VIDEO_CAPS_YUV("I420") ";"
    GST_VIDEO_CAPS_YUV("YV12") ";"
    GST_VIDEO_CAPS_YUV("Y41B") ";"
    GST_VIDEO_CAPS_YUV("YUY2") ";"
    GST_VIDEO_CAPS_YUV("YVYU") ";"
    GST_VIDEO_CAPS_YUV("UYVY") ";"
    GST_VIDEO_CAPS_RGB  ";"
    GST_VIDEO_CAPS_RGBx ";"
    GST_VIDEO_CAPS_xRGB ";"
    GST_VIDEO_CAPS_BGR  ";"
    GST_VIDEO_CAPS_BGRx ";"
    GST_VIDEO_CAPS_xBGR ";"
  )
);

static GstStaticPadTemplate src_template =
GST_STATIC_PAD_TEMPLATE (
  "src",
  GST_PAD_SRC,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS ("ANY")
);

/* debug category for fltering log messages
 *
 */
#define DEBUG_INIT(bla) \
  GST_DEBUG_CATEGORY_INIT (gst_oftvg_debug, "oftvg", 0, \
    "OptoFidelity Test Video Generator");

GST_BOILERPLATE_FULL (GstOFTVG, gst_oftvg, GstBaseTransform,
    GST_TYPE_BASE_TRANSFORM, DEBUG_INIT);

static void gst_oftvg_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_oftvg_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gint64 gst_oftvg_source_frame_number(GstOFTVG* const filter,
  const GstBuffer* const buf);

static gboolean gst_oftvg_event(GstBaseTransform* base, GstEvent *event);

static GstFlowReturn gst_oftvg_transform_ip (GstBaseTransform * base,
    GstBuffer * outbuf);

static gboolean gst_oftvg_set_caps(GstBaseTransform* btrans,
  GstCaps* incaps, GstCaps* outcaps);

static const GstOFTVGLayout&
  gst_oftvg_get_layout(const GstOFTVG* filter, const GstBuffer* buf);

static gboolean gst_oftvg_set_process_function(GstOFTVG* filter);

static gboolean gst_oftvg_start(GstBaseTransform* btrans);

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
oftvg_init (GstPlugin* oftvg)
{
  return gst_element_register (oftvg, "oftvg", GST_RANK_NONE, GST_TYPE_OFTVG);
}

/* GObject vmethod implementations */

static void
gst_oftvg_base_init (gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_set_details_simple (element_class,
    "OFTVG",
    "Filter/Editor/Video",
    "Overlays buffer timestamps on a video stream",
    "OptoFidelity <info@optofidelity.com>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_template));
}

/* initialize the oftvg's class */
static void
gst_oftvg_class_init (GstOFTVGClass* klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBaseTransformClass* btrans = GST_BASE_TRANSFORM_CLASS (klass);

  gobject_class->set_property = gst_oftvg_set_property;
  gobject_class->get_property = gst_oftvg_get_property;

  g_object_class_install_property(gobject_class, PROP_CALIBRATION,
    g_param_spec_string ("calibration", "Calibration",
      "(off|prepend|only). \"Only\" implies \"num-buffers=0\" and \"repeat=0\".",
      DEFAULT_CALIBRATION,
      (GParamFlags)(G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE)));

  g_object_class_install_property(gobject_class, PROP_LOCATION,
    g_param_spec_string ("location", "Location", "Layout bitmap file location",
      DEFAULT_LAYOUT_LOCATION,
      (GParamFlags)(G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE)));

  g_object_class_install_property(gobject_class, PROP_NUMBUF,
    g_param_spec_int ("num-buffers", "num-buffers", "Number of buffers to process.",
      -1, G_MAXINT, DEFAULT_NUM_BUFFERS,
      (GParamFlags)(G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE)));

  g_object_class_install_property(gobject_class, PROP_REPEAT,
    g_param_spec_int ("repeat", "Repeat", "Repeat n times.",
      1, G_MAXINT, DEFAULT_REPEAT,
      (GParamFlags)(G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE)));

  g_object_class_install_property(gobject_class, PROP_SILENT,
    g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, (GParamFlags)(G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE)));

  btrans->event = GST_DEBUG_FUNCPTR(gst_oftvg_event);
  btrans->transform_ip = GST_DEBUG_FUNCPTR(gst_oftvg_transform_ip);
  btrans->set_caps     = GST_DEBUG_FUNCPTR(gst_oftvg_set_caps);
  btrans->start = GST_DEBUG_FUNCPTR(gst_oftvg_start);
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_oftvg_init (GstOFTVG* filter, GstOFTVGClass* klass)
{
  /* unused parameter */ klass;

  // use placement new to construct the object in the memory already allocated.
  ::new (&(filter->oftvg)) OFTVG::Oftvg();
  filter->oftvg.setElement(filter->element);
  filter->oftvg.setLayoutLocation(DEFAULT_LAYOUT_LOCATION);
  filter->oftvg.setCalibrationPrepend(DEFAULT_CALIBRATION_PREPEND);
  filter->oftvg.setRepeat(DEFAULT_REPEAT);
  filter->oftvg.setNumBuffers(DEFAULT_NUM_BUFFERS);

  GST_DEBUG("GstOFTVG initialized.\n");
}


static gboolean gst_oftvg_start(GstBaseTransform* btrans)
{
  GstOFTVG* filter = (GstOFTVG*)btrans;
  filter->oftvg.reset();
  return true;
}

/*
Event handlers.
*/

static void
gst_oftvg_set_property (GObject * object, guint prop_id,
    const GValue* value, GParamSpec * pspec)
{
  GstOFTVG *filter = GST_OFTVG (object);

  switch (prop_id) {
    case PROP_CALIBRATION:
    {
      const gchar* str = g_value_get_string(value);
      if (g_strcasecmp(CALIBRATION_OFF, str) == 0)
      {
        filter->oftvg.setCalibrationPrepend(false);
      }
      else
      {
        filter->oftvg.setCalibrationPrepend(true);
        if (g_strcasecmp(CALIBRATION_ONLY, str) == 0)
        {
          filter->oftvg.setNumBuffers(0);
          filter->oftvg.setRepeat(0);
        }
      }
      break;
    }
    case PROP_LOCATION:
      filter->oftvg.setLayoutLocation(g_value_get_string(value));
      break;
    case PROP_NUMBUF:
      filter->oftvg.setNumBuffers(g_value_get_int(value));
      break;
    case PROP_REPEAT:
      filter->oftvg.setRepeat(g_value_get_int(value));
      break;
    case PROP_SILENT:
      filter->oftvg.setSilent(g_value_get_boolean(value)?true:false);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static void
gst_oftvg_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstOFTVG *filter = GST_OFTVG (object);

  switch (prop_id) {
    case PROP_CALIBRATION:
      if (filter->oftvg.getCalibrationPrepend())
      {
        g_value_set_string(value, CALIBRATION_PREPEND);
      }
      else
      {
        g_value_set_string(value, CALIBRATION_OFF);
      }
      break;
    case PROP_LOCATION:
      g_value_set_string(value, filter->oftvg.getLayoutLocation());
      break;
    case PROP_NUMBUF:
      g_value_set_int(value, filter->oftvg.getNumBuffers());
      break;
    case PROP_REPEAT:
      g_value_set_int(value, filter->oftvg.getRepeat());
      break;
    case PROP_SILENT:
      g_value_set_boolean(value, filter->oftvg.getSilent());
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* timing helpers */
static void gst_oftvg_process_ip_begin_timing(GstOFTVG* filter)
{
}

static void gst_oftvg_process_ip_end_timing(GstOFTVG* filter,
  timemeasure_t timer1)
{
  // NOTE: This function does not support multiple oftvg filters
  // to coexist. The timers and counters will be mixed.
  if (!filter->oftvg.getSilent())
  {
    static double total_oftvg_time = 0;
    static int timer_counter = 0;
    static timemeasure_t total_pipeline_timer = 0;
    total_oftvg_time += end_timing(timer1, "gst_oftvg_transform_ip");

    if (timer_counter == 0)
    {
      total_pipeline_timer = timer1;
    }
    timer_counter++;
    if (timer_counter % 240 == 0)
    {
      show_timing(total_oftvg_time / 240, "gst_oftvg_transform_ip");
      total_oftvg_time = 0;
    }
    if (timer_counter % 240 == 0)
    {
      double total_pipeline_time = end_timing(total_pipeline_timer,
        "total_pipeline");
      show_timing(total_pipeline_time / timer_counter,
        "total_pipeline");
    }
  }
}

/* GstBaseTransform vmethod implementations */

static gboolean gst_oftvg_event(GstBaseTransform* base, GstEvent *event)
{
  GstOFTVG* filter = GST_OFTVG(base);
  gboolean ret;

  switch (GST_EVENT_TYPE(event))
  {
    case GST_EVENT_FLUSH_START:
    case GST_EVENT_FLUSH_STOP:
    case GST_EVENT_NEWSEGMENT:
      // Block the events related to the repeat seek from propagating downstream.
      // We kind of split the pipeline in half: upstream seeks back to the start
      // of video while downstream keeps going.
      if (filter->oftvg.getRepeatCount() > 0)
      {
        // drop event
        ret = FALSE;
      }
      else if (GST_EVENT_TYPE(event) == GST_EVENT_NEWSEGMENT)
      {
        // Patch other NEWSEGMENT events so that the end time is indeterminate.
        // The default value is the length of the input video, but due to repeats
        // our output may be longer.
        event = gst_event_new_new_segment(false, 1.0, GST_FORMAT_TIME, 0, -1, 0);
        gst_pad_push_event (filter->element.srcpad, event);
        ret = FALSE;
      }
      break;
    case GST_EVENT_EOS:
      if (!filter->oftvg.atInputStreamEnd())
      {
        GST_ELEMENT_WARNING(filter, STREAM, FAILED,
          ("Stream ended prematurely."), (NULL));
      }
      // Pass on.
      ret = TRUE;
      break;
    default:
      ret = TRUE;
      break;
  }

  return ret;
}

/// This function does the actual processing. In place processing.
static GstFlowReturn
gst_oftvg_transform_ip(GstBaseTransform* base, GstBuffer *buf)
{
  timemeasure_t timer1 = begin_timing();

  GstOFTVG *filter = GST_OFTVG(base);

  GstFlowReturn ret = filter->oftvg.gst_oftvg_transform_ip(buf);

  if (GST_FLOW_IS_SUCCESS(ret))
  {
    gst_oftvg_process_ip_end_timing(filter, timer1);
  }

  return ret;
}


static gboolean gst_oftvg_set_caps(GstBaseTransform* object,
  GstCaps* incaps, GstCaps* outcaps)
{
  /* unused parameter */ outcaps;
  GstOFTVG *filter = GST_OFTVG(object);

  if (!filter->oftvg.videoFormatSetCaps(incaps))
  {
    GST_WARNING_OBJECT(filter, "Failed to set caps %"
      GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);
    return FALSE;
  }

  if (!filter->oftvg.initParams())
  {
    GST_WARNING_OBJECT(filter, "No processing function for this caps");
    return FALSE;
  }

  return TRUE;
}


#ifndef PACKAGE
#define PACKAGE "gstoftvg"
#endif

/* gstreamer looks for this structure to register oftvgs
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "oftvg",
    "OptoFidelity Test Video Generator",
    oftvg_init,
    "Compiled " __DATE__ " " __TIME__,
    "LGPL",
    "OptoFidelity",
    "http://optofidelity.com/"
)
