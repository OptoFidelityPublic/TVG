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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>

#include "gstoftvg.hh"
#include "gstoftvg_pixbuf.hh"

#define DO_TIMING 1
#include "timemeasure.h"

#if defined(_MSC_VER)
#define Restrict __restrict
#else
#define Restrict restrict
#endif

GST_DEBUG_CATEGORY_STATIC (gst_oftvg_debug);
#define GST_CAT_DEFAULT gst_oftvg_debug

static const gchar* const DEFAULT_LAYOUT_LOCATION =
  "../layout/test-layout-1920x1080-c.bmp";
  //"../layout/test-layout-1920x355-c.bmp";
  //"../layout/test-layout-1920x1080-b.png";

static const int DEFAULT_CALIBRATION_FRAMES = 0;
static const int DEFAULT_REPEAT = 1;
static const int DEFAULT_NUM_BUFFERS = 0;

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_CALIBRATION_FRAMES,
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

static gint64 gst_oftvg_get_frame_number(GstOFTVG* const filter,
  const GstBuffer* const buf);

static GstFlowReturn gst_oftvg_transform_ip (GstBaseTransform * base,
    GstBuffer * outbuf);

static gboolean gst_oftvg_set_caps(GstBaseTransform* btrans,
  GstCaps* incaps, GstCaps* outcaps);

static GstOFTVGLayout& gst_oftvg_get_layout(GstOFTVG* filter);

static gboolean gst_oftvg_set_process_function(GstOFTVG* filter);
static gboolean gst_oftvg_event(GstBaseTransform* base, GstEvent *event);

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

static void gst_oftvg_frame_counter_init(GstOFTVG* const filter,
  gint64 frame_counter)
{
  filter->frame_counter = frame_counter;
}

static void gst_oftvg_frame_counter_advance(GstOFTVG* const filter)
{
  filter->frame_counter++;
}

/// Gets the frame number
/// @param buf The buffer. Or null if no buffer at hand.
static gint64 gst_oftvg_get_frame_number(GstOFTVG* const filter,
  const GstBuffer* const buf)
{
  if (buf != NULL)
  {
    // For video frames, buf->offset is the frame number of this buffer.
    // By qtdemux and oggdemux offset seems to be constantly -1.
    // Lets use our internal frame counter then.
    if ((gint64) buf->offset >= 0)
    {
      //g_print("offset >= 0 %d\n", (int) buf->offset);
      gst_oftvg_frame_counter_init(filter, buf->offset);
    }
  }
  return filter->frame_counter;
}

static gint64 gst_oftvg_get_max_frame_number(GstOFTVG* const filter)
{
  gint64 max_frame_number = filter->num_buffers - 1;
  if (max_frame_number < 0)
  {
    max_frame_number = filter->layout.maxFrameNumber();
  }
  return max_frame_number;
}

