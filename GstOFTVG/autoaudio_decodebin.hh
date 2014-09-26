/* This is a wrapper around the GStreamer decodebin. It always provides one audio
 * source pad and one video source pad. If either stream is unavailable on input,
 * dummy data is generated.
 */

#ifndef __AUTOAUDIO_DECODEBIN_HH__
#define __AUTOAUDIO_DECODEBIN_HH__

#include <gst/gst.h>
#include <gst/gstbin.h>

G_BEGIN_DECLS

#define GST_TYPE_AUTOAUDIO_DECODEBIN \
  (gst_autoaudio_decodebin_get_type())
#define GST_AUTOAUDIO_DECODEBIN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AUTOAUDIO_DECODEBIN,GstAutoAudioDecodeBin))
#define GST_AUTOAUDIO_DECODEBIN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AUTOAUDIO_DECODEBIN,GstAutoAudioDecodeBinClass))
#define GST_IS_AUTOAUDIO_DECODEBIN(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AUTOAUDIO_DECODEBIN))
#define GST_IS_AUTOAUDIO_DECODEBIN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AUTOAUDIO_DECODEBIN))

typedef struct _GstAutoAudioDecodeBin      GstAutoAudioDecodeBin;
typedef struct _GstAutoAudioDecodeBinClass GstAutoAudioDecodeBinClass;

struct _GstAutoAudioDecodeBin {
  GstBin bin;
  GstElement *decodebin;
  GstElement *dummyvideo;
  GstElement *dummyaudio;
};

struct _GstAutoAudioDecodeBinClass {
  GstBinClass parent_class;
};

GType gst_autoaudio_decodebin_get_type(void);

G_END_DECLS


#endif
