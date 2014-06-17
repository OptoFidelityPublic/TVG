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

/* Debug category to use */
GST_DEBUG_CATEGORY_EXTERN(gst_oftvg_debug);
#define GST_CAT_DEFAULT gst_oftvg_debug

/* Identifier numbers for signals emitted by this element */
enum
{
  LAST_SIGNAL
};

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
static GstFlowReturn gst_oftvg_video_transform_ip (GstBaseTransform * base, GstBuffer * outbuf);
static gboolean gst_oftvg_video_set_caps(GstBaseTransform* btrans, GstCaps* incaps, GstCaps* outcaps);
static gboolean gst_oftvg_video_start(GstBaseTransform* btrans);

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
#define PROP_STR(up,name,desc,def)  case PROP_ ## up: filter->name = g_value_dup_string(value); break;
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
  return true;
}

/* Store information about the pin caps when they become available */
static gboolean gst_oftvg_video_set_caps(GstBaseTransform* object, GstCaps* incaps, GstCaps* outcaps)
{
  GstOFTVG_Video *filter = GST_OFTVG_VIDEO(object);
  (void)outcaps;

  gst_video_info_init(&filter->in_info);
  if (!gst_caps_is_fixed(incaps) || !gst_video_info_from_caps(&filter->in_info, incaps))
  {
    GST_WARNING_OBJECT(filter, "Could not get video info");
    return FALSE;
  }

  filter->in_format = GST_VIDEO_INFO_FORMAT(&filter->in_info);
  filter->in_format_info = gst_video_format_get_info(filter->in_format);
  filter->width = GST_VIDEO_INFO_WIDTH(&filter->in_info);
  filter->height = GST_VIDEO_INFO_HEIGHT(&filter->in_info);
  return TRUE;
}

/* Entry point for processing video frames */
static GstFlowReturn gst_oftvg_video_transform_ip(GstBaseTransform* object, GstBuffer *buf)
{
  return GST_FLOW_OK;
}




