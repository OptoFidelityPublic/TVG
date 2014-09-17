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
#include <tr1/memory>
#include <glib.h>

namespace OFTVG
{
  enum OverlayMode
  {
    OVERLAY_MODE_DEFAULT,
    OVERLAY_MODE_WHITE,
    OVERLAY_MODE_CALIBRATION,
    OVERLAY_MODE_RGB6_WHITE
  };

  enum MarkColor
  {
    MARKCOLOR_BLACK = 0,
    MARKCOLOR_RED = 1,
    MARKCOLOR_GREEN = 2,
    MARKCOLOR_YELLOW = 3,
    MARKCOLOR_BLUE = 4,
    MARKCOLOR_PURPLE = 5,
    MARKCOLOR_CYAN = 6,
    MARKCOLOR_WHITE = 7,
    MARKCOLOR_TRANSPARENT
  };
  
  enum FrameFlags
  {
    FRAMEFLAGS_NONE = 0,
    FRAMEFLAGS_LIPSYNC = 1
  };
};

class GstOFTVGLayout;

/**
 * Layout element.
 */
class GstOFTVGElement
{
public:
  GstOFTVGElement(int x, int y, int width, int height);

  inline const int& x() const { return x_; }
  inline const int& y() const { return y_; }
  inline const int& width() const { return width_; }
  inline const int& height() const { return height_; }

  /// Get the color of this marker in the given frame
  virtual OFTVG::MarkColor getColor(int frameNumber, OFTVG::FrameFlags flags) const = 0;

  /// Returns whether the properties apart from location and
  /// size equal to element b.
  virtual bool propertiesEqual(const GstOFTVGElement &b) const = 0;

  /// Identical copy of this element
  virtual GstOFTVGElement *copy() const = 0;

private:
  int x_;
  int y_;
  int width_;
  int height_;

  friend class GstOFTVGLayout;
};

/* Subclass for frame id marks */
class GstOFTVGElement_FrameID: public GstOFTVGElement
{
public:
  GstOFTVGElement_FrameID(int x, int y, int width, int height, int frameid_n);
  virtual OFTVG::MarkColor getColor(int frameNumber, OFTVG::FrameFlags flags) const;
  virtual bool propertiesEqual(const GstOFTVGElement &b) const;
  virtual inline GstOFTVGElement *copy() const { return new GstOFTVGElement_FrameID(*this); }
  
  inline int getFrameId() const { return frameid_; }
private:
  int frameid_;
};

/* Subclass for sync marks */
class GstOFTVGElement_SyncMark: public GstOFTVGElement
{
public:
  GstOFTVGElement_SyncMark(int x, int y, int width, int height, int syncidx, const std::vector<OFTVG::MarkColor> &customseq);
  virtual OFTVG::MarkColor getColor(int frameNumber, OFTVG::FrameFlags flags) const;
  virtual bool propertiesEqual(const GstOFTVGElement &b) const;
  virtual inline GstOFTVGElement *copy() const { return new GstOFTVGElement_SyncMark(*this); }
  
private:
  int syncidx_;
  const std::vector<OFTVG::MarkColor> &customseq_;
};

/* Subclass for calibration background */
class GstOFTVGElement_Constant: public GstOFTVGElement
{
public:
  GstOFTVGElement_Constant(int x, int y, int width, int height, OFTVG::MarkColor color);
  virtual OFTVG::MarkColor getColor(int frameNumber, OFTVG::FrameFlags flags) const;
  virtual bool propertiesEqual(const GstOFTVGElement &b) const;
  virtual inline GstOFTVGElement *copy() const { return new GstOFTVGElement_Constant(*this); }

private:
  OFTVG::MarkColor color_;
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
  
  /// Returns the number of elements.
  inline int size() const {return elements_.size();}

  /// Returns the element at position
  inline const GstOFTVGElement* at(int idx) const
  { return elements_.at(idx).get(); }
  
  /// Returns the number the highest frame number that can be
  /// represented by the frame id marks in the layout.
  int maxFrameNumber() const;

private:
  std::vector<std::tr1::shared_ptr<GstOFTVGElement> > elements_;
};



#endif /* __GST_OFTVG_LAYOUT_HH__ */
