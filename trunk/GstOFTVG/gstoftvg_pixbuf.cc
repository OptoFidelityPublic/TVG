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
 * GstOFTVGLayout initialization from a bitmap file.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gstoftvg_layout.hh"
#include "gstoftvg_pixbuf.hh"

const static int gst_oftvg_BITS_PER_SAMPLE = 8;

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

/// Initialize a layout from a bitmap.
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

/**
 * Loads a layout from a bitmap file. The layout is scaled to the requested
 * width and height.
 * If there is an error, error will point to the error message and false is
 * returned.
 * @param filename name and path of the file
 * @param error Pointer will be set to the error message if there is an error.
 * @param layout Pointer to layout.
 * @param width The target width of the layout.
 * @param height The target height of the layout.
 */
gboolean gst_oftvg_load_layout_bitmap(const gchar* filename, GError **error,
  GstOFTVGLayout* layout, int width, int height)
{
  GdkPixbuf* origbuf = gdk_pixbuf_new_from_file(filename, error);
  if (origbuf == NULL)
  {
    // Error is set by gdk_pixbuf_new_from_file directly.
    goto error;
  }
  if (gdk_pixbuf_get_bits_per_sample(origbuf) != gst_oftvg_BITS_PER_SAMPLE)
  {
    g_set_error(error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_FAILED,
      ("Layout bitmap is not of expected type."));
    goto error;
  }
  if (gdk_pixbuf_get_colorspace(origbuf) != GDK_COLORSPACE_RGB)
  {
    g_set_error(error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_FAILED,
      ("Layout bitmap is not of expected type."));
    goto error;
  }
  
  GdkPixbuf* buf =
    gdk_pixbuf_scale_simple(origbuf, width, height, GDK_INTERP_NEAREST);
  gdk_pixbuf_unref(origbuf);

  gst_oftvg_init_layout_from_bitmap(buf, layout);
  gdk_pixbuf_unref(buf);

  return TRUE;

error:
  if (origbuf != NULL)
  {
    gdk_pixbuf_unref(origbuf);
  }
  return FALSE;
}
