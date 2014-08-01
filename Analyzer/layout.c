#include "layout.h"
#include <stdbool.h>
#include <zlib.h>
#include <glib.h>
#include <string.h>

struct _layout_t
{
  int width;
  int height;
  
  /* Pixel mask of the pixels that are fully saturated. */
  uint8_t *saturated;
  
  /* Pixel mask of the pixels that are black/white. */
  uint8_t *blackwhite;
  
  /* Minimum and maximum values of each color component. */
  uint8_t *min_values;
  uint8_t *max_values;
  
  /* Pixel color checksums to detect connected areas. */
  uint32_t *pixel_crcs;
};

layout_t *layout_create(int width, int height)
{
  layout_t *layout = g_malloc0(sizeof(layout_t));
  
  layout->width = width;
  layout->height = height;
  
  layout->saturated = g_malloc(width * height);
  layout->blackwhite = g_malloc(width * height);
  layout->min_values = g_malloc(width * height * 4);
  layout->max_values = g_malloc(width * height * 4);
  layout->pixel_crcs = g_malloc(width * height * 4);
  
  memset(layout->saturated, 1, width * height);
  memset(layout->blackwhite, 1, width * height);
  memset(layout->min_values, 255, width * height * 4);
  memset(layout->max_values, 0, width * height * 4);
  memset(layout->pixel_crcs, 1, width * height * 4);
  
  return layout;
}

void layout_free(layout_t *layout)
{
  g_free(layout->saturated);  layout->saturated = NULL;
  g_free(layout->blackwhite);  layout->blackwhite = NULL;
  g_free(layout->min_values); layout->min_values = NULL;
  g_free(layout->max_values); layout->max_values = NULL;
  g_free(layout->pixel_crcs); layout->pixel_crcs = NULL;
  g_free(layout);
}

void layout_process(layout_t *layout, const uint8_t *frame, int stride)
{
  int x, y, c;
  for (y = 0; y < layout->height; y++)
  {
    for (x = 0; x < layout->width; x++)
    {
      /* We only care about the saturated pixels */
      int index_pixel = y * layout->width + x;
      if (layout->saturated[index_pixel])
      {
        int min = 255;
        int max = 0;
        for (c = 0; c < 3; c++)
        {
          int index_frame = y * stride + x * 4 + c;
          int index_color = index_pixel * 4 + c;
          uint8_t value = frame[index_frame];
          
          if (value < layout->min_values[index_color])
            layout->min_values[index_color] = value;
          if (value > layout->max_values[index_color])
            layout->max_values[index_color] = value;
          
          if (value < min)
            min = value;
          if (value > max)
            max = value;
        }
        
        if (min != 0 && min != 255 && max != 0 && max != 255)
        {
          /* This pixel wasn't fully saturated. */
          layout->saturated[index_pixel] = 0;
        }
        
        /* Update CRC32 of the pixel in order to detect joined areas.
         * Uses only the saturated pixel value in order to be tolerant of
         * lossy compression. */
        {
          uint8_t sat[3] = {
            frame[y * stride + x * 4 + 0] > TVG_COLOR_THRESHOLD,
            frame[y * stride + x * 4 + 1] > TVG_COLOR_THRESHOLD,
            frame[y * stride + x * 4 + 2] > TVG_COLOR_THRESHOLD
          };
          
          layout->pixel_crcs[index_pixel] = crc32(layout->pixel_crcs[index_pixel],
                                                  sat, 3);
          
          if (sat[0] != sat[1] || sat[1] != sat[2])
          {
            layout->blackwhite[index_pixel] = 0;
          }
        }
      }
    }
  }
}

/* Zero out the CRC for any pixels that fail the saturation/change checks. */
static void filter_crcs(layout_t *layout)
{
  int x, y;
  
  for (y = 0; y < layout->height; y++)
  {
    for (x = 0; x < layout->width; x++)
    {
      int index_pixel = y * layout->width + x;
      
      /* If some pixel happens by luck to have 0 CRC, rewrite it so that we can
       * use 0 as our special value. */
      if (layout->pixel_crcs[index_pixel] == 0)
        layout->pixel_crcs[index_pixel] = 1;
      
      if (layout->saturated[index_pixel] == 0)
      {
        /* Not saturated */
        layout->pixel_crcs[index_pixel] = 0;
      }
      else
      {
        uint8_t min1 = layout->min_values[index_pixel * 4 + 0];
        uint8_t max1 = layout->max_values[index_pixel * 4 + 0];
        uint8_t min2 = layout->min_values[index_pixel * 4 + 1];
        uint8_t max2 = layout->max_values[index_pixel * 4 + 1];
        uint8_t min3 = layout->min_values[index_pixel * 4 + 2];
        uint8_t max3 = layout->max_values[index_pixel * 4 + 2];
        
        if (min1 == max1 || min2 == max2 || min3 == max3)
        {
          /* Pixel value in atleast one color channel has not changed. */
          layout->pixel_crcs[index_pixel] = 0;
        }
      }
    }
  }
}

