/* in image preview helping code
 *
 * for the gluas plug-in 
 * Copyright (C) 2004 Øyvind Kolås <pippin@gimp.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include <stdio.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "preview_layer.h"

static gboolean original_visible = FALSE;

static gint32 preview_layer_ID  = -1;   /*< when -1, no preview */
static gint32 active_layer;
static gint32 src_image_ID;


/* return the position of the provided layer in the provided
 * image, 0 is the topmost layer
 */
gint
layer_position (gint32 image_ID,
                gint32 layer_ID)
{
  gint  position;
  gint *layers;
  gint  num_layers;
  gint *iter;

  layers = gimp_image_get_layers (image_ID, &num_layers);

  if (!layers)
    return 0;
  
  iter     = layers;
  position = 0;
  
  while (*iter && position < num_layers)
    {
      if (*iter == layer_ID)
        break;
      position++;
      iter++;
    }

  if (layers)
    g_free (layers);

  return position;
}

/*
 * create a preview layer for 'layer', layer is duplicated, mask mode and
 * transparency included, and insert right above the original layer, the
 * original layer's visibility is turned off to make the composited image
 * look correct as well.
 *
 * subsequent calls to preview_layer_create will remove the previous preview
 * layer and add a new one, thus facilitating successive rerenders on original
 * image data.
 */
gint32
preview_layer_create (gint32 image_ID, gint32 layer)
{
  if (preview_layer_ID != -1)
    preview_layer_destroy (image_ID);
  
  active_layer = layer;
  src_image_ID = image_ID;
  
  if (!active_layer)
    return -1;

  preview_layer_ID = gimp_layer_copy (active_layer);

  gimp_image_add_layer (image_ID, preview_layer_ID, layer_position (image_ID, layer));
  gimp_drawable_set_name (preview_layer_ID, "gluas preview");

  original_visible = gimp_drawable_get_visible (active_layer);
  gimp_drawable_set_visible (active_layer,  FALSE);
  gimp_drawable_set_visible (preview_layer_ID, TRUE);


  
  return preview_layer_ID;
}

/*
 * destroy preview layer, this returns the original visibility of
 * the layer used to create the preview layer
 */
void
preview_layer_destroy (void)
{
  if (preview_layer_ID != -1)
    {
      GimpDrawable *drawable;

      drawable = gimp_drawable_get (preview_layer_ID);

      gimp_drawable_detach (drawable);
      gimp_image_remove_layer (src_image_ID, preview_layer_ID);
      preview_layer_ID = -1;
      gimp_drawable_set_visible (active_layer, original_visible);
    }
}
