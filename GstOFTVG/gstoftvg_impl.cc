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

#include <string>
#include <cstring>
#include <fstream>
#include <sstream>

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


/* debug category for filtering log messages
 *
 */
#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_oftvg_debug, "oftvg", 0, \
    "OptoFidelity Test Video Generator");


// Utils
static int gst_oftvg_integer_log2(int val);

/// Gets the amount that x coordinate needs to be shifted to the right
/// to get a matching coordinate in the component data that is possibly
/// subsampled.
static int gst_oftvg_get_subsampling_h_shift(GstVideoInfo *info,
  int component, int width);

/// Gets the amount that y coordinate needs to be shifted to the right
/// to get a matching coordinate in the component data that is possibly
/// subsampled.
static int gst_oftvg_get_subsampling_v_shift(GstVideoInfo *info,
  int component, int height);

Oftvg::Oftvg()
  : calibration_layouts(), layout()
{
  Oftvg* filter = this;
  filter->silent = FALSE;
  filter->layout_location = NULL;
  filter->calibration_prepend = false;
  filter->num_buffers = -1;
  reset();
  DEBUG_INIT
}

void Oftvg::reset()
{
  // Start with calibration frames (repeat_count = 0).
  Oftvg* filter = this;
  filter->state = STATE_PRECALIBRATION;
  filter->frame_counter = 0;
  filter->progress_timestamp = G_MININT64;
}

/////////
// Initialization
/////////

bool Oftvg::videoFormatSetCaps(GstCaps* incaps)
{
  gst_video_info_init(&in_info);
  if (!gst_caps_is_fixed(incaps) || !gst_video_info_from_caps(&in_info, incaps)) {
	  return false;
  }

  this->in_format = GST_VIDEO_INFO_FORMAT(&in_info);
  this->in_format_info = gst_video_format_get_info(this->in_format);
  this->width = GST_VIDEO_INFO_WIDTH(&in_info);
  this->height = GST_VIDEO_INFO_HEIGHT(&in_info);
  return true;
}

bool Oftvg::initParams()
{
  init_colorspace();
  init_sequence();
  init_layout();
  if (GST_VIDEO_FORMAT_INFO_IS_YUV(in_format_info)
    || GST_VIDEO_FORMAT_INFO_IS_RGB(in_format_info)) {
	  return true;
  }
  else {
	  return false;
  }
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
  this->frame_counter = frame_counter;
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
  return calibration_frames + filter->num_buffers;
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
  if (GST_VIDEO_FORMAT_INFO_IS_YUV(in_format_info))
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

  for (size_t i = 0; i < sizeof(calibrationModes)/sizeof(calibrationModes[0]); ++i)
  {
    GstOFTVGLayout new_layout;
    OFTVG::OverlayMode mode = calibrationModes[i];
    
    filter->calibration_layouts.push_back(new_layout);    
    GstOFTVGLayout& layout = filter->calibration_layouts.back();

    ret &= gst_oftvg_load_layout_bitmap(filename, &error, &layout,
      filter->width, filter->height, mode, sequence_data);
  }

  ret &= gst_oftvg_load_layout_bitmap(filename, &error, &filter->layout,
    filter->width, filter->height, OFTVG::OVERLAY_MODE_DEFAULT, sequence_data);

  if (!ret)
  {
    GST_ELEMENT_ERROR(&filter->element().element, RESOURCE, OPEN_READ,
      ("Could not open layout file: %s. %s", filename, error->message),
      (NULL));
  }
}

static OFTVG::MarkColor char_to_color(char c)
{
  switch (c)
  {
    case 'w': return OFTVG::MARKCOLOR_WHITE;
    case 'k': return OFTVG::MARKCOLOR_BLACK;
    case 'r': return OFTVG::MARKCOLOR_RED;
    case 'g': return OFTVG::MARKCOLOR_GREEN;
    case 'b': return OFTVG::MARKCOLOR_BLUE;
    case 'c': return OFTVG::MARKCOLOR_CYAN;
    case 'm': return OFTVG::MARKCOLOR_PURPLE;
    case 'y': return OFTVG::MARKCOLOR_YELLOW;
    default: return OFTVG::MARKCOLOR_TRANSPARENT;
  }
}

