#include "layout.h"
#include <stdbool.h>
#include <zlib.h>
#include <glib.h>
#include <gst/gst.h>
#include <string.h>
#include <stdlib.h>

GST_DEBUG_CATEGORY_EXTERN(tvg_analyzer_debug);
#define GST_CAT_DEFAULT tvg_analyzer_debug

struct _layout_t
{
  int width;
  int height;
  
  /* Pixel mask of the pixels that are good so far. */
  uint8_t *good_mask;
  
  /* Pixel mask of the pixels that are black/white. */
  uint8_t *blackwhite;
  
  /* Minimum and maximum values of each color component. */
  uint8_t *min_values;
  uint8_t *max_values;
  
  /* Pixel color checksums to detect connected areas. */
  uint32_t *pixel_crcs;
  
  /* Pixel color after the previous large change. */
  uint32_t *pixel_colors;
  
  /* Sum of total pixel color changes */
  uint32_t *pixel_prevcolor;
  uint32_t *pixel_changesum;
};

layout_t *layout_create(int width, int height)
{
  layout_t *layout = g_malloc0(sizeof(layout_t));
  
  layout->width = width;
  layout->height = height;
  
  layout->good_mask = g_malloc(width * height);
  layout->blackwhite = g_malloc(width * height);
  layout->min_values = g_malloc(width * height * 4);
  layout->max_values = g_malloc(width * height * 4);
  layout->pixel_crcs = g_malloc(width * height * 4);
  layout->pixel_colors = g_malloc(width * height * 4);
  layout->pixel_prevcolor = g_malloc(width * height * 4);
  layout->pixel_changesum = g_malloc(width * height * 4);
  
  memset(layout->good_mask, 1, width * height);
  memset(layout->blackwhite, 1, width * height);
  memset(layout->min_values, 255, width * height * 4);
  memset(layout->max_values, 0, width * height * 4);
  memset(layout->pixel_crcs, 1, width * height * 4);
  memset(layout->pixel_colors, 0, width * height * 4);
  memset(layout->pixel_prevcolor, 0, width * height * 4);
  memset(layout->pixel_changesum, 0, width * height * 4);
  
  return layout;
}

void layout_free(layout_t *layout)
{
  g_free(layout->good_mask);  layout->good_mask = NULL;
  g_free(layout->blackwhite);  layout->blackwhite = NULL;
  g_free(layout->min_values); layout->min_values = NULL;
  g_free(layout->max_values); layout->max_values = NULL;
  g_free(layout->pixel_crcs); layout->pixel_crcs = NULL;
  g_free(layout->pixel_prevcolor); layout->pixel_prevcolor = NULL;
  g_free(layout->pixel_changesum); layout->pixel_changesum = NULL;
  g_free(layout);
}

