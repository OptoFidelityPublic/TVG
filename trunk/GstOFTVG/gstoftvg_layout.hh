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
 * GstOFTVGLayout describes a layout for frame ID and synchronization marks.
 *
 * The layout is composed of GstOFTVGElements which have a (x,y) location,
 * size and properties which decide when the element will be rendered as a
 * bit being on or off.
 *
 * To render the layout on a video frame, one goes through the elements in
 * the layout and renders white or black pixels in the location of the
 * element depending on whether isBitOn() returns true or false.
 */

#ifndef __GSTOFTVG_LAYOUT_HH__
#define __GSTOFTVG_LAYOUT_HH__

#include <vector>

#include <gst/gst.h>

G_BEGIN_DECLS

class GstOFTVGLayout;

/**
 * Layout element.
 */
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

  /// Returns whether the element should not be rendered.
  inline gboolean isTransparent(int frameNumber) const
  {
    return FALSE;
  }

  /// Returns whether bit should be rendered as white (true) or
  /// black (false).
  inline gboolean isBitOn(int frameNumber) const
  {
      return ((frameNumber + (period_ - offset_)) % period_) >= duty_;
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
  /// Whether this element is shown only in pause mode.
  gboolean isPauseMark_;

  friend class GstOFTVGLayout;
};

/**
 * Layout for the frame ID and synchronization marks.
 */
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
  /// @param mode The mode for rendering.
  const GstOFTVGElement* elements() const;
  
  /// Returns the number the highest frame number that can be
  /// represented by the frame id marks in the layout.
  int maxFrameNumber() const;

protected:
  /// Gets last element.
  /// Precondition: length() > 0.
  GstOFTVGElement& last();

private:
  std::vector<GstOFTVGElement> elements_;
  int frameidBits_;
};

G_END_DECLS

#endif /* __GST_OFTVG_LAYOUT_HH__ */
