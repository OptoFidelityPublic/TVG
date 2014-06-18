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

#ifndef __GST_OFTVG_VIDEO_H__
#define __GST_OFTVG_VIDEO_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>

/* List of properties understood by the GstOFTVG_Video element.
 * Each entry contains:
 * - uppercase identifier for defines
 * - short name for the property
 * - description of the property
 * - default value for the variable
 */
#define GSTOFTVG_VIDEO_PROPERTIES \
  PROP_STR(CALIBRATION, calibration, "(off|prepend|only|both) Generate white start/ending sequence", "off") \
  PROP_STR(LOCATION,    location,    "Layout bitmap file location" , "layout.bmp") \
  PROP_STR(SEQUENCE,    sequence,    "Optional text file with custom color sequence data", "") \
  PROP_INT(NUM_BUFFERS, num_buffers, "Number of frames to include, -1 for all.", -1) \
  PROP_INT(LIPSYNC,     lipsync,     "Interval of lipsync markers in milliseconds.", -1) \
  PROP_BOOL(SILENT,     silent,      "Suppress progress messages", false)
  
/* Declaration of the GObject subtype */
G_BEGIN_DECLS

#define GST_TYPE_OFTVG_VIDEO \
  (gst_oftvg_video_get_type())
#define GST_OFTVG_VIDEO(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OFTVG_VIDEO,GstOFTVG_Video))
#define GST_OFTVG_VIDEO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OFTVG_VIDEO,GstOFTVG_VideoClass))
#define GST_IS_OFTVG_VIDEO(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OFTVG_VIDEO))
#define GST_IS_OFTVG_VIDEO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OFTVG_VIDEO))

typedef struct _GstOFTVG_Video      GstOFTVG_Video;
typedef struct _GstOFTVG_VideoClass GstOFTVG_VideoClass;

#ifdef __cplusplus
class OFTVG_Video_Process;
#else
typedef struct OFTVG_Video_Process OFTVG_Video_Process;
#endif

enum state_t {STATE_PRECALIBRATION_WHITE, STATE_PRECALIBRATION_MARKS,
              STATE_VIDEO, STATE_POSTCALIBRATION, STATE_END};

/* Structure to contain the internal data of gstoftvg_video elements */
struct _GstOFTVG_Video
{
  GstBaseTransform element;
  GstPad *sinkpad, *srcpad;
  
  /* Current state of filter */
  enum state_t state;
  
  /* Count of frames processed so far in STATE_VIDEO.
   * Used as the frame id (starts from 0). */
  int frame_counter;
  
  /* Timestamp of last state change */
  GstClockTime last_state_change;
  
  /* End time of the video, if known */
  GstClockTime end_of_video;
  
  /* Last time a progress report was printed. */
  GstClockTime progress_timestamp;
  
  /* Last time a lipsync marker was generated. */
  GstClockTime lipsync_timestamp;
  
  /* This is the actual class that does the processing */
  OFTVG_Video_Process* process;
  
  /* Storage for element properties */
#define PROP_STR(up,name,desc,def) gchar *name;
#define PROP_INT(up,name,desc,def) gint name;
#define PROP_BOOL(up,name,desc,def) gboolean name;
GSTOFTVG_VIDEO_PROPERTIES
#undef PROP_STR
#undef PROP_INT
#undef PROP_BOOL
};

struct _GstOFTVG_VideoClass 
{
  GstBaseTransformClass parent_class;
};

GType gst_oftvg_video_get_type (void);

G_END_DECLS

#endif /* __GST_OFTVG_VIDEO_H__ */
