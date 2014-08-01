#include "markertype.h"
#include <string.h>
#include <stdbool.h>

/* Count the number of header (all white) frames in the video data. */
static void count_header_frames(videoinfo_t *videoinfo, GArray *frame_data)
{
  size_t i;
  for (i = 0; i < frame_data->len; i++)
  {
    char *frame = g_array_index(frame_data, char*, i);
    
    if ((int)strspn(frame, "w") != videoinfo->num_markers)
      break;
    
    videoinfo->num_header_frames++;
  }
}

/* Count the number of locator (marks in black) frames in the video data. */
static void count_locator_frames(videoinfo_t *videoinfo, GArray *frame_data)
{
  size_t i;
  for (i = videoinfo->num_header_frames;
       i < frame_data->len; i++)
  {
    char *frame = g_array_index(frame_data, char*, i);

    /* Locator frames are a bit difficult to identify as they have both
     * black and white markers. We detect them by requiring that all locator
     * frames are identical. */
    if (i == frame_data->len - 1)
      break;
    
    char *next = g_array_index(frame_data, char*, i+1);
    
    if (strcmp(frame, next) != 0)
    {
      /* If there have already been locator frames, count also this last one. */
      if (videoinfo->num_locator_frames > 0)
        videoinfo->num_locator_frames++;
      
      break;
    }
    
    videoinfo->num_locator_frames++;
  }
}

/* Count the number of content (non-white) frames in the video data. */
static void count_content_frames(videoinfo_t *videoinfo, GArray *frame_data)
{
  size_t i;
  for (i = videoinfo->num_header_frames
           + videoinfo->num_locator_frames; i < frame_data->len; i++)
  {
    char *frame = g_array_index(frame_data, char*, i);
    
    if ((int)strspn(frame, "w") == videoinfo->num_markers)
      break;
    
    videoinfo->num_content_frames++;
  }
}

/* Count the number of trailer (white) frames in the video data. */
static void count_trailer_frames(videoinfo_t *videoinfo, GArray *frame_data)
{
  size_t i;
  for (i = videoinfo->num_header_frames
           + videoinfo->num_locator_frames
           + videoinfo->num_content_frames; i < frame_data->len; i++)
  {
    char *frame = g_array_index(frame_data, char*, i);
    
    if ((int)strspn(frame, "w") != videoinfo->num_markers)
      break;
    
    videoinfo->num_trailer_frames++;
  }
}

#define GET_FRAME() g_array_index(frame_data, char*, framenum++)[marker_index]

/* Determine if this marker is a BW sync/frameid mark */
static bool detect_bw_mark(videoinfo_t *videoinfo, GArray *frame_data,
                           int marker_index, markerinfo_t *markerinfo)
{
  int framenum = 0, i;
  
  /* Should be white for whole header duration */
  for (i = 0; i < videoinfo->num_header_frames; i++)
  {
    char c = GET_FRAME();
    if (c != 'w')
      return false;
  }
  
  /* First locator frame tells us if this is a frameid or a sync mark */
  if (videoinfo->num_locator_frames > 0)
  {
    char c = GET_FRAME();
    i = 1;
    
    if (c == 'w')
      markerinfo->type = TVG_MARKER_SYNCMARK;
    else if (c == 'k')
      markerinfo->type = TVG_MARKER_FRAMEID;
    else
      return false;
  }
  else 
  {
    /* If there are no frame-ids, there are no locator frames either. */
    i = 0;
    markerinfo->type = TVG_MARKER_SYNCMARK;
  }
  
  /* Verify rest of locator frames */
  for (; i < videoinfo->num_locator_frames; i++)
  {
    char c = GET_FRAME();
    char expected = (markerinfo->type == TVG_MARKER_SYNCMARK) ? 'w' : 'k';
    if (c != expected)
      return false;
  }
  
  /* Figure out the interval for the marker */
  for (i = 0; i < videoinfo->num_content_frames; i++)
  {
    char c = GET_FRAME();
    if (c == 'w')
    {
      markerinfo->interval = i++;
      break;
    }
    else if (c != 'k')
    {
      return false;
    }
  }
  
  /* Verify rest of content frames */
  for (; i < videoinfo->num_content_frames; i++)
  {
    char c = GET_FRAME();
    char expected = (i % (markerinfo->interval * 2) < markerinfo->interval) ? 'k' : 'w';
    if (c != expected)
      return false;
  }
  
  /* Verify trailer frames */
  for (i = 0; i < videoinfo->num_trailer_frames; i++)
  {
    char c = GET_FRAME();
    if (c != 'w')
      return false;
  }
  
  return true;
}

/* Determine if this marker is a RGB6 marker */
static bool detect_rgb6_mark(videoinfo_t *videoinfo, GArray *frame_data,
                             int marker_index, markerinfo_t *markerinfo)
{
  int framenum = 0, i;
  
  /* Should be white for whole header duration */
  for (i = 0; i < videoinfo->num_header_frames; i++)
  {
    char c = GET_FRAME();
    if (c != 'w')
      return false;
  }
  
  /* Should be white for locator frames also */
  for (i = 0; i < videoinfo->num_locator_frames; i++)
  {
    char c = GET_FRAME();
    if (c != 'w')
      return false;
  }
  
  /* Should follow the RGB6 sequence for the content frames
   * Up to one frame at a time may be replaced by black lipsync marker. */
  {
    bool was_black = false;
    for (i = 0; i < videoinfo->num_content_frames; i++)
    {
      char c = GET_FRAME();
      char expected = "rygcbm"[i % 6];
      
      if (c == 'k' && !was_black)
        was_black = true;
      else if (c == expected)
        was_black = false;
      else
        return false;
    }
  }
  
  /* Trailer should be white */
  for (i = 0; i < videoinfo->num_trailer_frames; i++)
  {
    char c = GET_FRAME();
    if (c != 'w')
      return false;
  }
  
  markerinfo->type = TVG_MARKER_RGB6;
  markerinfo->interval = 1;
  
  return true;
}

videoinfo_t *markertype_analyze(GArray *frame_data)
{
  int i;
  videoinfo_t *videoinfo = g_malloc0(sizeof(videoinfo_t));
  videoinfo->num_markers = strlen(g_array_index(frame_data, char*, 0));
  videoinfo->markerinfo = g_malloc0(sizeof(markerinfo_t) * videoinfo->num_markers);
  
  count_header_frames(videoinfo, frame_data);
  count_locator_frames(videoinfo, frame_data);
  count_content_frames(videoinfo, frame_data);
  count_trailer_frames(videoinfo, frame_data);
  
  for (i = 0; i < videoinfo->num_markers; i++)
  {
    markerinfo_t *markerinfo = &videoinfo->markerinfo[i];
    if (!detect_bw_mark(videoinfo, frame_data, i, markerinfo) &&
        !detect_rgb6_mark(videoinfo, frame_data, i, markerinfo))
    {
      markerinfo->type = TVG_MARKER_UNKNOWN;
    }
  }
  
  return videoinfo;
}

void markertype_free(videoinfo_t *videoinfo)
{
  g_free(videoinfo->markerinfo); videoinfo->markerinfo = NULL;
  g_free(videoinfo);
}
