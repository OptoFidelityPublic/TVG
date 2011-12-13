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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gstoftvg.hh"
#include "gstoftvg_pixbuf.hh"
#include "timemeasure.h"

const static int gst_oftvg_BITS_PER_SAMPLE = 8;

GstOFTVGElement::GstOFTVGElement(int x, int y, int width, int height,
    gboolean isSyncMark, int offset, int period, int duty)
: x_(x), y_(y), width_(width), height_(height),
    isSyncMark_(isSyncMark), offset_(offset), period_(period), duty_(duty)
{
  // For simplicity only elements of height 1 are currently
  // implemented for rendering.
  g_assert(height == 1);
}

GstOFTVGElement::GstOFTVGElement(int x, int y, int width, int height,
    gboolean isSyncMark, int frameid_n)
: x_(x), y_(y), width_(width), height_(height),
    isSyncMark_(isSyncMark),
    offset_(0), period_(1 << frameid_n), duty_(1 << (frameid_n - 1))
{
  // For simplicity only elements of height 1 are currently
  // implemented for rendering.
  g_assert(height == 1);
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
  frameidBits_ |= (1 << frameid_n);
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
  // Copy element
  elements_.push_back(element);
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

static void gst_oftvg_addElementFromRGB(GstOFTVGLayout* layout,
  int x, int y, int width, int height,
  int red, int green, int blue)
{
  const int numSyncMarks = 2;
  const int syncMarks[numSyncMarks][3] = {
    { 255, 0, 0},
    { 0, 255, 0}
  };

  if (red == green && green == blue)
  {
    int val = red;
    if (val % 10 == 0 && val >= 10 && val <= 240)
    {
      int frameid_n = val / 10;
      gboolean isSyncMark = false;
      layout->addPixel(x, y, frameid_n, isSyncMark);
    }
  }
  else
  {
    // Check if it's a syncmark.
    for (int i = 0; i < numSyncMarks; ++i)
    {
      if (red == syncMarks[i][0] && green == syncMarks[i][1]
      && blue == syncMarks[i][2])
      {
        int frameid_n = i + 1;
        gboolean isSyncMark = true;
        layout->addPixel(x, y, frameid_n, isSyncMark);
      }
    }
  }
}

static void gst_oftvg_init_layout_from_bitmap(const GdkPixbuf* buf,
  GstOFTVGLayout* layout)
{
  int width = gdk_pixbuf_get_width(buf);
  int height = gdk_pixbuf_get_height(buf);
  int bits_per_sample = gdk_pixbuf_get_bits_per_sample(buf);
  int rowstride = gdk_pixbuf_get_rowstride(buf);
  const guchar* const pixels = gdk_pixbuf_get_pixels(buf);
  int n_channels = gdk_pixbuf_get_n_channels(buf);

  for (int y = 0; y < height; ++y)
  {
    const guchar* p = pixels + y * rowstride;
    for (int x = 0; x < width; ++x)
    {
      int red = p[0];
      int green = p[1];
      int blue = p[2];
      int pixelWidth = 1;
      int pixelHeight = 1;
      gst_oftvg_addElementFromRGB(layout,
            x,
            y,
            pixelWidth,
            pixelHeight,
            red, green, blue);

      p += n_channels * ((gst_oftvg_BITS_PER_SAMPLE + 7) / 8);
    }
  }
}

void gst_oftvg_load_layout_bitmap(const gchar* filename, GError **error,
  GstOFTVGLayout* layout, int width, int height)
{
  GdkPixbuf* origbuf = gdk_pixbuf_new_from_file(filename, error);
  if (origbuf == NULL)
  {
    return;
  }
  if (gdk_pixbuf_get_bits_per_sample(origbuf) != gst_oftvg_BITS_PER_SAMPLE)
  {
    g_set_error(error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_FAILED,
      ("Layout bitmap is not of expected type."));
    return;
  }
  if (gdk_pixbuf_get_colorspace(origbuf) != GDK_COLORSPACE_RGB)
  {
    g_set_error(error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_FAILED,
      ("Layout bitmap is not of expected type."));
    return;
  }
  
  GdkPixbuf* buf = gdk_pixbuf_scale_simple(origbuf, width, height, GDK_INTERP_NEAREST);
  gdk_pixbuf_unref(origbuf);

  gst_oftvg_init_layout_from_bitmap(buf, layout);
  gdk_pixbuf_unref(buf);
}