/* Find connected areas and collect information about them. */
static void find_markers(layout_t *layout, GArray *dest)
{
  int x, y, i;
  uint8_t *labels = g_malloc0(layout->width * layout->height);
  uint8_t *to_join = g_malloc0(256);
  int next_label = 1;
  
  for (y = 0; y < layout->height; y++)
  {
    for (x = 0; x < layout->width; x++)
    {
      int index_pixel = y * layout->width + x;
      int current = layout->pixel_crcs[index_pixel];
      
      if (current != 0)
      {
        int west = 0;
        int north = 0;
        int east = 0;
        int south = 0;
        int label = 0;
        
        if (y > 0) north = layout->pixel_crcs[index_pixel - layout->width];        
        if (x > 0) west = layout->pixel_crcs[index_pixel - 1];
        if (y < layout->height - 1) south = layout->pixel_crcs[index_pixel + layout->width];        
        if (x < layout->width - 1)  east = layout->pixel_crcs[index_pixel + 1];
        
        if (west == current)
          label = labels[index_pixel - 1];
        else if (north == current)
          label = labels[index_pixel - layout->width];
        
        if (west == current && north == current)
        {
          int l1 = labels[index_pixel - 1];
          int l2 = labels[index_pixel - layout->width];
          if (l1 != l2)
          {
            /* We'll need to join l2 into l1 later */
            if (l1 > l2) { int x = l1; l1 = l2; l2 = x; }
            to_join[l2] = l1;
            label = l1;
          }
        }
        
        if (label == 0 && next_label <= 255)
        {
          /* Skip any single-pixel markers */
          if (east == current && south == current)
          {         
            /* Assign new label */
            label = next_label++;
            marker_t marker = {x, y, x, y, false};
            marker.is_rgb = !layout->blackwhite[index_pixel];
            g_array_append_val(dest, marker);
          }
        }
        
        labels[index_pixel] = label;
        
        /* Update bounds of this marker */
        if (label != 0)
        {
          marker_t *marker = &g_array_index(dest, marker_t, label - 1);
          
          if (marker->x1 > x) marker->x1 = x;
          if (marker->y1 > y) marker->y1 = y;
          if (marker->x2 < x) marker->x2 = x;
          if (marker->y2 < y) marker->y2 = y;          
        }
      }
    }
  }
  
  /* Join any areas that were initially added as two markers */
  for (i = next_label - 1; i > 0; i--)
  {
    if (to_join[i] != 0)
    {
      marker_t *m1 = &g_array_index(dest, marker_t, to_join[i] - 1);
      marker_t *m2 = &g_array_index(dest, marker_t, i - 1);
      
      // Add m2 to m1
      if (m2->x1 < m1->x1) m1->x1 = m2->x1;
      if (m2->y1 < m1->y1) m1->y1 = m2->y1;
      if (m2->x2 > m1->x2) m1->x2 = m2->x2;
      if (m2->y2 > m1->y2) m1->y2 = m2->y2;
      
      g_array_remove_index(dest, i - 1);
    }
  }
  
  g_free(labels);
  g_free(to_join);
}

GArray* layout_fetch(layout_t *layout)
{
  GArray* result = g_array_new(FALSE, FALSE, sizeof(marker_t));
  
  filter_crcs(layout);
  find_markers(layout, result);
  
  return result;
}

char* layout_read_markers(GArray* markers, const uint8_t *frame, int stride)
{
  char *result = g_malloc0(markers->len + 1);
  const char lookup[] = "krgybmcw";
  size_t i;
  
  for (i = 0; i < markers->len; i++)
  {
    marker_t *marker = &g_array_index(markers, marker_t, i);
    int x = (marker->x1 + marker->x2) / 2;
    int y = (marker->y1 + marker->y2) / 2;
    uint8_t color = 0;
    
    if (frame[y * stride + x * 4 + 0] > TVG_COLOR_THRESHOLD) color |= 1;
    if (frame[y * stride + x * 4 + 1] > TVG_COLOR_THRESHOLD) color |= 2;
    if (frame[y * stride + x * 4 + 2] > TVG_COLOR_THRESHOLD) color |= 4;
    
    result[i] = lookup[color];
  }
  
  return result;
}
