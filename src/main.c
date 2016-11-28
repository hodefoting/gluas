/* gluas entry point
 *
 * Copyright (C) 2004 Øyvind Kolås <pippin@users.sf.net>
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
#include <stdlib.h>
#include <string.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "main.h"
#include "render.h"
#include "interface.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define PROCEDURE_NAME    "gluas"
#define DATA_KEY_VALS     "gluas"
#define DATA_KEY_UI_VALS  "gluas_ui"

/* Some useful macros */
#ifndef PATH_MAX
#ifdef _MAX_PATH
#define PATH_MAX         _MAX_PATH
#else
#define PATH_MAX         512
#endif
#endif


#define TILE_CACHE_SIZE   16
#define SCALE_WIDTH      125
#define ENTRY_WIDTH       50


/* Declare local functions.
 */
static void query (void);
static void run (const gchar      *name,
                 gint              nparams,
                 const GimpParam  *param,
                 gint             *nreturn_vals,
                 GimpParam       **return_vals);

/***** Local variables *****/

const PlugInVals default_vals =
{
  ""
};

const PlugInImageVals default_image_vals =
{
  0
};

const PlugInDrawableVals default_drawable_vals =
{
  0
};

const PlugInUIVals default_ui_vals =
{
  FALSE,
  GLUAS_DEFAULT_SCRIPT,
  ""
};

static PlugInVals vals;
static PlugInImageVals image_vals;
static PlugInDrawableVals drawable_vals;
static PlugInUIVals ui_vals;

GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,                         /* init_proc  */
  NULL,                         /* quit_proc  */
  query,                        /* query_proc */
  run,                          /* run_proc   */
};


/***** Functions *****/

MAIN ()

static void query (void)
{
  static GimpParamDef args[] =
  {
    {GIMP_PDB_INT32,    "run_mode", "Interactive, non-interactive"},
    {GIMP_PDB_IMAGE,    "image",    "Input image (unused)"},
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable"},
    {GIMP_PDB_STRING,   "script",   "the lua script file to execute"}
  };

  gimp_install_procedure ("plug_in_gluas",
                          "Use lua for image processing the current drawable",
                          "Use lua for image processing the current drawable",
                          "OEyvind Kolaas <pippin@gimp.org>",
                          "OEyvind Kolaas <pippin@gimp.org>",
                          "2003-2004",
                          "<Image>/Filters/Generic/gluas...",
                          "RGB*",
                          GIMP_PLUGIN, G_N_ELEMENTS (args), 0, args, NULL);
}

static void
run (const gchar      *name,
     gint              nparams,
     const GimpParam  *param,
     gint             *nreturn_vals,
     GimpParam       **return_vals)
{
  static GimpParam values[1];
  GimpDrawable *drawable;
  gint32    image_ID;
  GimpRunMode run_mode;
  GimpPDBStatusType status = GIMP_PDB_SUCCESS;
  gint32    active_layer = -1;

  *nreturn_vals = 1;
  *return_vals = values;

  run_mode = param[0].data.d_int32;
  image_ID = param[1].data.d_int32;
  drawable = gimp_drawable_get (param[2].data.d_drawable);

  vals = default_vals;
  image_vals = default_image_vals;
  drawable_vals = default_drawable_vals;
  ui_vals = default_ui_vals;

  active_layer = gimp_image_get_active_layer (image_ID);

  switch (run_mode)
    {
    case GIMP_RUN_NONINTERACTIVE:
      /*  Make sure all the arguments are there!  */
      if (nparams != 4)
        {
          status = GIMP_PDB_CALLING_ERROR;
        }
      if (status == GIMP_PDB_SUCCESS)
        {
          strncpy (vals.file, param[3].data.d_string, PATH_MAX);
        }
      break;
    case GIMP_RUN_INTERACTIVE:
      /*  Possibly retrieve data  */
      gimp_get_data (DATA_KEY_VALS, &vals);
      gimp_get_data (DATA_KEY_UI_VALS, &ui_vals);

      /*  First acquire information with a dialog  */

      gimp_image_undo_freeze (image_ID);

      if (!dialog (image_ID, drawable,
                   &vals, &image_vals, &drawable_vals, &ui_vals))
        {
          status = GIMP_PDB_CANCEL;
        }

      gimp_image_undo_thaw (image_ID);
      break;

    case GIMP_RUN_WITH_LAST_VALS:
      /*  Possibly retrieve data  */
      gimp_get_data (DATA_KEY_VALS, &vals);
      break;

    default:
      break;
    }

  if (status == GIMP_PDB_SUCCESS)
    {
      /*  Make sure that the drawable is RGB color or grayscale */

      if (gimp_drawable_is_rgb (drawable->drawable_id) ||
          gimp_drawable_is_gray (drawable->drawable_id))
        {

          gimp_image_undo_group_start (image_ID);

          /* FIXME: animation */

          if (vals.frames == 0)
            {
              if (run_mode == GIMP_RUN_INTERACTIVE)
                drawable_lua_do_buffer (drawable, ui_vals.script, vals.user_value);
              else
                drawable_lua_do_file (drawable, vals.file, vals.user_value);
            }
          else
            {
              gint      frame;
              gdouble   user_value;

              for (frame = 0; frame < vals.frames; frame++)
                {
                  GimpDrawable *drawable;
                  gint32    layer_id;

                  layer_id = gimp_layer_copy (active_layer);
                  gimp_image_add_layer (image_ID, layer_id, -1);
                  drawable = gimp_drawable_get (layer_id);

                  if (!gimp_drawable_has_alpha (drawable->drawable_id))
                    gimp_layer_add_alpha (layer_id);
                  drawable = gimp_drawable_get (layer_id);

                  user_value = (gdouble) frame / vals.frames;

                  if (run_mode == GIMP_RUN_INTERACTIVE)
                    drawable_lua_do_buffer (drawable, ui_vals.script, user_value);
                  else
                    drawable_lua_do_file (drawable, vals.file, user_value);
                }
            }


          gimp_image_undo_group_end (image_ID);

          if (run_mode != GIMP_RUN_NONINTERACTIVE)
            gimp_displays_flush ();

          /*  Store data  */
          if (run_mode == GIMP_RUN_INTERACTIVE)
            {
              gimp_set_data (DATA_KEY_VALS, &vals, sizeof (vals));
              gimp_set_data (DATA_KEY_UI_VALS, &ui_vals, sizeof (ui_vals));
            }

          gimp_drawable_detach (drawable);
        }
      else
        {
          status = GIMP_PDB_EXECUTION_ERROR;
        }
    }

  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = status;

}
