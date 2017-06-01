#include "gstoftvg_video_process.hh"
#include "gstoftvg_pixbuf.hh"
#include <string>
#include <cstring>
#include <fstream>
#include <sstream>

/* Debug category to use */
GST_DEBUG_CATEGORY_EXTERN(gst_oftvg_debug);
#define GST_CAT_DEFAULT gst_oftvg_debug

// Set the video format
bool OFTVG_Video_Process::init_caps(GstCaps *incaps)
{
  gst_video_info_init(&in_info);
  if (!gst_caps_is_fixed(incaps) || !gst_video_info_from_caps(&in_info, incaps))
  {
    GST_ERROR("Could not get video info");
    return false;
  }

  in_format = GST_VIDEO_INFO_FORMAT(&in_info);
  in_format_info = gst_video_format_get_info(in_format);
  width = GST_VIDEO_INFO_WIDTH(&in_info);
  height = GST_VIDEO_INFO_HEIGHT(&in_info);
  
  return true;
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
    case 'p': return OFTVG::MARKCOLOR_PURPLE;
    case 'y': return OFTVG::MARKCOLOR_YELLOW;
    default:  return OFTVG::MARKCOLOR_TRANSPARENT;
  }
}

// Load a custom sequence file, if any.
bool OFTVG_Video_Process::init_custom_sequence(const gchar* sequence_file)
{
  if (strlen(sequence_file) > 0)
  {
    std::ifstream file(sequence_file);
    custom_sequence.clear();

    if (!file.good())
    {
      GST_ERROR("Could not load the custom sequence file %s.", sequence_file);
      return false;
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
        custom_sequence.push_back(color);
        frame++;
      }

      frame = newframe;
      color = char_to_color(newcolor);

      custom_sequence.push_back(color);
      frame++;
    }

    g_print("Loaded custom sequence with length %d\n", (int)custom_sequence.size());
  }
  
  return true;
}

// Load the layout bitmap
bool OFTVG_Video_Process::init_layout(const gchar* layout_file, bool calibration_rgb6_white)
{
  GError* error = NULL;
  gboolean ret = TRUE;
  
  // Load the main layout
  {
    layout_normal.clear();
    ret = gst_oftvg_load_layout_bitmap(layout_file, &error, &layout_normal, width, height,
                                       OFTVG::OVERLAY_MODE_DEFAULT, custom_sequence);
  }
  
  // Load the all-white calibration layout
  if (ret)
  {
    layout_calibration_white.clear();
    ret = gst_oftvg_load_layout_bitmap(layout_file, &error, &layout_calibration_white, width, height,
                                       OFTVG::OVERLAY_MODE_WHITE, custom_sequence);
  }
  
  // Load the black marks on white background calibration layout
  if (ret)
  {
    layout_calibration_marks.clear();
    ret = gst_oftvg_load_layout_bitmap(layout_file, &error, &layout_calibration_marks, width, height,
                                       OFTVG::OVERLAY_MODE_CALIBRATION, custom_sequence);
  }
  
  // Layout option where only the RGB6 markers are white during prefix/suffix
  if (ret && calibration_rgb6_white)
  {
    layout_calibration_white.clear();
    ret = gst_oftvg_load_layout_bitmap(layout_file, &error, &layout_calibration_white, width, height,
                                       OFTVG::OVERLAY_MODE_RGB6_WHITE, custom_sequence);
    layout_calibration_marks = layout_calibration_white;
  }
  
  if (!ret)
  {
    GST_ERROR("Could not open layout file: %s. %s", layout_file, error->message);
  }
  
  return ret;
}

// Process a fully white calibration frame
void OFTVG_Video_Process::process_calibration_white(GstBuffer *buf)
{
  process_with_layout(buf, &layout_calibration_white, 0, OFTVG::FRAMEFLAGS_NONE);
}

// Process a calibration frame with the frame ids in black.
void OFTVG_Video_Process::process_calibration_marks(GstBuffer *buf)
{
  process_with_layout(buf, &layout_calibration_marks, 0, OFTVG::FRAMEFLAGS_NONE);
}

