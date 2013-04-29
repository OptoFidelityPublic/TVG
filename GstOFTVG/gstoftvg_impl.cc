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
 * OFTVG (OptoFidelity Test Video Generator) is a filter that adds overlay
 * frame id and synchronization markings to each frame.
 *
 * Implementation details.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>

#include "gstoftvg_impl.hh"
#include "gstoftvg_pixbuf.hh"

#if defined(_MSC_VER)
#define Restrict __restrict
#else
#define Restrict restrict
#endif

GST_DEBUG_CATEGORY_STATIC (gst_oftvg_debug);
#define GST_CAT_DEFAULT gst_oftvg_debug

namespace OFTVG
{

/// The timestamps where each calibration phase ends.
static const int numCalibrationTimestamps = 2;
static const GstClockTime calibrationTimestamps[numCalibrationTimestamps] =
  { 4 * GST_SECOND, 5 * GST_SECOND };


/* debug category for fltering log messages
 *
 */
#define DEBUG_INIT(bla) \
  GST_DEBUG_CATEGORY_INIT (gst_oftvg_debug, "oftvg", 0, \
    "OptoFidelity Test Video Generator");


// Utils
static int gst_oftvg_integer_log2(int val);

/// Gets the amount that x coordinate needs to be shifted to the right
/// to get a matching coordinate in the component data that is possibly
/// subsampled.
static int gst_oftvg_get_subsampling_h_shift(GstVideoFormat format,
  int component, int width);

/// Gets the amount that y coordinate needs to be shifted to the right
/// to get a matching coordinate in the component data that is possibly
/// subsampled.
static int gst_oftvg_get_subsampling_v_shift(GstVideoFormat format,
  int component, int height);

Oftvg::Oftvg()
  : layout(), calibration_layouts()
{
  Oftvg* filter = this;
  filter->silent = FALSE;
  filter->layout_location = NULL;
  filter->calibration_prepend = false;
  filter->repeat = 1;
  filter->num_buffers = -1;
  reset();
}

void Oftvg::reset()
{
  // Start with calibration frames (repeat_count = 0).
  Oftvg* filter = this;
  filter->repeat_count = 0;
  filter->frame_counter = 0;
  filter->progress_timestamp = G_MININT64;
  filter->timestamp_offset = 0;
  filter->frame_offset = 0;
}

/////////
// Initialization
/////////

bool Oftvg::videoFormatSetCaps(GstCaps* incaps)
{
  Oftvg* filter = this;
  return ::gst_video_format_parse_caps(incaps, &filter->in_format,
            &filter->width, &filter->height)?true:false;
}

bool Oftvg::initParams()
{
  Oftvg* filter = this;
  init_colorspace();
  init_layout();
  return set_process_function();
}

bool Oftvg::atInputStreamEnd()
{
  return input_frame_number(NULL)
          >= get_max_frame_number();
}

/////////
// Frame numbers
/////////

void Oftvg::frame_counter_init(gint64 frame_counter)
{
  Oftvg* filter = this;
  filter->frame_counter = frame_counter;
}

void Oftvg::frame_counter_advance()
{
  Oftvg* filter = this;
  filter->frame_counter++;
}

gint64 Oftvg::output_frame_number(gint64 frame_number)
{
  const Oftvg* filter = this;
  return frame_number + filter->frame_offset;
}

/// Gets the frame number
/// @param buf The buffer. Or null if no buffer at hand.
gint64 Oftvg::input_frame_number(const GstBuffer* buf)
{
  Oftvg* filter = this;
  if (buf != NULL)
  {
    // For video frames, buf->offset is the frame number of this buffer.
    // By qtdemux and oggdemux offset seems to be constantly -1.
    // Lets use our internal frame counter then.
    if ((gint64) GST_BUFFER_OFFSET(buf) >= 0)
    {
      frame_counter_init(GST_BUFFER_OFFSET(buf));
    }
  }
  return filter->frame_counter;
}

/// Gets the maximum source frame number, or -1 if no frames
/// should be read.
gint64 Oftvg::get_max_frame_number()
{
  const Oftvg* filter = this;
  gint64 max_frame_number = 0;
  if (filter->num_buffers == -1)
  {
    max_frame_number = filter->layout.maxFrameNumber();
  }
  else if (filter->num_buffers >= 0)
  {
    max_frame_number = filter->num_buffers - 1;
  }
  else
  {
    g_assert_not_reached();
  }
  return max_frame_number;
}

gint64 Oftvg::get_max_output_frame_number(const GstBuffer* buf)
{
  const Oftvg* filter = this;
  gint64 calibration_frames = 0;
  if (filter->calibration_prepend)
  {
    // Estimation
    if (GST_BUFFER_DURATION(buf))
      calibration_frames =
        calibrationTimestamps[numCalibrationTimestamps - 1]
        / GST_BUFFER_DURATION(buf);
    else
      calibration_frames = 100; // Variable FPS, just make a guess (only messes up the percent readings).
  }
  if (filter->calibration_append)
  {
    calibration_frames += 200;
  }
  return calibration_frames +
    (get_max_frame_number() + 1) * filter->repeat;
}

bool Oftvg::set_process_function()
{
  Oftvg* filter = this;
  if (gst_video_format_is_yuv(filter->in_format)
    || gst_video_format_is_rgb(filter->in_format))
  {
    filter->process_inplace = &Oftvg::process_default;
  }
  
  return filter->process_inplace != NULL;
}

static guint8 color_array_yuv[20][4] = {
  {   0, 128, 128, 0},
  { 128,  64, 255, 0},
  { 128,   0,   0, 0},
  { 255,   0, 128, 0},
  {  64, 255,   0, 0},
  { 128, 255, 255, 0},
  { 255, 255,   0, 0},
  { 255, 128, 128, 0}
};

static guint8 color_array_rgb[20][4] = {
  {   0,   0,   0, 0},
  { 255,   0,   0, 0},
  {   0, 255,   0, 0},
  { 255, 255,   0, 0},
  {   0,   0, 255, 0},
  { 255,   0, 255, 0},
  {   0, 255, 255, 0},
  { 255, 255, 255, 0}
};

void Oftvg::init_colorspace()
{
  Oftvg* filter = this;

  if (gst_video_format_is_yuv(filter->in_format))
  {
    color_array_ = color_array_yuv;
  }
  else
  {
    color_array_ = color_array_rgb;
  }
}

void Oftvg::init_layout()
{
  Oftvg* filter = this;
  OFTVG::OverlayMode calibrationModes[2] =
  {
    OFTVG::OVERLAY_MODE_WHITE,
    OFTVG::OVERLAY_MODE_CALIBRATION
  };

  const gchar* filename = filter->layout_location;
  GError* error = NULL;
  gboolean ret = TRUE;

  filter->layout.clear();
  filter->calibration_layouts.clear();

  for (int i = 0; i < sizeof(calibrationModes)/sizeof(calibrationModes[0]); ++i)
  {
    GstOFTVGLayout new_layout;
    OFTVG::OverlayMode mode = calibrationModes[i];
    
    filter->calibration_layouts.push_back(new_layout);    
    GstOFTVGLayout& layout = filter->calibration_layouts.back();

    ret &= gst_oftvg_load_layout_bitmap(filename, &error, &layout,
      filter->width, filter->height, mode);
  }

  ret &= gst_oftvg_load_layout_bitmap(filename, &error, &filter->layout,
    filter->width, filter->height, OFTVG::OVERLAY_MODE_DEFAULT);

  if (!ret)
  {
    GST_ELEMENT_ERROR(&filter->element().element, RESOURCE, OPEN_READ,
      ("Could not open layout file: %s. %s", filename, error->message),
      (NULL));
  }
}

/* Progress reporting */
void Oftvg::report_progress(GstBuffer* buf)
{
  Oftvg* filter = this;
  gint64 frame_number = input_frame_number(buf);
  gint64 output_frame_number = Oftvg::output_frame_number(frame_number);
  gint64 max_frame_number = get_max_frame_number();

  GstClockTime timestamp = GST_BUFFER_TIMESTAMP(buf);

  if (timestamp / GST_SECOND - filter->progress_timestamp / GST_SECOND != 0)
  {
    filter->progress_timestamp = timestamp;

	// Just show some feedback to user.
	// Currently displayed regardless of the 'silent' attribute, because
    // setting silent=0 also prints some debug stuff.
    gint64 frames_done = output_frame_number;
    gint64 total_frames = get_max_output_frame_number(buf);
    float progress =
      ((float) frames_done) * 100 / ((float) total_frames + 0.0001f);
    float timestamp_seconds = ((float) GST_BUFFER_TIMESTAMP(buf)) / GST_SECOND;

    g_print("Progress: %0.1f%% (%0.1f seconds) complete\n",
      progress, timestamp_seconds);
  }

  if (!filter->silent)
  {
    // DEBUG
    //g_print("frame %d (%d / %d) [ts: %0.3f]\n", (int) frame_number,
    //  (int) max_frame_number, filter->repeat_count,
    //  ((double) buf->timestamp) / GST_SECOND);
  }
}

/* Frame number, repeat, calibration frames */

GstFlowReturn Oftvg::repeatFromZero(GstBuffer* buf)
{
  Oftvg* filter = this;
  gint64 frame_number = input_frame_number(buf);

  GST_DEBUG("gstoftvg: repeat\n");

  // Seek the stream
  gint64 seek_pos = 0;
  if (!gst_element_seek(&filter->element().element, 1.0, GST_FORMAT_TIME,
          (GstSeekFlags) (GST_SEEK_FLAG_FLUSH|GST_SEEK_FLAG_ACCURATE),
          GST_SEEK_TYPE_SET, seek_pos, GST_SEEK_TYPE_NONE, 0))
  {
    GST_ELEMENT_ERROR(&filter->element(), STREAM, FAILED, (NULL),
      ("gstoftvg: seek"));
    return GST_FLOW_ERROR;
  }

  // Keep the timestamps we output sequential.
  filter->timestamp_offset =
    GST_BUFFER_TIMESTAMP(buf) + GST_BUFFER_DURATION(buf);
  filter->frame_offset =
    output_frame_number(frame_number) + 1;

  frame_counter_init(0);
  return GST_FLOW_OK;
}

GstFlowReturn Oftvg::handle_frame_numbers(GstBuffer* buf)
{
  Oftvg* filter = this;
  gint64 frame_number = input_frame_number(buf);
  gint64 max_frame_number = get_max_frame_number();
  
  report_progress(buf);

  if (filter->repeat_count == 0 && !filter->calibration_prepend)
  {
    // There was no prepend calibration frames.
    filter->repeat_count++;
  }
  else if (filter->repeat_count == filter->repeat + 1 && !filter->calibration_append)
  {
    // There was no append calibration frames.
    filter->repeat_count++;
  }
  else if (filter->repeat_count == 0)
  {
    // Calibration frames. No frame limit. Just time limit.
    max_frame_number = G_MAXINT64;
    GstClockTime ts = calibrationTimestamps[numCalibrationTimestamps-1];
    if (buf->timestamp + buf->duration >= ts)
    {
      filter->repeat_count++;
      return repeatFromZero(buf);
    }
  }
  else if (filter->repeat_count == filter->repeat + 1)
  {
    max_frame_number = 200;
  }

  if (frame_number >= max_frame_number
    && filter->repeat_count < filter->repeat + (filter->calibration_append ? 1 : 0))
  {
    filter->repeat_count++;
    return repeatFromZero(buf);
  }

  if ((frame_number > max_frame_number)
    || filter->repeat_count > filter->repeat + 1)
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
    gst_pad_push_event(filter->element().srcpad, gst_event_new_eos());
    return GST_FLOW_UNEXPECTED;
  }

