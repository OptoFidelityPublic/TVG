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
 * Private declarations for GstOFTVG element.
 */

#ifndef __GST_OFTVG_HH__
#define __GST_OFTVG_HH__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/base/gstbasetransform.h>

#include "gstoftvg_layout.hh"

G_BEGIN_DECLS

#define GST_TYPE_OFTVG \
  (gst_oftvg_get_type())
#define GST_OFTVG(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OFTVG,GstOFTVG))
#define GST_OFTVG_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OFTVG,GstOFTVGClass))
#define GST_IS_OFTVG(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OFTVG))
#define GST_IS_OFTVG_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OFTVG))

typedef struct _GstOFTVG      GstOFTVG;
typedef struct _GstOFTVGClass GstOFTVGClass;

struct _GstOFTVG {
  GstBaseTransform element;

  /* caps */
  GstVideoFormat in_format;
  GstVideoFormat out_format;
  int width;
  int height;

  /* internal state */
  
  /// How many times to repeat the frames.
  int repeat_count;
  /// Number of frames to process. If -1, the number is determined by
  /// the layout.
  int num_buffers;

  GstOFTVGLayout layout;

  /// Plugin's internal frame counter.
  gint64 frame_counter;

  /* properties */
  gboolean silent;
  gchar* layout_location;
  /// Repeat first x frames n times.
  int repeat;
  
  /* processing function */
  void (*process_inplace)(guint8 *buf, GstOFTVG *filter, int frame_number);

  /* precalculated values */
  guint8 bit_off_color[4];
  guint8 bit_on_color[4];

  /* timestamp offset for repeating frames */
  GstClockTime timestamp_offset;
};

struct _GstOFTVGClass {
  GstBaseTransformClass parent_class;
};

GType gst_oftvg_get_type (void);

G_END_DECLS

#endif /* __GST_OFTVG_HH__ */
