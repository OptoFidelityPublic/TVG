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
 * Functions to initialize a layout from a bitmap file.
 */

#ifndef __GSTOFTVG_PIXBUF_H__
#define __GSTOFTVG_PIXBUF_H__

#include <vector>

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

class GstOFTVGLayout;

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
  GstOFTVGLayout* layout, int width, int height);

G_END_DECLS

#endif /* __GST_OFTVG_H__ */
