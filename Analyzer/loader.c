#include "loader.h"
#include <gst/app/gstappsink.h>
#include <gst/audio/audio.h>
#include <gst/video/video.h>
#include <stdbool.h>
#include <string.h>

struct _loader_t
{
  GstElement *pipeline;
  GstElement *filesrc;
  GstElement *decodebin;
  GstElement *audioconvert;
  GstElement *videoconvert;
  GstElement *videosink;
  GstElement *audiosink;
  GstBus *bus;
  gint available_audio_buffers;
  gint available_video_buffers;
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
    GstPad *audiopad = gst_element_get_static_pad(state->audioconvert, "sink");
    if (!GST_PAD_IS_LINKED(audiopad))
    {
      if (gst_pad_link(pad, audiopad) != GST_PAD_LINK_OK)
      {
        GST_ELEMENT_ERROR(state->audioconvert, STREAM, WRONG_TYPE, NULL,
                          ("Could not link decodebin to audioconvert"));
      }
      else if (!gst_element_link(state->audioconvert, state->audiosink))
      {
        GST_ELEMENT_ERROR(state->audiosink, STREAM, WRONG_TYPE, NULL,
                          ("Could not link audioconvert to audiosink"));
      }
    }
    g_object_unref(audiopad);
  }
  
  /* Handle video pads */
  if (is_video)
  {
    GstPad *videopad = gst_element_get_static_pad(state->videoconvert, "sink");
    if (!GST_PAD_IS_LINKED(videopad))
    {
      if (gst_pad_link(pad, videopad) != GST_PAD_LINK_OK)
      {
        GST_ELEMENT_ERROR(state->videoconvert, STREAM, WRONG_TYPE, NULL,
                          ("Could not link decodebin to videoconvert"));
      }
      else if (!gst_element_link(state->videoconvert, state->videosink))
      {
        GST_ELEMENT_ERROR(state->videoconvert, STREAM, WRONG_TYPE, NULL,
                          ("Could not link videoconvert to videosink"));
      }
    }
    g_object_unref(videopad);
  }
}

/* Keeps track of the number of available audio/video buffers */
GstFlowReturn new_sample_cb(GstAppSink *appsink, loader_t *state)
{
  if (GST_ELEMENT(appsink) == state->videosink)
  {
    g_atomic_int_inc(&state->available_video_buffers);
  }
  else if (GST_ELEMENT(appsink) == state->audiosink)
  {
    g_atomic_int_inc(&state->available_audio_buffers);
  }
  
  /* Notify any waiting threads */
  gst_bus_post(state->bus,
               gst_message_new_application(GST_OBJECT(appsink),
                                           gst_structure_new_empty("new-sample"))
  );
  
  return GST_FLOW_OK;
}

/* Returns true if there was an error */
static bool handle_bus_errors(loader_t *state, GstMessage *msg, GError **error)
{
  if (msg->type == GST_MESSAGE_ERROR)
  {
    GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(state->pipeline), GST_DEBUG_GRAPH_SHOW_ALL,
                              "error");
    
    gchar *debug = NULL;
    GError *err;
    gst_message_parse_error(msg, &err, &debug);
    *error = g_error_new(err->domain, err->code, "Error from %s: %s\n%s",
                          GST_OBJECT_NAME(msg->src), err->message,
                          debug ? debug : "(no debug info)");
    g_error_free(err);
    g_free(debug);
    return true;
  }
  else
  {
    return false;
  }
}

