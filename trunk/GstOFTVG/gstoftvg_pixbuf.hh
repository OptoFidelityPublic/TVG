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
 
#ifndef __GSTOFTVG_PIXBUF_H__
#define __GSTOFTVG_PIXBUF_H__

#include <vector>

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

class GstOFTVGLayout;

/// Layout element.
class GstOFTVGElement
{
public:
  GstOFTVGElement(int x, int y, int width, int height,
    gboolean isSyncMark, int offset, int period, int duty);

  GstOFTVGElement::GstOFTVGElement(int x, int y, int width, int height,
    gboolean isSyncMark, int frameid_n);

  // Implicit copy constructor works.
  inline const int& x() const { return x_; }
  inline const int& y() const { return y_; }
  inline const int& width() const { return width_; }
  inline const int& height() const { return height_; }

  inline gboolean isBitOn(int frame_number) const
  {
    return ((frame_number + (period_ - offset_)) % period_) >= duty_;
  }

  /// Returns whether the properties apart from location and
  /// size equal to element b.
  gboolean propertiesEqual(const GstOFTVGElement& b) const;
private:
  int x_;
  int y_;
  int width_;
  int height_;

  int offset_;
  int period_;
  int duty_;

  /// Whether this element is sync id element which will not be paused.
  gboolean isSyncMark_;

  friend class GstOFTVGLayout;
};

class GstOFTVGLayout
{
public:
  /// Constructs a new layout object.
  GstOFTVGLayout();

  /// Clears the layout.
  void clear();

  /// Adds an element to the layout.
  void addElement(const GstOFTVGElement& element);
  
  /// Adds a pixel to the layout.
  void addPixel(int x, int y, int frameid_n, gboolean isSyncMark);
  
  /// Returns the number of elements.
  int length() const;

  /// Returns the list of elements.
  const GstOFTVGElement* elements() const;

protected:
  /// Get last element.
  /// Precondition: length() > 0.
  GstOFTVGElement& last();

private:
  std::vector<GstOFTVGElement> elements_;
};

/**
 * Loads layout from a bitmap file.
 * If there is an error, error will point to the error message.
 * @param filename name and path of the file
 * @param error Pointer will be set to the error message if there is an error.
 * @param layout Pointer to layout.
 * @param width The target width of the layout.
 * @param height The target height of the layout.
 */
void gst_oftvg_load_layout_bitmap(const gchar* filename, GError **error,
  GstOFTVGLayout* layout, int width, int height);

G_END_DECLS

#endif /* __GST_OFTVG_H__ */
