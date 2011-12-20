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
 * Private declarations for GstOFTVG element.
 */

#ifndef __GST_OFTVG_IMPL_HH__
#define __GST_OFTVG_IMPL_HH__

#include <vector>

#include <gst/base/gstbasetransform.h>

#include "gstoftvg_layout.hh"

#include "timemeasure.h"

namespace OFTVG
{

class Oftvg
{
public:
  Oftvg();
  void setElement(GstBaseTransform& element)
  {
    element_ = &element;
  }
  bool gst_video_format_parse_caps(GstCaps* incaps);
  bool Oftvg::gst_oftvg_init_params();
  GstFlowReturn Oftvg::gst_oftvg_transform_ip(GstBuffer *buf);

  /* Properties */
  bool getCalibrationPrepend() { return calibration_prepend; }
  void setCalibrationPrepend(bool value) { calibration_prepend = value; }
  int getNumBuffers() { return num_buffers; }
  void setNumBuffers(int value) { num_buffers = value; }
  int getRepeat() { return repeat; }
  void setRepeat(int value) { repeat = value; }
  const gchar* getLayoutLocation() { return layout_location; }
  void setLayoutLocation(const gchar* value) {
    g_free(layout_location);
    layout_location = g_strdup(value);
  }
  bool getSilent() { return silent; }
  void setSilent(bool value) { silent = value; }

  /* state */
  int getRepeatCount() { return repeat_count; }
  bool outputStreamEnded();

private:
  GstBaseTransform& element() { return *element_; }

  void gst_oftvg_frame_counter_init(gint64 frame_counter);
  void Oftvg::gst_oftvg_frame_counter_advance();
  gint64 Oftvg::gst_oftvg_output_frame_number(gint64 frame_number);
  gint64 gst_oftvg_input_frame_number(const GstBuffer* buf);
  gint64 gst_oftvg_get_max_frame_number();
  gint64 Oftvg::gst_oftvg_get_max_output_frame_number(const GstBuffer* buf);

  /* initialization */
  void Oftvg::gst_oftvg_init();
  bool gst_oftvg_set_process_function();
  void Oftvg::gst_oftvg_init_colorspace();
  void Oftvg::gst_oftvg_init_layout();

  /// Report progress
  void Oftvg::gst_oftvg_report_progress(GstBuffer* buf);

  GstFlowReturn Oftvg::gst_oftvg_repeat(GstBuffer* buf);
  GstFlowReturn Oftvg::gst_oftvg_handle_frame_numbers(GstBuffer* buf);

  const GstOFTVGLayout&
    gst_oftvg_get_layout(const GstBuffer* buf);
  
  void gst_oftvg_process_default(guint8 *buf, int frame_number,
    const GstOFTVGLayout& layout);
  
private:
  GstBaseTransform* element_;

  /* caps */
  GstVideoFormat in_format;
  GstVideoFormat out_format;
  int width;
  int height;

  /* internal state */
  
  /// Which round is it? 0 = calibration frames, 1 = first round, ...
  int repeat_count;
  /// Number of frames to process. If -1, the number is determined by
  /// the layout.
  int num_buffers;

  std::vector<GstOFTVGLayout> calibration_layouts;
  GstOFTVGLayout layout;

  /// Plugin's internal frame counter.
  gint64 frame_counter;

  /// Previously reported timestamp
  GstClockTime progress_timestamp;

  /// Timestamp offset for repeating frames
  GstClockTime timestamp_offset;
  
  /// Frame count offset
  gint64 frame_offset;

  /* properties */
  bool silent;
  gchar* layout_location;
  /// Produce calibration frames in the beginning
  bool calibration_prepend;
  /// Repeat first x frames n times.
  int repeat;

  /* processing function */
  typedef void (Oftvg::*ProcessInplaceFunc)(guint8* buf, int frame_number,
    const GstOFTVGLayout& layout);
  
  ProcessInplaceFunc process_inplace;

  /* precalculated values */
  guint8 bit_off_color[4];
  guint8 bit_on_color[4];
};

}; // namespace OFTVG

#endif /* __GST_OFTVG_IMPL_HH__ */

