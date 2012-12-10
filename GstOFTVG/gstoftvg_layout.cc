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
 * Generic implementation of GstOFTVGLayout.
 * 
 * Layout can be initialized by the functions declared in gstoftvg_pixbuf.hh
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <memory>
#include "gstoftvg_layout.hh"

GstOFTVGElement::GstOFTVGElement(int x, int y, int width, int height)
: x_(x), y_(y), width_(width), height_(height)
{
}

/* Frame ID marks */
GstOFTVGElement_FrameID::GstOFTVGElement_FrameID(int x, int y, int width, int height, int frameid_n):
  GstOFTVGElement(x, y, width, height), frameid_(frameid_n)
{

}

OFTVG::MarkColor GstOFTVGElement_FrameID::getColor(int frameNumber) const
{
  if (frameNumber & (1 << (frameid_ - 1)))
    return OFTVG::MARKCOLOR_WHITE;
  else
    return OFTVG::MARKCOLOR_BLACK;
}

bool GstOFTVGElement_FrameID::propertiesEqual(const GstOFTVGElement &b) const
{
  const GstOFTVGElement_FrameID *other = dynamic_cast<const GstOFTVGElement_FrameID*>(&b);
  return (other != nullptr && frameid_ == other->frameid_);
}

/* Sync marks */
GstOFTVGElement_SyncMark::GstOFTVGElement_SyncMark(int x, int y, int width, int height, int syncidx):
  GstOFTVGElement(x, y, width, height), syncidx_(syncidx)
{

}

OFTVG::MarkColor GstOFTVGElement_SyncMark::getColor(int frameNumber) const
{
  if (syncidx_ == 1)
  {
    // Every frame sync mark
    if (frameNumber & 1)
      return OFTVG::MARKCOLOR_WHITE;
    else
      return OFTVG::MARKCOLOR_BLACK;
  }
  else if (syncidx_ == 2)
  {
    // Every other frame sync mark
    if (frameNumber & 2)
      return OFTVG::MARKCOLOR_WHITE;
    else
      return OFTVG::MARKCOLOR_BLACK;
  }
  else if (syncidx_ == 3)
  {
    // 6-color sync marker
    const OFTVG::MarkColor sequence[6] =
      {OFTVG::MARKCOLOR_RED, OFTVG::MARKCOLOR_YELLOW, OFTVG::MARKCOLOR_GREEN,
       OFTVG::MARKCOLOR_CYAN, OFTVG::MARKCOLOR_BLUE, OFTVG::MARKCOLOR_PURPLE};
    return sequence[frameNumber % 6];
  }
  else if (syncidx_ == 4)
  {
    // 3-color sync marker
    const OFTVG::MarkColor sequence[3] =
      {OFTVG::MARKCOLOR_RED, OFTVG::MARKCOLOR_GREEN, OFTVG::MARKCOLOR_BLUE};
    return sequence[frameNumber % 3];
  }
  else
  {
    return OFTVG::MARKCOLOR_TRANSPARENT; // Unknown
  }
}

bool GstOFTVGElement_SyncMark::propertiesEqual(const GstOFTVGElement &b) const
{
  const GstOFTVGElement_SyncMark *other = dynamic_cast<const GstOFTVGElement_SyncMark*>(&b);
  return (other != nullptr && syncidx_ == other->syncidx_);
}

/* Background for calibration */
GstOFTVGElement_Constant::GstOFTVGElement_Constant(int x, int y, int width, int height, OFTVG::MarkColor color):
  GstOFTVGElement(x, y, width, height), color_(color)
{

}

OFTVG::MarkColor GstOFTVGElement_Constant::getColor(int frameNumber) const
{
  return color_;
}

bool GstOFTVGElement_Constant::propertiesEqual(const GstOFTVGElement &b) const
{
  const GstOFTVGElement_Constant *other = dynamic_cast<const GstOFTVGElement_Constant*>(&b);
  return (other != nullptr && color_ == other->color_);
}

/* GstOFTVGLayout class */

GstOFTVGLayout::GstOFTVGLayout()
   : elements_()
{
}

void GstOFTVGLayout::clear()
{
  if (elements_.size() > 0)
  {
    elements_.clear();
  }
}

void GstOFTVGLayout::addElement(const GstOFTVGElement& element)
{
  if (element.height() != 1)
  {
    // For simplicity only elements of height 1 are currently
    // implemented for rendering.
    // Lets break it down to one pixel high elements.
    for (int y = element.y(); y < element.y() + element.height(); ++y)
    {
      // Copy element
      std::shared_ptr<GstOFTVGElement> rowElement(element.copy());
      rowElement->height_ = 1;
      rowElement->y_ = y;
      addElement(*rowElement);
    }
  }

  // Combine adjancent single-pixel elements
  else if (element.width_ == 1 && element.height_ == 1
      && elements_.size() > 0
      && element.propertiesEqual(*elements_.back())
      && element.x_ == elements_.back()->x_ + 1
      && element.y_ == elements_.back()->y_)
  {
    elements_.back()->width_++;
  }
  
  // Otherwise, add as a new element
  else
  {
    elements_.push_back(std::shared_ptr<GstOFTVGElement>(element.copy()));
  }
}

int GstOFTVGLayout::maxFrameNumber() const
{
  int maxid = 0;
  for (int i = 0; i < size(); i++)
  {
    const GstOFTVGElement_FrameID *frameid = dynamic_cast<const GstOFTVGElement_FrameID*>(at(i));
    if (frameid != nullptr)
    {
      if (frameid->getFrameId() > maxid)
        maxid = frameid->getFrameId();
    }
  }

  return 1 << maxid;
}
