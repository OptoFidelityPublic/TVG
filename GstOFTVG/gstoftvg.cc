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

#include "gstoftvg.hh"

/* Debug category to use */
GST_DEBUG_CATEGORY_EXTERN(gst_oftvg_debug);
#define GST_CAT_DEFAULT gst_oftvg_debug

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

/* Definition of the GObject subtype. We inherit from GstBin. */
static void gst_oftvg_class_init (GstOFTVGClass* klass);
static void gst_oftvg_init (GstOFTVG* filter);
G_DEFINE_TYPE (GstOFTVG, gst_oftvg, GST_TYPE_BIN);

/* Prototypes for the overridden methods */
static void gst_oftvg_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_oftvg_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);

/* Callbacks for passing lipsync data between video and audio elements */
static void lipsync_generated_cb(GstElement *video_element, GstClockTime start,
                                 GstClockTime end, GstOFTVG *filter);
static void video_processed_upto_cb(GstElement *video_element, GstClockTime start,
                                    GstOFTVG *filter);
static void video_end_of_stream_cb(GstElement *video_element, GstOFTVG *filter);

/* Initializer for the class type */
static void gst_oftvg_class_init (GstOFTVGClass* klass)
{
  /* GObject method overrides */
  {  
    GObjectClass *gobject_class = (GObjectClass *) klass;
    
    gobject_class->set_property = gst_oftvg_set_property;
    gobject_class->get_property = gst_oftvg_get_property;
  }
  
  /* Element metadata */
  {
    GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
    
    gst_element_class_set_metadata (element_class,
      "OptoFidelity test video generator",
      "Filter/Editor/Video",
      "Overlays buffer timestamps on a video stream",
      "OptoFidelity <info@optofidelity.com>");
  }
  
  /* Pass through the pads from the video element */
  {
    GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
    GstElementClass *video_class = GST_ELEMENT_CLASS(g_type_class_ref(GST_TYPE_OFTVG_VIDEO));
    
    gst_element_class_add_pad_template(element_class,
      gst_element_class_get_pad_template(video_class, "sink"));
    gst_element_class_add_pad_template(element_class,
      gst_element_class_get_pad_template(video_class, "src"));
    
    g_type_class_unref(video_class);
  }
  
  /* Pass through the pads from the audio element */
  {
    GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
    GstElementClass *audio_class = GST_ELEMENT_CLASS(g_type_class_ref(GST_TYPE_OFTVG_AUDIO));
    
    GstPadTemplate* pad = gst_element_class_get_pad_template(audio_class, "src");
    gst_element_class_add_pad_template(element_class,
      gst_pad_template_new("asrc", pad->direction, pad->presence, pad->caps)
    );
    
    g_type_class_unref(audio_class);
  }
  
  /* Pass through the properties from the video element */
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
static void gst_oftvg_init (GstOFTVG* filter)
{
  GstPad* pad;
  
  /* Create child elements */
  filter->video_element = GST_OFTVG_VIDEO(gst_element_factory_make("oftvg_video", "video"));
  gst_bin_add(GST_BIN(filter), GST_ELEMENT(filter->video_element));
  filter->audio_element = GST_OFTVG_AUDIO(gst_element_factory_make("oftvg_audio", "audio"));
  gst_bin_add(GST_BIN(filter), GST_ELEMENT(filter->audio_element));
  
  /* Add ghost pads for the pads */
  pad = gst_element_get_static_pad(GST_ELEMENT(filter->video_element), "sink");
  gst_element_add_pad(GST_ELEMENT(filter), gst_ghost_pad_new ("sink", pad));
  gst_object_unref(GST_OBJECT(pad));
  pad = gst_element_get_static_pad(GST_ELEMENT(filter->video_element), "src");
  gst_element_add_pad(GST_ELEMENT(filter), gst_ghost_pad_new ("src", pad));
  gst_object_unref(GST_OBJECT(pad));
  pad = gst_element_get_static_pad(GST_ELEMENT(filter->audio_element), "src");
  gst_element_add_pad(GST_ELEMENT(filter), gst_ghost_pad_new ("asrc", pad));
  gst_object_unref(GST_OBJECT(pad));
  
  /* Connect the signals from the video element */
  g_signal_connect(filter->video_element, "lipsync-generated",
                   G_CALLBACK(lipsync_generated_cb), filter);
  g_signal_connect(filter->video_element, "video-processed-upto",
                   G_CALLBACK(video_processed_upto_cb), filter);
  g_signal_connect(filter->video_element, "video-end-of-stream",
                   G_CALLBACK(video_end_of_stream_cb), filter);
}

/* Property setting */
static void gst_oftvg_set_property(GObject *object, guint prop_id, const GValue* value, GParamSpec * pspec)
{
  GstOFTVG* filter = GST_OFTVG(object);
  
  switch (prop_id)
  {
#define PROP_STR(up,name,desc,def)  \
    case PROP_ ## up: \
      g_object_set(filter->video_element, #name, g_value_get_string(value), NULL); \
      break;
#define PROP_INT(up,name,desc,def)  \
    case PROP_ ## up: \
      g_object_set(filter->video_element, #name, g_value_get_int(value), NULL); \
      break;
#define PROP_BOOL(up,name,desc,def) \
    case PROP_ ## up: \
      g_object_set(filter->video_element, #name, g_value_get_boolean(value), NULL);\
      break;
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
static void gst_oftvg_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstOFTVG* filter = GST_OFTVG(object);
  
  switch (prop_id)
  {
#define PROP_STR(up,name,desc,def)  \
    case PROP_ ## up: \
    { \
      gchar *str; \
      g_object_get(filter->video_element, #name, &str, NULL); \
      g_value_set_string(value, str); \
      g_free(str); \
      break; \
    }
#define PROP_INT(up,name,desc,def)  \
    case PROP_ ## up: \
    { \
      gint intval; \
      g_object_get(filter->video_element, #name, &intval, NULL); \
      g_value_set_int(value, intval); \
      break; \
    }
#define PROP_BOOL(up,name,desc,def) \
    case PROP_ ## up: \
    { \
      gboolean boolval; \
      g_object_get(filter->video_element, #name, &boolval, NULL); \
      g_value_set_boolean(value, boolval); \
      break; \
    }
GSTOFTVG_VIDEO_PROPERTIES
#undef PROP_STR
#undef PROP_INT
#undef PROP_BOOL

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* Pass signals from the video element to the audio side */
static void lipsync_generated_cb(GstElement *video_element, GstClockTime start,
                                 GstClockTime end, GstOFTVG *filter)
{
  gst_oftvg_audio_generate_beep(filter->audio_element, start, end);
}

static void video_processed_upto_cb(GstElement *video_element, GstClockTime start,
                                    GstOFTVG *filter)
{
  gst_oftvg_audio_generate_silence(filter->audio_element, start);
}

static void video_end_of_stream_cb(GstElement *video_element, GstOFTVG *filter)
{
  gst_oftvg_audio_end_stream(filter->audio_element);
}

