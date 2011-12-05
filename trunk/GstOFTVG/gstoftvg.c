/*
 * Test Video Generator
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
 * OFTVG is an overlay frame id marker.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! oftvg ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>

#include "gstoftvg.h"

GST_DEBUG_CATEGORY_STATIC (gst_oftvg_debug);
#define GST_CAT_DEFAULT gst_oftvg_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SILENT,
};

/* the capabilities of the inputs and outputs.
 *
 * FIXME:describe the real formats here.
 */
static GstStaticPadTemplate sink_template =
GST_STATIC_PAD_TEMPLATE (
  "sink",
  GST_PAD_SINK,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS(
    GST_VIDEO_CAPS_YUV("I420") ";" GST_VIDEO_CAPS_YUV("YV12")
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
 * FIXME:exchange the string 'Template oftvg' with your description
 */
#define DEBUG_INIT(bla) \
  GST_DEBUG_CATEGORY_INIT (gst_oftvg_debug, "oftvg", 0, "Test Video Generator");

GST_BOILERPLATE_FULL (GstOFTVG, gst_oftvg, GstBaseTransform,
    GST_TYPE_BASE_TRANSFORM, DEBUG_INIT);

static void gst_oftvg_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_oftvg_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_oftvg_transform_ip (GstBaseTransform * base,
    GstBuffer * outbuf);

gboolean gst_oftvg_set_caps(GstBaseTransform* btrans, GstCaps* incaps, GstCaps* outcaps);

/* GObject vmethod implementations */

static void
gst_oftvg_base_init (gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_set_details_simple (element_class,
    "OFTVG",
    "Generic/Filter",
    "Test Video Generator",
    "OptoFidelity <info@optofidelity.com>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_template));
}

/* initialize the oftvg's class */
static void
gst_oftvg_class_init (GstOFTVGClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBaseTransformClass* btrans = GST_BASE_TRANSFORM_CLASS (klass);

  gobject_class->set_property = gst_oftvg_set_property;
  gobject_class->get_property = gst_oftvg_get_property;

  g_object_class_install_property (gobject_class, PROP_SILENT,
    g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, (GParamFlags)(G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE)));

  btrans->transform_ip = GST_DEBUG_FUNCPTR(gst_oftvg_transform_ip);
  btrans->set_caps     = GST_DEBUG_FUNCPTR(gst_oftvg_set_caps);
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_oftvg_init (GstOFTVG *filter, GstOFTVGClass * klass)
{
  filter->silent = FALSE;
}

static void
gst_oftvg_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOFTVG *filter = GST_OFTVG (object);

  switch (prop_id) {
    case PROP_SILENT:
      filter->silent = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_oftvg_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstOFTVG *filter = GST_OFTVG (object);

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, filter->silent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstBaseTransform vmethod implementations */

/* this function does the actual processing. In place processing.
 */
static GstFlowReturn
gst_oftvg_transform_ip(GstBaseTransform * base, GstBuffer *buf)
{
  GstOFTVG *filter = GST_OFTVG (base);
  gint width;
  gint height;

  if (filter->silent == FALSE)
    g_print ("I'm plugged, therefore I'm in.\n");
  
  /* FIXME: do something interesting here.  This simply copies the source
   * to the destination. */
  
  width = filter->width;
  height = filter->height;
  filter->process_inplace(GST_BUFFER_DATA(buf), filter);

  return GST_FLOW_OK;
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
oftvg_init (GstPlugin * oftvg)
{
  return gst_element_register (oftvg, "oftvg", GST_RANK_NONE, GST_TYPE_OFTVG);
}

static void gst_oftvg_init_params(GstOFTVG* filter)
{
  filter->bit_on_color[0] = 255;
  filter->bit_on_color[1] = 128;
  filter->bit_on_color[2] = 128;
  filter->bit_on_color[3] = 0;

  filter->bit_off_color[0] = 0;
  filter->bit_off_color[1] = 128;
  filter->bit_off_color[2] = 128;
  filter->bit_off_color[3] = 0;
}

gboolean gst_oftvg_set_caps(GstBaseTransform* object, GstCaps* incaps, GstCaps* outcaps)
{
    GstOFTVG *filter = GST_OFTVG(object);

    if (!gst_video_format_parse_caps(incaps, &filter->in_format, &filter->width, &filter->height)
      ||
        !gst_video_format_parse_caps(incaps, &filter->out_format, &filter->width, &filter->height))
    {
      GST_WARNING_OBJECT(filter, "Failed to parse caps %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT,
        incaps, outcaps);
      return FALSE;
    }

    if (!gst_oftvg_set_process_function(filter))
    {
      GST_WARNING_OBJECT(filter, "No processing function for this caps");
      return FALSE;
    }

    gst_oftvg_init_params(filter);

    return TRUE;
}

void gst_oftvg_process_planar_yuv(guint8 *buf, GstOFTVG* filter)
{
  const guint8* bit_on_color  = filter->bit_on_color;
  const guint8* bit_off_color = filter->bit_off_color;
  gint width = filter->width;
  gint height = filter->height;
  int y, x;
  guint8* bufY;
  guint8* bufU;
  guint8* bufV;
  gint y_stride;
  gint uv_stride;
  gint v_subs;
  gint h_subs;

  y_stride  = gst_video_format_get_row_stride(filter->in_format, 0, width);
  uv_stride = gst_video_format_get_row_stride(filter->in_format, 1, width);
  g_assert(uv_stride == gst_video_format_get_row_stride(filter->in_format, 2, width));

  bufY = buf;
  bufU =
    buf + gst_video_format_get_component_offset(filter->in_format, 1, width,
    height);
  bufV =
    buf + gst_video_format_get_component_offset(filter->in_format, 2, width,
    height);

  switch (filter->in_format)
  {
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_YV12:
      v_subs = h_subs = 1; // lg2(2)
      break;
    default:
      g_assert_not_reached();
      break;
  }

  for (y = 0; y < height; y++)
  {
    for (x = 0; x < width; x++)
    {
      int boxw = 32;
      int boxh = 32;

      // Testing black boxen
      if (((y % 256) >= 128 && (y % 256) < 128 + boxh) && (x >= 128 && x < 128 + boxw) || (x == 200 && y == 200))
      {
        bufY[x]         = bit_on_color[0];
        bufU[x>>h_subs] = bit_on_color[1];
        bufV[x>>h_subs] = bit_on_color[2];
      }
      // Testing white boxen
      else if (((y % 256) >= 128 && (y % 256) < 128 + boxh) && (x >= 256 && x < 256 + boxw) || (x == 300 && y == 300))
      {
        bufY[x]         = bit_off_color[0];
        bufU[x>>h_subs] = bit_off_color[1];
        bufV[x>>h_subs] = bit_off_color[2];
      }
      else
      {
        // Pass through unchanged.
      }
    }
    bufY += y_stride;
    if ((y) & (1 << h_subs) != 0)
    {
      bufU += uv_stride;
      bufV += uv_stride;
    }
  }
}

static gboolean gst_oftvg_set_process_function(GstOFTVG* filter)
{
  filter->process_inplace = gst_oftvg_process_planar_yuv;
  return filter->process_inplace != NULL;
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