void layout_process(layout_t *layout, const uint8_t *frame, int stride)
{
  int x, y, c;
  for (y = 0; y < layout->height; y++)
  {
    for (x = 0; x < layout->width; x++)
    {
      /* Don't recheck pixels that have already been ruled out. */
      int index_pixel = y * layout->width + x;
      if (layout->good_mask[index_pixel])
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
        
        {
          int r = frame[y * stride + x * 4 + 0];
          int g = frame[y * stride + x * 4 + 1];
          int b = frame[y * stride + x * 4 + 2];
          uint8_t color = 0;
          if (r > TVG_COLOR_THRESHOLD) color |= 1;
          if (g > TVG_COLOR_THRESHOLD) color |= 2;
          if (b > TVG_COLOR_THRESHOLD) color |= 4;
          
          if (min > 5 && max < 250)
          {
            GST_DEBUG("Marking %d,%d as non-saturated: color #%02x%02x%02x\n",
                      x, y, r, g, b);
            
            /* This pixel wasn't fully saturated. */
            layout->good_mask[index_pixel] = 0;
          }
          
          /* Update CRC32 of the pixel in order to detect joined areas.
           * Uses only the saturated pixel value in order to be tolerant of
           * lossy compression. */
          {
            layout->pixel_crcs[index_pixel] = crc32(layout->pixel_crcs[index_pixel],
                                                    &color, 1);
            
            if (color != 0 && color != 7)
            {
              layout->blackwhite[index_pixel] = 0;
            }
          }
          
          /* Check if the pixel value has changed */
          {
            int old_value = layout->pixel_colors[index_pixel];
            int new_value = (color << 24) | (r << 16) | (g << 8) | b;
            
            if ((old_value >> 24) != (new_value >> 24))
            {
              /* Ok, large change, update value */
              layout->pixel_colors[index_pixel] = new_value;
            }
            else
            {
              /* There shouldn't be much change in the value */
              int delta_r = abs(r - ((old_value >> 16) & 0xFF));
              int delta_g = abs(g - ((old_value >>  8) & 0xFF));
              int delta_b = abs(b - ((old_value >>  0) & 0xFF));
              if (delta_r + delta_g + delta_b > 32)
              {
                GST_DEBUG("Marking %d,%d as slowly changing: old %08x, new %08x\n",
                      x, y, old_value, new_value);
                
                layout->good_mask[index_pixel] = 0;
              }
            }
          }
        }
      }
      
      /* Keep track of the most changing pixel (for use by camera fps) */
      {
        int r = frame[y * stride + x * 4 + 0];
        int g = frame[y * stride + x * 4 + 1];
        int b = frame[y * stride + x * 4 + 2];
        uint32_t newcolor = (r << 16) | (g << 8) | b;
        uint32_t oldcolor = layout->pixel_prevcolor[index_pixel];
        int oldr = (oldcolor >> 16) & 0xFF;
        int oldg = (oldcolor >> 8) & 0xFF;
        int oldb = (oldcolor >> 0) & 0xFF;
        int change = abs(r-oldr) + abs(g-oldg) + abs(b-oldb);
        
        layout->pixel_changesum[index_pixel] += change;
        layout->pixel_prevcolor[index_pixel] = newcolor;
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
      
      if (layout->good_mask[index_pixel] == 0)
      {
        /* Not saturated */
        layout->pixel_crcs[index_pixel] = 0;
        continue;
      }
      
      /* Rule out constant valued pixels */
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
            marker_t marker = {x, y, x, y, false, 0};
            marker.is_rgb = !layout->blackwhite[index_pixel];
            marker.crc = current;
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
  
  /* Filter out any too small areas */
  for (i = dest->len - 1; i >= 0; i--)
  {
    marker_t *m = &g_array_index(dest, marker_t, i);
    if (m->x2 - m->x1 < 16 || m->y2 - m->y1 < 16)
      g_array_remove_index(dest, i);
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

void layout_most_changing_pixel(layout_t *layout, int *x, int *y)
{
  uint32_t largest = 0;
  int py, px;
  
  *x = *y = 0;
  
  for (py = 5; py < layout->height - 5; py++)
  {
    for (px = 5; px < layout->width - 5; px++)
    {
      int index_pixel = py * layout->width + px;
      if (layout->pixel_changesum[index_pixel] >= largest)
      {
        *x = px;
        *y = py;
        largest = layout->pixel_changesum[index_pixel];
      }
    }
  }
}

char* layout_sample_color(const uint8_t *frame, int stride, int x, int y)
{
  int r = 0, g = 0, b = 0;
  int count = 0;
  int py, px;
  
  for (py = y - 2; py <= y + 2; py++)
  {
    for (px = x - 2; px <= x + 2; px++)
    {
      r += frame[py * stride + px * 4 + 0];
      g += frame[py * stride + px * 4 + 1];
      b += frame[py * stride + px * 4 + 2];
      count++;
    }
  }
  
  r /= count;
  g /= count;
  b /= count;
  
  return g_strdup_printf("#%02x%02x%02x", r, g, b);
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
