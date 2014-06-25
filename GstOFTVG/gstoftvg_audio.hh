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

#ifndef __GST_OFTVG_AUDIO_H__
#define __GST_OFTVG_AUDIO_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>

/* Declaration of the GObject subtype */
G_BEGIN_DECLS

#define GST_TYPE_OFTVG_AUDIO \
  (gst_oftvg_audio_get_type())
#define GST_OFTVG_AUDIO(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OFTVG_AUDIO,GstOFTVG_Audio))
#define GST_OFTVG_AUDIO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OFTVG_AUDIO,GstOFTVG_AudioClass))
#define GST_IS_OFTVG_AUDIO(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OFTVG_AUDIO))
#define GST_IS_OFTVG_AUDIO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OFTVG_AUDIO))

typedef struct _GstOFTVG_Audio      GstOFTVG_Audio;
typedef struct _GstOFTVG_AudioClass GstOFTVG_AudioClass;
typedef struct _beep_t beep_t;

/* Structure to contain the internal data of gstoftvg_audio elements */
struct _GstOFTVG_Audio
{
  GstBaseTransform element;
  
  /* Queue used to pass information about the beeps to the processing thread */
  GAsyncQueue *queue;
  
  /* Currently active event */
  beep_t *current;
  int phase;
};

struct _GstOFTVG_AudioClass 
{
  GstBaseTransformClass parent_class;
};

GType gst_oftvg_audio_get_type (void);

/* Use these functions to instruct the element in generation of the beeps */
void gst_oftvg_audio_generate_beep(GstOFTVG_Audio* element, GstClockTime start, GstClockTime end);
void gst_oftvg_audio_generate_silence(GstOFTVG_Audio* element, GstClockTime end);
void gst_oftvg_audio_end_stream(GstOFTVG_Audio* element);

G_END_DECLS

#endif /* __GST_OFTVG_AUDIO_H__ */