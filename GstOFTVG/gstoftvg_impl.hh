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
 * Implementation of the filter.
 */

#ifndef __GST_OFTVG_IMPL_HH__
#define __GST_OFTVG_IMPL_HH__

#include <vector>

#include <gst/base/gstbasetransform.h>

#include "gstoftvg_layout.hh"

namespace OFTVG
{

/// OFTVG Filter implementation.
class Oftvg
{
public:
  Oftvg();

  /* Initialization */

  void reset();

  /// Sets the element. This has to be called before other
  /// members.
  void setElement(GstBaseTransform& element)
  {
    element_ = &element;
  }
  
  /// Initializes the filter for the format.
  bool videoFormatSetCaps(GstCaps* incaps);

  /// Initializes parameters of the filter.
  /// Precondition: videoFormatSetCaps has been called successfully.
  bool initParams();

  /// Processes one frame.
  /// Precondition: gst_oftvg_init_params has been called succesfully.
  GstFlowReturn gst_oftvg_transform_ip(GstBuffer *buf);

  /* Properties */
  bool getCalibrationPrepend() { return calibration_prepend; }
  void setCalibrationPrepend(bool value) { calibration_prepend = value; }
  bool getCalibrationAppend() { return calibration_append; }
  void setCalibrationAppend(bool value) { calibration_append = value; }
  int getNumBuffers() { return num_buffers; }
  void setNumBuffers(int value) { num_buffers = value; }
  const gchar* getLayoutLocation() { return layout_location; }
  void setLayoutLocation(const gchar* value) {
    g_free(layout_location);
    layout_location = g_strdup(value);
  }
  bool getSilent() { return silent; }
  void setSilent(bool value) { silent = value; }
  const gchar *getCustomSequence() { return custom_sequence; }
  void setCustomSequence(const gchar *value) {
    g_free(custom_sequence);
    custom_sequence = g_strdup(value);
  }

  /* state */
  /// Returns whether we are at the last frame to process from the input
  /// stream.
  bool atInputStreamEnd();

private:
  GstBaseTransform& element() { return *element_; }

  /// Resets the input frame counter.
  void frame_counter_init(gint64 frame_counter);
  /// Advances the frame counter.
  void frame_counter_advance();
  /// Returns the output frame number.
  gint64 output_frame_number(gint64 frame_number);
  /// Returns the input frame number.
  gint64 input_frame_number(const GstBuffer* buf);
  /// Returns the maximum input frame number.
  gint64 get_max_frame_number();
  /// Returns the maximum output frame number.
  gint64 get_max_output_frame_number(const GstBuffer* buf);

  /* initialization */
  void init_colorspace();
  void init_layout();
  void init_sequence();

  /// Report progress
  void report_progress(GstBuffer* buf);

  GstFlowReturn handle_frame_numbers(GstBuffer* buf);

  const GstOFTVGLayout&
    get_layout(const GstBuffer* buf);
  
  void process_default(GstBuffer *buf, int frame_number,
    const GstOFTVGLayout& layout);
  
private:
  GstBaseTransform* element_;

  /* caps */
  GstVideoInfo in_info;
  GstVideoFormatInfo const *in_format_info;
  GstVideoFormat in_format;
  GstVideoFormat out_format;
  int width;
  int height;

  /* internal state */
  
  enum {STATE_PRECALIBRATION, STATE_VIDEO, STATE_POSTCALIBRATION} state; 
  
  /// Number of frames to process. If -1, the number is determined by
  /// the layout.
  int num_buffers;

  std::vector<GstOFTVGLayout> calibration_layouts;
  GstOFTVGLayout layout;

  /// Plugin's internal frame counter.
  gint64 frame_counter;

  /// Previously reported timestamp
  GstClockTime progress_timestamp;

  /// Frame count offset
  gint64 frame_offset;

  /* properties */
  bool silent;
  gchar* layout_location;
  gchar* custom_sequence;
  std::vector<OFTVG::MarkColor> sequence_data;
  /// Produce calibration frames in the beginning
  bool calibration_prepend;
  bool calibration_append;
  
  /* processing function */
  typedef void (Oftvg::*ProcessInplaceFunc)(guint8* buf, int frame_number,
    const GstOFTVGLayout& layout);

  /* precalculated values */
  guint8 (*color_array_)[4];
};

}; // namespace OFTVG

#endif /* __GST_OFTVG_IMPL_HH__ */

