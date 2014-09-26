#include "autoaudio_decodebin.hh"

/* Debug category to use */
GST_DEBUG_CATEGORY_EXTERN(gst_oftvg_debug);
#define GST_CAT_DEFAULT gst_oftvg_debug

/* Templates for sink and source pads */
static GstStaticPadTemplate sink_template =
GST_STATIC_PAD_TEMPLATE (
  "sink",
  GST_PAD_SINK,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS ("ANY")
);

static GstStaticPadTemplate video_src_template =
GST_STATIC_PAD_TEMPLATE (
  "video",
  GST_PAD_SRC,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS ("video/x-raw")
);

static GstStaticPadTemplate audio_src_template =
GST_STATIC_PAD_TEMPLATE (
  "audio",
  GST_PAD_SRC,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS ("audio/x-raw")
);

/* Definition of the GObject subtype. We inherit from GstBin. */
static void gst_autoaudio_decodebin_class_init(GstAutoAudioDecodeBinClass* klass);
static void gst_autoaudio_decodebin_init(GstAutoAudioDecodeBin* filter);
G_DEFINE_TYPE (GstAutoAudioDecodeBin, gst_autoaudio_decodebin, GST_TYPE_BIN);

/* GstElement function overrides */
static void gst_element_state_changed(GstElement *element, GstState oldstate,
                                      GstState newstate, GstState pending);

/* Callbacks from the real decodebin */
static void decodebin_pad_added(GstElement *decodebin, GstPad *pad, gpointer data);
static void decodebin_no_more_pads(GstElement *decodebin, gpointer data);

/* Callback from end-of-stream on sink pad */
static GstPadProbeReturn sink_pad_probe_callback(GstPad *pad, GstPadProbeInfo *info, gpointer data);

/* Initializer for the class type */
static void gst_autoaudio_decodebin_class_init (GstAutoAudioDecodeBinClass* klass)
{
  /* Element metadata */
  {
    GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

    element_class->state_changed = GST_DEBUG_FUNCPTR(gst_element_state_changed);
    
    gst_element_class_set_metadata (element_class,
      "Decodebin with automatic dummy video/audio generation",
      "Generic/Bin/Decoder",
      "Wraps decodebin and always provides one video and one audio pin",
      "OptoFidelity <info@optofidelity.com>");
    
    gst_element_class_add_pad_template (element_class,
        gst_static_pad_template_get (&sink_template));
    gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&video_src_template));
    gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&audio_src_template));
  }
}

/* Initializer for class instances */
static void gst_autoaudio_decodebin_init (GstAutoAudioDecodeBin* filter)
{
  GstPad *pad;
  
  /* Dummy video / audio are added in decodebin_no_more_pads() if needed */
  filter->dummyvideo = NULL;
  filter->dummyaudio = NULL;
  
  /* Create the main decodebin */
  filter->decodebin = gst_element_factory_make("decodebin", "decodebin0");
  gst_bin_add(GST_BIN(filter), filter->decodebin);
  
  /* Ghost the sink pad */
  pad = gst_element_get_static_pad(GST_ELEMENT(filter->decodebin), "sink");
  gst_element_add_pad(GST_ELEMENT(filter), gst_ghost_pad_new("sink", pad));
  gst_object_unref(GST_OBJECT(pad));
  
  /* Ghost the source pads */
  {
    GstPadTemplate *tmpl;
    tmpl = gst_static_pad_template_get(&video_src_template);
    gst_element_add_pad(GST_ELEMENT(filter),
                        gst_ghost_pad_new_no_target_from_template("video", tmpl));
    g_object_unref(tmpl);
    tmpl = gst_static_pad_template_get(&audio_src_template);
    gst_element_add_pad(GST_ELEMENT(filter),
                        gst_ghost_pad_new_no_target_from_template("audio", tmpl));
    g_object_unref(tmpl);
  }
    
  /* Connect the signals from the decodebin */
  g_signal_connect(filter->decodebin, "pad-added", G_CALLBACK(decodebin_pad_added), filter);
  g_signal_connect(filter->decodebin, "no-more-pads", G_CALLBACK(decodebin_no_more_pads), filter);
}

static void gst_element_state_changed(GstElement *element, GstState oldstate,
                                      GstState newstate, GstState pending)
{
  if (newstate == GST_STATE_READY || newstate == GST_STATE_NULL)
  {
    GstAutoAudioDecodeBin *filter = (GstAutoAudioDecodeBin*)element;
    if (filter->dummyaudio != NULL)
    {
      gst_element_set_state(filter->dummyaudio, GST_STATE_NULL);
      gst_bin_remove(GST_BIN(filter), filter->dummyaudio);
      filter->dummyaudio = NULL;
    }
    if (filter->dummyvideo != NULL)
    {
      gst_element_set_state(filter->dummyvideo, GST_STATE_NULL);
      gst_bin_remove(GST_BIN(filter), filter->dummyvideo);
      filter->dummyvideo = NULL;
    }
  }
}

