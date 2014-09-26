#include "gstoftvg.hh"
#include "gstoftvg_video.hh"
#include "gstoftvg_audio.hh"
#include "autoaudio_decodebin.hh"

GST_DEBUG_CATEGORY(gst_oftvg_debug);


/* Entry point to the plugin.
 * Registers all elements implemented by this plugin.
 */
gboolean oftvg_init (GstPlugin* plugin)
{
  GST_DEBUG_CATEGORY_INIT(gst_oftvg_debug, "oftvg", 0, "");
  
  return gst_element_register(plugin, "oftvg", GST_RANK_NONE, GST_TYPE_OFTVG)
      && gst_element_register(plugin, "oftvg_video", GST_RANK_NONE, GST_TYPE_OFTVG_VIDEO)
      && gst_element_register(plugin, "oftvg_audio", GST_RANK_NONE, GST_TYPE_OFTVG_AUDIO)
      && gst_element_register(plugin, "autoaudio_decodebin", GST_RANK_NONE, GST_TYPE_AUTOAUDIO_DECODEBIN);
}

#ifndef PACKAGE
#define PACKAGE "gstoftvg"
#endif

/* Information structure for the GStreamer plugin */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    oftvg,
    "OptoFidelity Test Video Generator",
    oftvg_init,
    "Compiled " __DATE__ " " __TIME__,
    "LGPL",
    "OptoFidelity",
    "http://optofidelity.com/"
)