loader_t *loader_open(const gchar *filename, GError **error)
{
  *error = NULL;
  loader_t *state = g_malloc0(sizeof(loader_t));
  
  state->pipeline = gst_pipeline_new("tvg_analyzer");
  state->filesrc = gst_element_factory_make("filesrc", "filesrc0");
  state->decodebin = gst_element_factory_make("decodebin", "decodebin0");
  state->videosink = gst_element_factory_make("appsink", "videosink");
  state->videoconvert = gst_element_factory_make("videoconvert", "videoconvert0");
  state->audiosink = gst_element_factory_make("appsink", "audiosink");
  state->audioconvert = gst_element_factory_make("audioconvert", "audioconvert0");
  state->bus = gst_pipeline_get_bus(GST_PIPELINE(state->pipeline));
  
  g_object_set(G_OBJECT(state->filesrc), "location", filename, NULL);
  g_object_set(G_OBJECT(state->videosink), "max-buffers", 10, NULL);
  g_object_set(G_OBJECT(state->videosink), "emit-signals", TRUE, NULL);
  g_object_set(G_OBJECT(state->videosink), "sync", FALSE, NULL);
  g_object_set(G_OBJECT(state->videosink), "caps",
               gst_caps_new_simple("video/x-raw",
                                   "format", G_TYPE_STRING, "RGBx",
                                   NULL), NULL
  );
  g_object_set(G_OBJECT(state->audiosink), "max-buffers", 10, NULL);
  g_object_set(G_OBJECT(state->audiosink), "emit-signals", TRUE, NULL);
  g_object_set(G_OBJECT(state->audiosink), "sync", FALSE, NULL);
  g_object_set(G_OBJECT(state->audiosink), "caps",
               gst_caps_new_simple("audio/x-raw",
                                   "format", G_TYPE_STRING, GST_AUDIO_NE(S16),
                                   "channels", G_TYPE_INT, 1,
                                   NULL), NULL
  );
  
  g_signal_connect(state->videosink, "new-sample", G_CALLBACK(new_sample_cb), state);
  g_signal_connect(state->audiosink, "new-sample", G_CALLBACK(new_sample_cb), state);
  
  /* Add and connect elements */
  gst_bin_add_many(GST_BIN(state->pipeline), state->filesrc, state->decodebin,
                   state->videoconvert, state->videosink,
                   state->audioconvert, state->audiosink, NULL);
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
      if (handle_bus_errors(state, msg, error))
      {
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
  g_free(state);
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

void loader_get_resolution(loader_t *state, int *width, int *height, int *stride)
{
  GstPad *videopad = gst_element_get_static_pad(state->videosink, "sink");
  GstCaps *caps = gst_pad_get_current_caps(videopad);
  GstVideoInfo info;
  
  gst_video_info_init(&info);
  gst_video_info_from_caps(&info, caps);
  
  *width = GST_VIDEO_INFO_WIDTH(&info);
  *height = GST_VIDEO_INFO_HEIGHT(&info);
  *stride = GST_VIDEO_INFO_COMP_STRIDE(&info, 0);
  
  gst_caps_unref(caps);
  g_object_unref(videopad);
}

bool loader_get_buffer(loader_t *state, GstBuffer **audio_buf,
                       GstBuffer **video_buf, GError **error)
{
  *audio_buf = NULL;
  *video_buf = NULL;
  *error = NULL;
  
  do
  {
    if (gst_app_sink_is_eos(GST_APP_SINK(state->videosink)) &&
        gst_app_sink_is_eos(GST_APP_SINK(state->audiosink)))
    {
      /* End of stream and appsinks are empty */
      return false;
    }
    
    GstMessage *msg = gst_bus_timed_pop(state->bus, GST_SECOND);
    if (msg != NULL)
    {
      handle_bus_errors(state, msg, error);
      gst_message_unref(msg);
    }
    
    if (g_atomic_int_get(&state->available_audio_buffers))
    {
      g_atomic_int_dec_and_test(&state->available_audio_buffers);
      GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(state->audiosink));
      if (sample)
      {
        *audio_buf = gst_buffer_ref(gst_sample_get_buffer(sample));
        gst_sample_unref(sample);
      }
    }
    
    if (g_atomic_int_get(&state->available_video_buffers))
    {
      g_atomic_int_dec_and_test(&state->available_video_buffers);
      GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(state->videosink));
      if (sample)
      {
        *video_buf = gst_buffer_ref(gst_sample_get_buffer(sample));
        gst_sample_unref(sample);
      }
    }
  } while (*audio_buf == NULL && *video_buf == NULL && *error == NULL);
  
  return true;
}