/* initialize the oftvg's class */
static void
gst_oftvg_class_init (GstOFTVGClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBaseTransformClass* btrans = GST_BASE_TRANSFORM_CLASS (klass);

  gobject_class->set_property = gst_oftvg_set_property;
  gobject_class->get_property = gst_oftvg_get_property;

  g_object_class_install_property(gobject_class, PROP_CALIBRATION_FRAMES,
    g_param_spec_int ("calibration-frames", "Calibration frames",
      "Number of calibration frames to produce.",
      0, G_MAXINT, DEFAULT_CALIBRATION_FRAMES,
      (GParamFlags)(G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE)));

  g_object_class_install_property(gobject_class, PROP_LOCATION,
    g_param_spec_string ("location", "Location", "Layout bitmap file location",
      DEFAULT_LAYOUT_LOCATION,
      (GParamFlags)(G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE)));

  g_object_class_install_property(gobject_class, PROP_NUMBUF,
    g_param_spec_int ("num-buffers", "num-buffers", "Number of buffers to process.",
      0, G_MAXINT, DEFAULT_NUM_BUFFERS,
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
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_oftvg_init (GstOFTVG* filter, GstOFTVGClass* klass)
{
  /* unused parameter */ klass;

  filter->silent = FALSE;
  filter->layout_location = g_strdup(DEFAULT_LAYOUT_LOCATION);
  filter->calibration_frames = DEFAULT_CALIBRATION_FRAMES;
  filter->repeat = DEFAULT_REPEAT;
  // Start with calibration frames.
  filter->repeat_count = 0;
  filter->num_buffers = DEFAULT_NUM_BUFFERS;
  filter->frame_counter = 0;
  filter->progress_timestamp = G_MININT64;
  filter->timestamp_offset = 0;

  if (!filter->silent)
  {
    g_print("GstOFTVG initialized.\n");
  }
}

static void
gst_oftvg_set_property (GObject * object, guint prop_id,
    const GValue* value, GParamSpec * pspec)
{
  GstOFTVG *filter = GST_OFTVG (object);

  switch (prop_id) {
    case PROP_CALIBRATION_FRAMES:
      filter->calibration_frames = g_value_get_int(value);
      break;
    case PROP_LOCATION:
      if (filter->layout_location != NULL)
      {
        g_free(filter->layout_location);
      }
      filter->layout_location = g_value_dup_string(value);
      break;
    case PROP_NUMBUF:
      filter->num_buffers = g_value_get_int(value);
      break;
    case PROP_REPEAT:
      filter->repeat = g_value_get_int(value);
      break;
    case PROP_SILENT:
      filter->silent = g_value_get_boolean(value);
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
    case PROP_CALIBRATION_FRAMES:
      g_value_set_int(value, filter->calibration_frames);
      break;
    case PROP_LOCATION:
      g_value_set_string(value, filter->layout_location);
      break;
    case PROP_NUMBUF:
      g_value_set_int(value, filter->num_buffers);
      break;
    case PROP_REPEAT:
      g_value_set_int(value, filter->repeat);
      break;
    case PROP_SILENT:
      g_value_set_boolean(value, filter->silent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

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
      if (filter->repeat_count > 0)
      {
        // drop event
        ret = FALSE;
      }
      break;
    case GST_EVENT_EOS:
      if (gst_oftvg_get_frame_number(filter, NULL)
          < gst_oftvg_get_max_frame_number(filter))
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

/* timing helpers */
static void gst_oftvg_process_ip_begin_timing(GstOFTVG* filter)
{
}

static void gst_oftvg_process_ip_end_timing(GstOFTVG* filter,
  timemeasure_t timer1)
{
  if (!filter->silent)
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
    if (timer_counter % 100 == 0)
    {
      show_timing(total_oftvg_time / 100, "gst_oftvg_transform_ip");
      total_oftvg_time = 0;
    }
    if (timer_counter % 100 == 0)
    {
      double total_pipeline_time = end_timing(total_pipeline_timer,
        "total_pipeline");
      show_timing(total_pipeline_time / timer_counter,
        "total_pipeline");
    }
  }
}

/* Progress reporting */
static void gst_oftvg_report_progress(GstOFTVG* filter, GstBuffer* buf)
{
  gint64 frame_number = gst_oftvg_get_frame_number(filter, buf);
  gint64 max_frame_number = gst_oftvg_get_max_frame_number(filter);

  GstClockTime timestamp = GST_BUFFER_TIMESTAMP(buf);

  if (timestamp / GST_SECOND - filter->progress_timestamp / GST_SECOND != 0)
  {
    filter->progress_timestamp = timestamp;

	// Just show some feedback to user.
	// Currently displayed regardless of the 'silent' attribute, because
    // setting silent=0 also prints some debug stuff.
    gint64 frames_done =
      max_frame_number * filter->repeat_count + frame_number;
    gint64 total_frames = max_frame_number * (filter->repeat + 1);
    float progress =
      ((float) frames_done) * 100 / total_frames;
    float timestamp_seconds = ((float) GST_BUFFER_TIMESTAMP(buf)) / GST_SECOND;

    g_print("Progress: %0.1f%% (%0.1f seconds) complete\n",
      progress, timestamp_seconds);
  }
}

/* Frame number, repeat, calibration frames */

static GstFlowReturn gst_oftvg_handle_frame_numbers(
  GstOFTVG* filter, GstBuffer* buf)
{
  gint64 frame_number = gst_oftvg_get_frame_number(filter, buf);
  gint64 max_frame_number = gst_oftvg_get_max_frame_number(filter);
  
  if (!filter->silent)
  {
    // DEBUG
    //g_print("frame %d (%d / %d) [ts: %0.3f]\n", (int) frame_number,
    //  (int) max_frame_number, filter->repeat_count,
    //  ((double) buf->timestamp) / GST_SECOND);
  }

  gst_oftvg_report_progress(filter, buf);

  gboolean do_repeat = false;
  if (filter->repeat_count == 0 && filter->calibration_frames == 0)
  {
    // There was no calibration frames.
    filter->repeat_count++;
  }
  else if (filter->repeat_count == 0)
  {
    // Calibration frames.
    max_frame_number = filter->calibration_frames - 1;
  }

  if (frame_number >= max_frame_number
    && filter->repeat_count < filter->repeat)
  {
    if (!filter->silent)
    {
      g_print("gstoftvg: repeat\n");
    }
    filter->repeat_count++;
    // Seek the stream
    gint64 seek_pos = 0;
    if (!gst_element_seek(&filter->element.element, 1.0, GST_FORMAT_TIME,
      (GstSeekFlags) (GST_SEEK_FLAG_FLUSH|GST_SEEK_FLAG_ACCURATE),
      GST_SEEK_TYPE_SET, seek_pos, GST_SEEK_TYPE_NONE, 0))
    {
      GST_ELEMENT_ERROR(filter, STREAM, FAILED, (NULL),
        ("gstoftvg: seek"));
      return GST_FLOW_ERROR;
    }

	// Keep the timestamps we output sequential.
	filter->timestamp_offset = GST_BUFFER_TIMESTAMP(buf) + GST_BUFFER_DURATION(buf);

    gst_oftvg_frame_counter_init(filter, 0);
    return GST_FLOW_OK;
  }

  if ((frame_number > max_frame_number)
    || filter->repeat_count > filter->repeat)
  {
    // Enough frames have been processed.
    if (!filter->silent)
    {
      g_print("gstoftvg: Enough frames have been processed: %d x %d.\n",
        filter->repeat_count, (int) frame_number);
    }
    /*
    "It is important to note that only elements driving the pipeline should ever
    send an EOS event. If your element is chain-based, it is not driving the
    pipeline. Chain-based elements should just return GST_FLOW_UNEXPECTED from
    their chain function at the end of the stream (or the configured segment),
    the upstream element that is driving the pipeline will then take care of
    sending the EOS event (or alternatively post a SEGMENT_DONE message on the
    bus depending on the mode of operation)."
    http://gstreamer.freedesktop.org/data/doc/gstreamer/head/pwg/html/section-events-definitions.html
    */
    gst_pad_push_event(filter->element.srcpad, gst_event_new_eos());
    return GST_FLOW_UNEXPECTED;
  }

  gst_oftvg_frame_counter_advance(filter);

  return GST_FLOW_OK;
}

/* GstBaseTransform vmethod implementations */

static void gst_oftvg_before_transform(GstBaseTransform* btrans,
  GstBuffer* buffer)
{
}

/// This function does the actual processing. In place processing.
static GstFlowReturn
gst_oftvg_transform_ip(GstBaseTransform* base, GstBuffer *buf)
{
  timemeasure_t timer1 = begin_timing();

  GstOFTVG *filter = GST_OFTVG(base);
  const GstOFTVGLayout& layout = gst_oftvg_get_layout(filter);

  if (layout.length() == 0)
  {
    // Reported elsewhere
    return GST_FLOW_ERROR;
  }
  
  GST_BUFFER_TIMESTAMP(buf) += filter->timestamp_offset;
  GstFlowReturn ret = gst_oftvg_handle_frame_numbers(filter, buf);
  if (GST_FLOW_IS_SUCCESS(ret))
  {
    gint64 frame_number = gst_oftvg_get_frame_number(filter, buf);
    filter->process_inplace(GST_BUFFER_DATA(buf), filter, (int) frame_number,
      layout);
    gst_oftvg_process_ip_end_timing(filter, timer1);
  }

  return ret;
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
oftvg_init (GstPlugin* oftvg)
{
  return gst_element_register (oftvg, "oftvg", GST_RANK_NONE, GST_TYPE_OFTVG);
}

static void gst_oftvg_init_colorspace(GstOFTVG* filter)
{
  const guint8 bit_on_color_yuv[4] = { 255, 128, 128, 0 };
  const guint8 bit_off_color_yuv[4] = { 0, 128, 128, 0 };

  const guint8 bit_on_color_rgb[4] = { 255, 255, 255, 0 };
  const guint8 bit_off_color_rgb[4] = { 0, 0, 0, 0 };

  if (!filter->silent)
  {
    g_print("gst_oftvg_init_params()\n");
  }

  const guint8* bit_on_color = NULL;
  const guint8* bit_off_color = NULL;

  if (gst_video_format_is_yuv(filter->in_format))
  {
    bit_on_color = bit_on_color_yuv;
    bit_off_color = bit_off_color_yuv;
  }
  else
  {
    bit_on_color = bit_on_color_rgb;
    bit_off_color = bit_off_color_rgb;
  }

  memcpy((void *) &(filter->bit_on_color[0]),
    bit_on_color, sizeof(guint8)*4);
  memcpy((void *) &(filter->bit_off_color[0]),
    bit_off_color, sizeof(guint8)*4);
}

static void gst_oftvg_init_layout(GstOFTVG* filter)
{
  filter->layout.clear();
  const gchar* filename = filter->layout_location;
  GError* error = NULL;

  gst_oftvg_load_layout_bitmap(filename, &error, &filter->calibration_layout,
    filter->width, filter->height, OFTVG::OVERLAY_MODE_CALIBRATION);

  if (error != NULL)
  {
    GST_ELEMENT_ERROR(filter, RESOURCE, OPEN_READ,
      ("Could not open layout file: %s. %s", filename, error->message),
      (NULL));
  }

  gst_oftvg_load_layout_bitmap(filename, &error, &filter->layout,
    filter->width, filter->height, OFTVG::OVERLAY_MODE_DEFAULT);

  if (error != NULL)
  {
    GST_ELEMENT_ERROR(filter, RESOURCE, OPEN_READ,
      ("Could not open layout file: %s. %s", filename, error->message),
      (NULL));
  }
}

static void gst_oftvg_init_params(GstOFTVG* filter)
{
  gst_oftvg_init_colorspace(filter);
  gst_oftvg_init_layout(filter);
}


static gboolean gst_oftvg_set_caps(GstBaseTransform* object,
  GstCaps* incaps, GstCaps* outcaps)
{
  /* unused parameter */ outcaps;
  GstOFTVG *filter = GST_OFTVG(object);

  if (!gst_video_format_parse_caps(incaps, &filter->in_format,
          &filter->width, &filter->height)
    || !gst_video_format_parse_caps(incaps, &filter->out_format,
          &filter->width, &filter->height))
  {
    GST_WARNING_OBJECT(filter, "Failed to parse caps %"
      GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);
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

static int gst_oftvg_integer_log2(int val)
{
  int r = 0;
  while (val >>= 1)
  {
    r++;
  }
  return r;
}

/// Gets the amount that x coordinate needs to be shifted to the right
/// to get a matching coordinate in the component data that is possibly
/// subsampled.
static int gst_oftvg_get_subsampling_h_shift(GstVideoFormat format,
  int component, int width)
{
  int val =
    width / gst_video_format_get_component_width(format, component, width);
  return gst_oftvg_integer_log2(val);
}

/// Gets the amount that y coordinate needs to be shifted to the right
/// to get a matching coordinate in the component data that is possibly
/// subsampled.
static int gst_oftvg_get_subsampling_v_shift(GstVideoFormat format,
  int component, int height)
{
  int val =
    height / gst_video_format_get_component_height(format, component, height);
  return gst_oftvg_integer_log2(val);
}

static GstOFTVGLayout& gst_oftvg_get_layout(GstOFTVG* filter)
{
  GstOFTVGLayout* layout;
  if (filter->repeat_count == 0)
  {
    layout = &(filter->calibration_layout);
  }
  else
  {
    layout = &(filter->layout);
  }
  return *layout;
}

/// Generic processing function. The color components to use are
/// determined by filter->color.
/// Note: gst_oftvg_set_process_function determines
/// which processing function to use.
void gst_oftvg_process_default(guint8 *buf, GstOFTVG* filter, int frame_number,
  const GstOFTVGLayout& layout)
{
  GstVideoFormat format = filter->in_format;

  const int width = filter->width;
  const int height = filter->height;

  guint8* const bufY =
    buf + gst_video_format_get_component_offset(format, 0, width, height);
  int y_stride  = gst_video_format_get_row_stride(format, 0, width);
  int yoff = gst_video_format_get_pixel_stride(format, 0);
  
  int h_subs = gst_oftvg_get_subsampling_h_shift(format, 1, width);
  int v_subs = gst_oftvg_get_subsampling_v_shift(format, 1, height);

  guint8* const bufU =
    buf + gst_video_format_get_component_offset(format, 1, width, height);
  guint8* const bufV =
    buf + gst_video_format_get_component_offset(format, 2, width, height);
  
  int uv_stride = gst_video_format_get_row_stride(format, 1, width);
  int uoff = gst_video_format_get_pixel_stride(format, 1);
  int voff = gst_video_format_get_pixel_stride(format, 2);

  if (layout.length() != 0)
  {
    int length = layout.length();
    for (int i = 0; i < length; ++i)
    {
      const GstOFTVGElement& element = layout.elements()[i];

      // The components are disjoint. There they may be qualified
      // with the restrict keyword.
      guint8* Restrict posY = bufY + element.y() * y_stride
        + element.x() * yoff;
      guint8* Restrict posU = bufU + (element.y() >> v_subs) * uv_stride
        + (element.x() >> h_subs) * uoff;
      guint8* Restrict posV = bufV + (element.y() >> v_subs) * uv_stride
        + (element.x() >> h_subs) * voff;

      if (!element.isTransparent(frame_number))
      {
        gboolean bit_on = element.isBitOn(frame_number);
        
        const guint8* color =
          bit_on ? filter->bit_on_color : filter->bit_off_color;

        for (int dx = 0; dx < element.width(); dx++)
        {
          *posY = color[0];
          posY += yoff;
        }
      
        for (int dx = 0; dx < element.width(); dx += 1 << h_subs)
        {
          *posU = color[1];
          *posV = color[2];
          posU += uoff;
          posV += voff;
        }
      }
    }
  }
}

static gboolean gst_oftvg_set_process_function(GstOFTVG* filter)
{
  if (gst_video_format_is_yuv(filter->in_format)
    || gst_video_format_is_rgb(filter->in_format))
  {
    filter->process_inplace = gst_oftvg_process_default;
  }
  
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
