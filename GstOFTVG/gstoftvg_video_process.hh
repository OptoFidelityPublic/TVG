/* Class for the actual video frame processing */

#ifndef GSTOFTVG_VIDEO_PROCESS_HH
#define GSTOFTVG_VIDEO_PROCESS_HH

#include <vector>
#include "gstoftvg_layout.hh"
#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>

class OFTVG_Video_Process
{
public:
  // The init functions below should be called prior to processing video frames.
  // Each function will print an error message and return false if it fails.
  
  // Set the video format
  bool init_caps(GstCaps *incaps);
  
  // Load a custom sequence file, if any.
  bool init_custom_sequence(const gchar* sequence_file);
  
  // Load the layout bitmap
  // init_caps() must be called before this function.
  bool init_layout(const gchar* layout_file);
  
  // Process a fully white calibration frame
  void process_calibration_white(GstBuffer *buf);
  
  // Process a calibration frame with the frame ids in black.
  void process_calibration_marks(GstBuffer *buf);
  
  // Process a normal video frame, based on frame index
  void process_frame(GstBuffer *buf, int frame_index, OFTVG::FrameFlags flags);

  // Process a frame with the defined layout and frame index
  void process_with_layout(GstBuffer *buf, GstOFTVGLayout *layout, int frame_index, OFTVG::FrameFlags flags);
  
private:
  GstOFTVGLayout layout_calibration_white;
  GstOFTVGLayout layout_calibration_marks;
  GstOFTVGLayout layout_normal;
  
  std::vector<OFTVG::MarkColor> custom_sequence;
  
  GstVideoInfo in_info;
  GstVideoFormatInfo const *in_format_info;
  GstVideoFormat in_format;
  int width;
  int height;
};


#endif
