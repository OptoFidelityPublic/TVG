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
const static int gst_oftvg_layout_DEFAULT_CAPACITY = 8;

// Initializes GstOFTVGElement.
static void gst_oftvg_element_init(GstOFTVGElement* element, guint8 frameid_n, gboolean isSyncMark,
  int x, int y, int width, int height)
{
  element->frameid_n = frameid_n;
  element->isSyncMark = isSyncMark;
  element->x = x;
  element->y = y;
  element->width = width;
  element->height = height;
}

GstOFTVGLayout::GstOFTVGLayout()
   : elements_(NULL), n_elements(0), capacity(0)
{
}


void GstOFTVGLayout::addPixel(int x, int y, int frameid_n, gboolean isSyncMark)
{
  if (length() > 0)
  {
    GstOFTVGElement& prev = last();

    if (prev.y == y && x >= prev.x && x < prev.x + prev.width)
    {
      // There is a pixel here already.
      return;
    }
    else if (prev.y == y && prev.x == x - prev.width && prev.frameid_n == frameid_n
      && prev.height == 1 && prev.isSyncMark == isSyncMark)
    {
      // Combine to previous element. Assume we are adding pixels from
      // left to right.
      last().width++;
      return;
    }
  }
  // Create new element.
  int width = 1;
  int height = 1;
  GstOFTVGElement element;
  gst_oftvg_element_init(&element, frameid_n, isSyncMark, x, y, width, height);
  addElement(element);
}

void GstOFTVGLayout::clear()
{
  this->n_elements = 0;
}

void GstOFTVGLayout::addElement(const GstOFTVGElement& element)
{
  GstOFTVGLayout* layout = this;
  if (layout->elements_ == NULL || layout->n_elements == layout->capacity)
  {
    layout->capacity = layout->capacity * 2;
    if (layout->capacity == 0)
    {
      layout->capacity = gst_oftvg_layout_DEFAULT_CAPACITY;
    }
    
    layout->elements_ = (GstOFTVGElement*)
        realloc(layout->elements_, layout->capacity * sizeof(GstOFTVGElement));
  }
  // Copy element
  layout->elements_[layout->n_elements++] = element;
}

int GstOFTVGLayout::length() const
{
  return n_elements;
}

const GstOFTVGElement* GstOFTVGLayout::elements() const
{
  return elements_;
}

GstOFTVGElement& GstOFTVGLayout::last()
{
  g_assert(length() > 0);
  return elements_[length() - 1];
}

static void gst_oftvg_init_layout_from_bitmap(const GdkPixbuf* buf,
  GstOFTVGLayout* layout, int target_width, int target_height)
{
  const int numSyncMarks = 2;
  const int syncMarks[numSyncMarks][3] = {
    { 255, 0, 0},
    { 0, 255, 0}
  };

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
      if (red == green && green == blue)
      {
        int val = red;
        if (val % 10 == 0 && val >= 10 && val <= 240)
        {
          int frameid_n = val / 10;
          gboolean isSyncMark = false;
          layout->addPixel(x * target_width / width, y * target_height / height, frameid_n, isSyncMark);
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
            layout->addPixel(x * target_width / width, y * target_height / height, frameid_n, isSyncMark);
            g_print("%d %d : %d %d -> %d %d\n", target_width, target_height,
              x, y,
              x * target_width / width, y * target_height / height);
          }
        }
      }
      p += n_channels * ((gst_oftvg_BITS_PER_SAMPLE + 7) / 8);
    }
  }
}

void gst_oftvg_load_layout_bitmap(const gchar* filename, GError **error,
  GstOFTVGLayout* layout, int width, int height)
{
  GdkPixbuf* buf;
  *error = NULL;
  buf = gdk_pixbuf_new_from_file(filename, error);
  if (buf == NULL)
  {
    return;
  }
  if (gdk_pixbuf_get_bits_per_sample(buf) != gst_oftvg_BITS_PER_SAMPLE)
  {
    // TODO: Throw or return error
    g_assert_not_reached();
  }
  if (gdk_pixbuf_get_colorspace(buf) != GDK_COLORSPACE_RGB)
  {
    // TODO: Throw or return error
    g_assert(NULL);
  }
  
  gst_oftvg_init_layout_from_bitmap(buf, layout, width, height);
  
  gdk_pixbuf_unref(buf);
}