static void decodebin_pad_added(GstElement *decodebin, GstPad *pad, gpointer data)
{
  GstAutoAudioDecodeBin *filter = (GstAutoAudioDecodeBin*)data;
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
    GstPad *audiopad = gst_element_get_static_pad(GST_ELEMENT(filter), "audio");
    if (!gst_ghost_pad_get_target(GST_GHOST_PAD(audiopad)))
    {
      GST_DEBUG("Linking audio pad");
      if (!gst_ghost_pad_set_target(GST_GHOST_PAD(audiopad), pad))
      {
        GST_ELEMENT_ERROR(filter, STREAM, WRONG_TYPE, (NULL),
                          ("Could not link audio pads"));
      }
      
      gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
                        sink_pad_probe_callback, filter, NULL);
    }
    g_object_unref(audiopad);
  }
  
  /* Handle video pads */
  if (is_video)
  {
    GstPad *videopad = gst_element_get_static_pad(GST_ELEMENT(filter), "video");
    if (!gst_ghost_pad_get_target(GST_GHOST_PAD(videopad)))
    {
      GST_DEBUG("Linking video pad");
      if (!gst_ghost_pad_set_target(GST_GHOST_PAD(videopad), pad))
      {
        GST_ELEMENT_ERROR(filter, STREAM, WRONG_TYPE, (NULL),
                          ("Could not link video pads"));
      }
      
      gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
                        sink_pad_probe_callback, filter, NULL);
    }
    g_object_unref(videopad);
  }
}

static void decodebin_no_more_pads(GstElement *decodebin, gpointer data)
{
  GstAutoAudioDecodeBin *filter = (GstAutoAudioDecodeBin*)data;
  
  /* Add dummy audio if needed */
  {
    GstPad *audiopad = gst_element_get_static_pad(GST_ELEMENT(filter), "audio");
    if (!gst_ghost_pad_get_target(GST_GHOST_PAD(audiopad)))
    {
      GST_DEBUG("Inserting dummy audio source");
      filter->dummyaudio = gst_element_factory_make("audiotestsrc", "dummyaudio");
      gst_bin_add(GST_BIN(filter), filter->dummyaudio);
      
      g_object_set(G_OBJECT(filter->dummyaudio), "wave", 4, NULL); /* 4 = silence */
      
      {
        GstPad *dummypad = gst_element_get_static_pad(filter->dummyaudio, "src");
        if (!gst_ghost_pad_set_target(GST_GHOST_PAD(audiopad), dummypad))
        {
          GST_ELEMENT_ERROR(filter, STREAM, WRONG_TYPE, (NULL),
                            ("Could not link dummy audio source"));
        }
        gst_object_unref(dummypad);
      }
      
      gst_element_set_state(filter->dummyaudio, GST_STATE_NEXT(GST_ELEMENT(filter)));
    }
    g_object_unref(audiopad);
  }
  
  /* Add dummy video if needed */
  {
    GstPad *videopad = gst_element_get_static_pad(GST_ELEMENT(filter), "video");
    if (!gst_ghost_pad_get_target(GST_GHOST_PAD(videopad)))
    {
      GST_DEBUG("Inserting dummy video source");
      filter->dummyvideo = gst_element_factory_make("videotestsrc", "dummyvideo");
      gst_bin_add(GST_BIN(filter), filter->dummyvideo);
      
      g_object_set(G_OBJECT(filter->dummyvideo), "pattern", 18, NULL); /* 18 = moving ball */
      
      {
        GstPad *dummypad = gst_element_get_static_pad(filter->dummyvideo, "src");
        if (!gst_ghost_pad_set_target(GST_GHOST_PAD(videopad), dummypad))
        {
          GST_ELEMENT_ERROR(filter, STREAM, WRONG_TYPE, (NULL),
                            ("Could not link dummy audio source"));
        }
        gst_object_unref(dummypad);
      }
      
      gst_element_set_state(filter->dummyvideo, GST_STATE_NEXT(GST_ELEMENT(filter)));
    }
    g_object_unref(videopad);
  }
}

static GstPadProbeReturn sink_pad_probe_callback(GstPad *pad, GstPadProbeInfo *info, gpointer data)
{
  GstAutoAudioDecodeBin *filter = (GstAutoAudioDecodeBin*)data;
  GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
  
  if (event->type == GST_EVENT_EOS)
  {
    /* If we have any dummy sources, make them stop also */
    if (filter->dummyaudio)
    {
      GST_DEBUG("Sending EOS on dummy audio");
      gst_element_send_event(filter->dummyaudio, gst_event_new_eos());
    }
    if (filter->dummyvideo)
    {
      GST_DEBUG("Sending EOS on dummy video");
      gst_element_send_event(filter->dummyvideo, gst_event_new_eos());
    }
  }
  
  return GST_PAD_PROBE_OK;
}