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

#include "gstoftvg_layout.hh"

GstOFTVGElement::GstOFTVGElement(int x, int y, int width, int height,
    gboolean isSyncMark, int offset, int period, int duty)
: x_(x), y_(y), width_(width), height_(height),
    isSyncMark_(isSyncMark), offset_(offset), period_(period), duty_(duty)
{
}

GstOFTVGElement::GstOFTVGElement(int x, int y, int width, int height,
    gboolean isSyncMark, int frameid_n)
: x_(x), y_(y), width_(width), height_(height),
    isSyncMark_(isSyncMark),
    offset_(0), period_(1 << frameid_n), duty_(1 << (frameid_n - 1))
{
  if (frameid_n == 0)
  {
    duty_ = 1;
  }
}

/// Returns whether the properties apart from location and
/// size equal to element b.
gboolean GstOFTVGElement::propertiesEqual(const GstOFTVGElement& b) const
{
  const GstOFTVGElement& a = *this;
  return a.offset_ == b.offset_ && a.period_ == b.period_ && a.duty_ == b.duty_
    && a.isSyncMark_ == b.isSyncMark_;
}

GstOFTVGLayout::GstOFTVGLayout()
   : elements_(), frameidBits_(0)
{
}

/// Adds a pixel to the layout.
/// @param x X coordinate.
/// @param y Y coordinate.
/// @param frameid_n Frame ID number. 1 == the lowest bit of the frame number.
void GstOFTVGLayout::addPixel(int x, int y, int frameid_n, gboolean isSyncMark)
{
  int width = 1;
  int height = 1;
  GstOFTVGElement element(x, y, width, height, isSyncMark, frameid_n);

  if (length() > 0)
  {
    GstOFTVGElement& prev = last();

    if (prev.y() == y && x >= prev.x() && x < prev.x() + prev.width())
    {
      // There is a pixel here already.
      return;
    }
    else if (prev.y() == y && prev.x() == x - prev.width()
      && prev.propertiesEqual(element))
    {
      // Combine with previous element. Assume we are adding pixels from
      // left to right.
      last().width_ += element.width();
      return;
    }
  }
  // Create new element.
  addElement(element);
  frameidBits_ |= (1 << (frameid_n - 1));
}

void GstOFTVGLayout::clear()
{
  if (elements_.size() > 0)
  {
    elements_.clear();
    frameidBits_ = 0;
  }
}

void GstOFTVGLayout::addElement(const GstOFTVGElement& element)
{
  // For simplicity only elements of height 1 are currently
  // implemented for rendering.

  if (element.height() == 1)
  {
    // Copy element
    elements_.push_back(element);
  }
  else
  {
    // Lets break it down to one pixel high elements.
    for (int y = element.y(); y < element.y() + element.height(); ++y)
    {
      // Copy element
      GstOFTVGElement rowElement(element);
      rowElement.height_ = 1;
      rowElement.y_ = y;
      
      addElement(rowElement);
    }
  }
}

int GstOFTVGLayout::length() const
{
  return elements_.size();
}

const GstOFTVGElement* GstOFTVGLayout::elements() const
{
  return &elements_[0];
}

int GstOFTVGLayout::maxFrameNumber() const
{
  return frameidBits_;
}

GstOFTVGElement& GstOFTVGLayout::last()
{
  g_assert(length() > 0);
  return elements_[length() - 1];
}