// Process a normal video frame, based on frame index
void OFTVG_Video_Process::process_frame(GstBuffer *buf, int frame_index, OFTVG::FrameFlags flags)
{
  process_with_layout(buf, &layout_normal, frame_index, flags);
}

// Computes integer logarithm in base-2
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

/* Color values to use for YUV videos */
static guint8 color_array_yuv[20][4] = {
  {   0, 128, 128, 0}, /* Black */
  { 128,  64, 255, 0}, /* Red */
  { 128,   0,   0, 0}, /* Green */
  { 255,   0, 128, 0}, /* Yellow */
  {  64, 255,   0, 0}, /* Blue */
  { 128, 255, 255, 0}, /* Magenta */
  { 255, 255,   0, 0}, /* Cyan */
  { 255, 128, 128, 0}  /* White */
};

/* Color values to use for RGB videos */
static guint8 color_array_rgb[20][4] = {
  {   0,   0,   0, 0}, /* Black */
  { 255,   0,   0, 0}, /* Red */
  {   0, 255,   0, 0}, /* Green */
  { 255, 255,   0, 0}, /* Yellow */
  {   0,   0, 255, 0}, /* Blue */
  { 255,   0, 255, 0}, /* Magenta */
  {   0, 255, 255, 0}, /* Cyan */
  { 255, 255, 255, 0}  /* White */
};

// Process a frame with the defined layout and frame index
void OFTVG_Video_Process::process_with_layout(GstBuffer *buf, GstOFTVGLayout *layout,
                                              int frame_index, OFTVG::FrameFlags flags)
{
  /* Map the buffer data to memory */
  GstVideoFrame frame = {};
  if (!gst_video_frame_map(&frame, &in_info, buf, GST_MAP_WRITE))
  {
    GST_ERROR("Could not map buffer");
    return;
  }
  
  /* Compute pointers to color components in the buffer */
  guint8* const bufY = GST_VIDEO_FRAME_COMP_DATA(&frame, 0);
  guint8* const bufU = GST_VIDEO_FRAME_COMP_DATA(&frame, 1);
  guint8* const bufV = GST_VIDEO_FRAME_COMP_DATA(&frame, 2);
  
  /* Length of lines in bytes */
  int y_stride       = GST_VIDEO_FRAME_COMP_STRIDE(&frame, 0);
  int uv_stride      = GST_VIDEO_FRAME_COMP_STRIDE(&frame, 1);
  
  /* Increment between pixels in bytes */
  int yoff           = GST_VIDEO_FRAME_COMP_PSTRIDE(&frame, 0);
  int uoff           = GST_VIDEO_FRAME_COMP_PSTRIDE(&frame, 1);
  int voff           = GST_VIDEO_FRAME_COMP_PSTRIDE(&frame, 2);
  
  /* Subsampling of color components */
  int h_subs         = gst_oftvg_get_subsampling_h_shift(&in_info, 1, width);
  int v_subs         = gst_oftvg_get_subsampling_v_shift(&in_info, 1, height);

  for (int i = 0; i < layout->size(); i++)
  {
    const GstOFTVGElement& element = *layout->at(i);

    /* Get color of the marker for this frame */
    OFTVG::MarkColor markcolor = element.getColor(frame_index, flags);
    
    if (markcolor == OFTVG::MARKCOLOR_TRANSPARENT)
      continue;
    
    /* Compute the start of the element in Y/U/V buffers */
    guint8* posY = bufY + element.y() * y_stride + element.x() * yoff;
    guint8* posU = bufU + (element.y() >> v_subs) * uv_stride + (element.x() >> h_subs) * uoff;
    guint8* posV = bufV + (element.y() >> v_subs) * uv_stride + (element.x() >> h_subs) * voff;

    /* Find out the actual color components for this format */
    const guint8* color = NULL;
    if (GST_VIDEO_FORMAT_INFO_IS_YUV(in_format_info))
    {
      color = color_array_yuv[markcolor];
    }
    else
    {
      color = color_array_rgb[markcolor];
    }
    
    /* Draw the marker in the frame */
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

  gst_video_frame_unmap(&frame);
}