  frame_counter_advance();

  return GST_FLOW_OK;
}

const GstOFTVGLayout&
  Oftvg::get_layout(const GstBuffer* buf)
{
  const Oftvg* filter = this;
  // Default
  const GstOFTVGLayout* layout = &(filter->layout);

  if (filter->calibration_prepend)
  {
    GstClockTime dest_timestamp = GST_BUFFER_TIMESTAMP(buf);
    int i;
    for (i = 0; i < numCalibrationTimestamps; ++i)
    {
      GstClockTime timestamp = calibrationTimestamps[i];
      if (dest_timestamp < timestamp)
      {
        layout = &(filter->calibration_layouts[i]);
        break;
      }
    }
  }

  if (filter->calibration_append && filter->repeat_count == filter->repeat + 1)
  {
    layout = &(filter->calibration_layouts[0]);
  }

  return *layout;
}


/// In place processing. Calls the processing function to do the
/// actual processing on the buffer.
GstFlowReturn Oftvg::gst_oftvg_transform_ip(GstBuffer *buf)
{
  Oftvg *filter = this;

  GST_BUFFER_TIMESTAMP(buf) += filter->timestamp_offset;
  GstFlowReturn ret = handle_frame_numbers(buf);

  if (GST_FLOW_IS_SUCCESS(ret))
  {
    const GstOFTVGLayout& layout = get_layout(buf);

    if (layout.size() == 0)
    {
      // Reported elsewhere
      return GST_FLOW_ERROR;
    }

    gint64 frame_number = input_frame_number(buf);

    // Call through member function pointer.
    (filter->*(process_inplace))
      (GST_BUFFER_DATA(buf), (int) frame_number, layout);
  }

  return ret;
}




////////
// Utils
////////

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


////////////////////
// Process one frame
////////////////////

/// Generic processing function. The color components to use are
/// determined by filter->color.
/// Note: gst_oftvg_set_process_function determines
/// which processing function to use.
void Oftvg::process_default(guint8 *buf, int frame_number,
  const GstOFTVGLayout& layout)
{
  Oftvg* filter = this;
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

  if (layout.size() != 0)
  {
    int length = layout.size();
    for (int i = 0; i < length; ++i)
    {
      const GstOFTVGElement& element = *layout.at(i);

      // The components are disjoint. There they may be qualified
      // with the restrict keyword.
      guint8* Restrict posY = bufY + element.y() * y_stride
        + element.x() * yoff;
      guint8* Restrict posU = bufU + (element.y() >> v_subs) * uv_stride
        + (element.x() >> h_subs) * uoff;
      guint8* Restrict posV = bufV + (element.y() >> v_subs) * uv_stride
        + (element.x() >> h_subs) * voff;

      OFTVG::MarkColor markcolor = element.getColor(frame_number);
      if (markcolor != OFTVG::MARKCOLOR_TRANSPARENT)
      {
        const guint8* color = color_array_[markcolor];

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


} // namespace OFTVG
