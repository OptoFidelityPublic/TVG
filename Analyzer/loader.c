#include "loader.h"
#include <stdbool.h>
#include <string.h>

struct _loader_t
{
  GstElement *pipeline;
  GstElement *filesrc;
  GstElement *decodebin;
  GstElement *videosink;
  GstElement *audiosink;
  GstBus *bus;
};

/* Connects the decodebin to the sinks once the pads are available. */
static void decodebin_pad_added(GstElement *decodebin, GstPad *pad, gpointer data)
{
  loader_t *state = (loader_t*)data;
  bool is_audio;
  bool is_video;
  
  /* Check media type */
  {
    GstCaps *caps = gst_pad_query_caps(pad, NULL);
    GstStructure *str = gst_caps_get_structure(caps, 0);
    is_audio = g_str_has_prefix(gst_structure_get_name(str), "audio/");
    is_video = g_str_has_prefix(gst_structure_get_name(str), "video/");
    gst_caps_unref(caps);
  }
  
  /* Handle audio pads */
  if (is_audio)
  {
    GstPad *audiopad = gst_element_get_static_pad(state->audiosink, "sink");
    if (!GST_PAD_IS_LINKED(audiopad))
    {
      gst_pad_link(pad, audiopad);
    }
    g_object_unref(audiopad);
  }
  
  /* Handle video pads */
  if (is_video)
  {
    GstPad *videopad = gst_element_get_static_pad(state->videosink, "sink");
    if (!GST_PAD_IS_LINKED(videopad))
    {
      gst_pad_link(pad, videopad);
    }
    g_object_unref(videopad);
  }
}

loader_t *loader_open(const gchar *filename, GError **error)
{
  *error = NULL;
  loader_t *state = g_malloc0(sizeof(loader_t));
  
  state->pipeline = gst_pipeline_new("tvg_analyzer");
  state->filesrc = gst_element_factory_make("filesrc", "filesrc0");
  state->decodebin = gst_element_factory_make("decodebin", "decodebin0");
  state->videosink = gst_element_factory_make("fakesink", "videosink");
  state->audiosink = gst_element_factory_make("fakesink", "audiosink");
  state->bus = gst_pipeline_get_bus(GST_PIPELINE(state->pipeline));
  
  g_object_set(G_OBJECT(state->filesrc), "location", filename, NULL);
  
  /* Add and connect elements */
  gst_bin_add_many(GST_BIN(state->pipeline), state->filesrc, state->decodebin,
                   state->videosink, state->audiosink, NULL);
  gst_element_link(state->filesrc, state->decodebin);
  
  /* The decodebin pads are added dynamically */
  g_signal_connect(state->decodebin, "pad-added", G_CALLBACK(decodebin_pad_added), state);
  
  /* Try to start the pipeline and check for errors */
  gst_element_set_state(state->pipeline, GST_STATE_PLAYING);
  while(1)
  {
    GstMessage *msg = gst_bus_timed_pop(state->bus, GST_SECOND);
    if (msg != NULL)
    {
      if (msg->type == GST_MESSAGE_ERROR)
      {
        gchar *debug = NULL;
        GError *err;
        gst_message_parse_error(msg, &err, &debug);
        *error = g_error_new(err->domain, err->code, "Error from %s: %s\n%s",
                             GST_OBJECT_NAME(msg->src), err->message,
                             debug ? debug : "(no debug info)");
        g_error_free(err);
        g_free(debug);
        break;
      }
      else if (msg->type == GST_MESSAGE_STATE_CHANGED)
      {
        GstState newstate;
        gst_message_parse_state_changed(msg, NULL, &newstate, NULL);
        if (newstate == GST_STATE_PLAYING)
        {
          break;
        }
      }
      gst_message_unref(msg);
    }
  }
  
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(state->pipeline), GST_DEBUG_GRAPH_SHOW_ALL,
                            "loader_open_done");
  
  return state;
}

void loader_close(loader_t *state)
{
  gst_element_set_state(state->pipeline, GST_STATE_NULL);
  gst_object_unref(state->pipeline); state->pipeline = NULL;
}

/* Iterate the decodebin children and find an element with given klass.
 * E.g. Demuxer or Decoder/Video */
static const gchar *get_element_name_with_klass(loader_t *state, const gchar *wanted_klass)
{
  GstIterator *iter = gst_bin_iterate_elements(GST_BIN(state->decodebin));
  GValue item = G_VALUE_INIT;
  bool done = false;
  const gchar *result = NULL;
  
  while (gst_iterator_next(iter, &item) == GST_ITERATOR_OK && !done)
  {
    GstElement *element = GST_ELEMENT_CAST(g_value_get_object(&item));
    GstElementClass *eclass = (GstElementClass*)G_OBJECT_GET_CLASS(element);
    const gchar *klass = gst_element_class_get_metadata(eclass,
                                                        GST_ELEMENT_METADATA_KLASS);
    
    if (strstr(klass, wanted_klass))
    {
      result = gst_element_class_get_metadata(eclass,
                                              GST_ELEMENT_METADATA_LONGNAME);
      done = true;
    }
    
    g_value_unset(&item);
  }
  
  gst_iterator_free(iter);
  
  return result;
}

const gchar *loader_get_demux(loader_t *state)
{
  return get_element_name_with_klass(state, "Demux");
}

const gchar *loader_get_video_decoder(loader_t *state)
{
  return get_element_name_with_klass(state, "Decoder/Video");
}

const gchar *loader_get_audio_decoder(loader_t *state)
{
  return get_element_name_with_klass(state, "Decoder/Audio");
}

