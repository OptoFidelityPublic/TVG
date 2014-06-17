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
 * gst-launch -v videotestsrc ! oftvg location=layout.bmp ! autovideosink
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
#include <string.h>
#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>

// for timemeasure.h
#define DO_TIMING 1

#include "gstoftvg.hh"
#include "gstoftvg_pixbuf.hh"
#include "timemeasure.h"

using namespace OFTVG;

GST_DEBUG_CATEGORY_STATIC (gst_oftvg_debug);
#define GST_CAT_DEFAULT gst_oftvg_debug

static const gchar* const DEFAULT_LAYOUT_LOCATION = "layout.bmp";

static const gchar* const CALIBRATION_OFF = "off";
static const gchar* const CALIBRATION_ONLY = "only";
static const gchar* const CALIBRATION_PREPEND = "prepend";
static const gchar* const CALIBRATION_BOTH = "both";
static const gchar* const DEFAULT_CALIBRATION = CALIBRATION_OFF;
static const bool DEFAULT_CALIBRATION_PREPEND = false;
static const int DEFAULT_REPEAT = 1;
static const int DEFAULT_NUM_BUFFERS = -1;

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
  PROP_SILENT,
  PROP_SEQUENCE,
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

static void gst_oftvg_init (GstOFTVG* filter);
static void gst_oftvg_class_init (GstOFTVGClass* klass);

G_DEFINE_TYPE (GstOFTVG, gst_oftvg, GST_TYPE_BASE_TRANSFORM);

static void gst_oftvg_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_oftvg_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_oftvg_transform_ip (GstBaseTransform * base,
    GstBuffer * outbuf);

static gboolean gst_oftvg_set_caps(GstBaseTransform* btrans,
  GstCaps* incaps, GstCaps* outcaps);

static gboolean gst_oftvg_start(GstBaseTransform* btrans);

/* GObject vmethod implementations */

/* initialize the oftvg's class */
static void
gst_oftvg_class_init (GstOFTVGClass* klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBaseTransformClass* btrans = GST_BASE_TRANSFORM_CLASS (klass);

  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  gst_element_class_set_metadata (element_class,
    "OptoFidelity test video generator",
    "Filter/Editor/Video",
    "Overlays buffer timestamps on a video stream",
    "OptoFidelity <info@optofidelity.com>");
	
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_template));
	
  gobject_class->set_property = gst_oftvg_set_property;
  gobject_class->get_property = gst_oftvg_get_property;

  g_object_class_install_property(gobject_class, PROP_CALIBRATION,
    g_param_spec_string ("calibration", "Calibration",
      "(off|prepend|only|both). \"Only\" implies \"num-buffers=0\" and \"repeat=0\".",
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

  g_object_class_install_property(gobject_class, PROP_SILENT,
    g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, (GParamFlags)(G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE)));

  g_object_class_install_property(gobject_class, PROP_SEQUENCE,
    g_param_spec_string ("sequence", "Custom color sequence", "Text file with custom color sequence data.",
      "", (GParamFlags)(G_PARAM_READWRITE)));

  btrans->transform_ip = GST_DEBUG_FUNCPTR(gst_oftvg_transform_ip);
  btrans->set_caps     = GST_DEBUG_FUNCPTR(gst_oftvg_set_caps);
  btrans->start = GST_DEBUG_FUNCPTR(gst_oftvg_start);
  
  GST_DEBUG_CATEGORY_INIT(gst_oftvg_debug, "oftvg", 0, "");
}

/* initialize the new element
 * initialize instance structure
 */
static void gst_oftvg_init (GstOFTVG* filter)
{
  // use placement new to construct the object in the memory already allocated.
  ::new (&(filter->oftvg)) OFTVG::Oftvg();
  filter->oftvg.setElement(filter->element);
  filter->oftvg.setLayoutLocation(DEFAULT_LAYOUT_LOCATION);
  filter->oftvg.setCalibrationPrepend(DEFAULT_CALIBRATION_PREPEND);
  filter->oftvg.setNumBuffers(DEFAULT_NUM_BUFFERS);
  filter->oftvg.setCustomSequence("");

  GST_DEBUG("GstOFTVG initialized.");
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
      if (g_ascii_strncasecmp(CALIBRATION_OFF, str, strlen(CALIBRATION_OFF) + 1) == 0)
      {
        filter->oftvg.setCalibrationPrepend(false);
        filter->oftvg.setCalibrationAppend(false);
      }
      else
      {
        filter->oftvg.setCalibrationPrepend(true);
        if (g_ascii_strncasecmp(CALIBRATION_BOTH, str, strlen(CALIBRATION_BOTH) + 1) == 0)
        {
          filter->oftvg.setCalibrationAppend(true);
        }
        else
        {
          filter->oftvg.setCalibrationAppend(false);
        }
        if (g_ascii_strncasecmp(CALIBRATION_ONLY, str, strlen(CALIBRATION_ONLY)) == 0)
        {
          filter->oftvg.setNumBuffers(0);
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
    case PROP_SILENT:
      filter->oftvg.setSilent(g_value_get_boolean(value)?true:false);
      break;
    case PROP_SEQUENCE:
      filter->oftvg.setCustomSequence(g_value_get_string(value));
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
    case PROP_SILENT:
      g_value_set_boolean(value, filter->oftvg.getSilent());
      break;
    case PROP_SEQUENCE:
      g_value_set_string(value, filter->oftvg.getCustomSequence());
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* timing helpers */
static void gst_oftvg_process_ip_end_timing(GstOFTVG* filter,
  timemeasure_t timer1)
{
  // NOTE: This function does not support multiple oftvg filters
  // to coexist. The timers and counters will be mixed.
  if (!filter->oftvg.getSilent())
  {
    static double total_oftvg_time = 0;
    static int timer_counter = 0;
    static timemeasure_t total_pipeline_timer;
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

/// This function does the actual processing. In place processing.
static GstFlowReturn
gst_oftvg_transform_ip(GstBaseTransform* base, GstBuffer *buf)
{
  timemeasure_t timer1 = begin_timing();

  GstOFTVG *filter = GST_OFTVG(base);

  GstFlowReturn ret = filter->oftvg.gst_oftvg_transform_ip(buf);

  if (ret == GST_FLOW_OK)
  {
    gst_oftvg_process_ip_end_timing(filter, timer1);
  }

  return ret;
}


static gboolean gst_oftvg_set_caps(GstBaseTransform* object,
  GstCaps* incaps, GstCaps* outcaps)
{
  (void)outcaps;
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
