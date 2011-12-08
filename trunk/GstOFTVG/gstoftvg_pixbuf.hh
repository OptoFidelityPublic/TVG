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

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

// Layout element.
struct GstOFTVGElement
{
  int x;
  int y;
  int width;
  int height;
  // frameid_n == 1 (swap state on every frame)
  // frameid_n == 2 (swap state on every other frame)
  guint8 frameid_n;
  // Whether this element is sync id element which will not be paused.
  gboolean isSyncMark;
};

class GstOFTVGLayout
{
public:
  // Constructs a new layout object.
  GstOFTVGLayout();

  // Clears the layout.
  void clear();

  // Adds an element to the layout.
  void addElement(const GstOFTVGElement& element);
  
  // Adds a pixel to the layout.
  void addPixel(int x, int y, int frameid_n, gboolean isSyncMark);
  
  // Returns the number of elements.
  int length() const;

  // Returns the list of elements.
  const GstOFTVGElement* elements() const;

protected:
  // Get last element.
  // Precondition: length() > 0.
  GstOFTVGElement& last();

private:
  // Elements buffer. May be NULL.
  GstOFTVGElement* elements_;
  // Number of elements.
  int n_elements;
  // Capacity of current elements buffer. How many elements
  // will fit.
  int capacity;
};

void gst_oftvg_load_layout_bitmap(const gchar* filename, GError **error,
  GstOFTVGLayout* layout, int width, int height);

G_END_DECLS

#endif /* __GST_OFTVG_H__ */