void Oftvg::init_sequence()
{
  if (strlen(custom_sequence) > 0)
  {
    std::ifstream file(custom_sequence);
    sequence_data.clear();

    if (!file.good())
    {
      g_print("Could not load the custom sequence file %s.\n", custom_sequence);
      return;
    }

    int frame = 0;
    OFTVG::MarkColor color = OFTVG::MARKCOLOR_WHITE;

    while (file.good())
    {
      std::string line;
      std::getline(file, line);

      if (line.size() < 2 || line.at(0) == '#')
        continue;

      int newframe;
      char newcolor;
      std::istringstream linestream(line);
      linestream >> newframe >> newcolor;

      while (frame < newframe)
      {
        sequence_data.push_back(color);
        frame++;
      }

      frame = newframe;
      color = char_to_color(newcolor);

      sequence_data.push_back(color);
      frame++;
    }

    g_print("Loaded custom sequence with length %d\n", sequence_data.size());
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

  GST_INFO("Found buffer %ld, stopping at %ld", GST_BUFFER_OFFSET(buf), get_max_frame_number());

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

GstFlowReturn Oftvg::handle_frame_numbers(GstBuffer* buf)
{
  Oftvg* filter = this;
  gint64 frame_number = input_frame_number(buf);
  
  report_progress(buf);

  if (filter->state == STATE_PRECALIBRATION)
  {
    GstClockTime ts = calibrationTimestamps[numCalibrationTimestamps-1];
    if (!filter->calibration_prepend ||
        GST_BUFFER_PTS(buf) + GST_BUFFER_DURATION(buf) >= ts)
    {
      filter->state = STATE_VIDEO;
    }
  }
  else if (filter->state == STATE_VIDEO)
  {
    if (frame_number > filter->num_buffers)
    {
      if (!filter->silent)
      {
	g_print("gstoftvg: Enough frames have been processed: %d.\n", (int) frame_number);
      }
      filter->state = STATE_POSTCALIBRATION;
    }
  }
  else if (filter->state == STATE_POSTCALIBRATION)
  {
    if (frame_number > filter->num_buffers)
    {
      /* Everything done */
      return GST_FLOW_EOS;
    }
  }
 
  frame_counter_advance();

  return GST_FLOW_OK;
}

const GstOFTVGLayout&
  Oftvg::get_layout(const GstBuffer* buf)
{
  const Oftvg* filter = this;
  
  if (filter->state == STATE_PRECALIBRATION)
  {
    GstClockTime dest_timestamp = GST_BUFFER_TIMESTAMP(buf);
    int i;
    for (i = 0; i < numCalibrationTimestamps; ++i)
    {
      GstClockTime timestamp = calibrationTimestamps[i];
      if (dest_timestamp < timestamp)
      {
        return filter->calibration_layouts[i];
      }
    }
  }
  else if (filter->state == STATE_VIDEO)
  {
    return filter->layout;
  }
  else if (filter->state == STATE_POSTCALIBRATION)
  {
    return filter->calibration_layouts[0];
  }
  
  /* Not reached */
  g_assert_not_reached();
}


/// In place processing. Calls the processing function to do the
/// actual processing on the buffer.
GstFlowReturn Oftvg::gst_oftvg_transform_ip(GstBuffer *buf)
{
  Oftvg *filter = this;

  GstFlowReturn ret = handle_frame_numbers(buf);

  if (ret == GST_FLOW_OK)
  {
    const GstOFTVGLayout& layout = get_layout(buf);

    if (layout.size() == 0)
    {
      // Reported elsewhere
      return GST_FLOW_ERROR;
    }

    gint64 frame_number = input_frame_number(buf);

    // Call through member function pointer.
    process_default(buf, (int) frame_number, layout);
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
static int gst_oftvg_get_subsampling_h_shift(GstVideoInfo *info,
  int component, int width)
{
  int val =
    width / GST_VIDEO_INFO_COMP_WIDTH(info, component);
  return gst_oftvg_integer_log2(val);
}

/// Gets the amount that y coordinate needs to be shifted to the right
/// to get a matching coordinate in the component data that is possibly
/// subsampled.
static int gst_oftvg_get_subsampling_v_shift(GstVideoInfo *info,
  int component, int height)
{
  int val =
    height / GST_VIDEO_INFO_COMP_HEIGHT(info, component);
  return gst_oftvg_integer_log2(val);
}


////////////////////
// Process one frame
////////////////////

/// Generic processing function. The color components to use are
/// determined by filter->color.
void Oftvg::process_default(GstBuffer *buf, int frame_number,
  const GstOFTVGLayout& layout)
{
  Oftvg* filter = this;
  GstVideoFormat format = filter->in_format;

  GstMapInfo mapinfo;
  // ToDo: Check if succeess
  gst_buffer_map(buf, &mapinfo, GST_MAP_WRITE);
  guint8 *bufmem = mapinfo.data;
  const int width = filter->width;
  const int height = filter->height;

  guint8* const bufY =
    bufmem + GST_VIDEO_INFO_COMP_OFFSET(&in_info, 0);
  int y_stride  =  GST_VIDEO_INFO_COMP_STRIDE(&in_info, 0);
  int yoff = GST_VIDEO_FORMAT_INFO_PSTRIDE(in_format_info, 0);
  
  int h_subs = gst_oftvg_get_subsampling_h_shift(&in_info, 1, width);
  int v_subs = gst_oftvg_get_subsampling_v_shift(&in_info, 1, height);

  guint8* const bufU =
    bufmem + GST_VIDEO_INFO_COMP_OFFSET(&in_info, 1);
  guint8* const bufV =
    bufmem + GST_VIDEO_INFO_COMP_OFFSET(&in_info, 2);
  
  int uv_stride = GST_VIDEO_INFO_COMP_STRIDE(&in_info, 1);
  int uoff = GST_VIDEO_FORMAT_INFO_PSTRIDE(in_format_info, 1);
  int voff = GST_VIDEO_FORMAT_INFO_PSTRIDE(in_format_info, 2);

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

  gst_buffer_unmap(buf, &mapinfo);
}


} // namespace OFTVG
