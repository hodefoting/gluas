/* user interface, and interactive mode control code for gluas
 *
 * gluas plug-in 
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

#include <string.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "main.h"
#include "interface.h"

#include "render.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define SCALE_WIDTH 180
#define SPIN_BUTTON_WIDTH 75

#ifdef USE_GTKSOURCEVIEW
#include <gtksourceview/gtksourcebuffer.h>
#include <gtksourceview/gtksourceview.h>
#include <gtksourceview/gtksourcelanguage.h>
#include <gtksourceview/gtksourcelanguagesmanager.h>

#endif


#include "preview_layer.h"


/* backwards compatibility with gimp 2.0, doesn't look as good,
 * but it works
 */

# if GIMP_CHECK_VERSION(2,2,0)
/* ok, we have gimp_frame_new */
# else
#define gimp_frame_new(label) gtk_frame_new((label))
# endif


/* Some useful macros */

#define HELP_ID "plug-in-gluas"

#define TILE_CACHE_SIZE   16
#define ENTRY_WIDTH       30

static gint32 active_layer = -1;


static GtkWidget     *script_view   = NULL;
static GtkTextBuffer *script_buffer = NULL;

static PlugInUIVals *ui_state     = NULL;
static PlugInVals   *state        = NULL;
static GimpDrawable *src_drawable = NULL;
static gint32        src_image_ID = 0;

static GtkWidget    *toplevel        = NULL;
static GtkWidget    *properties_frame = NULL;
static GtkWidget    *error_frame     = NULL;
static GtkWidget    *error_label     = NULL;
static GtkWidget    *print_frame     = NULL;
static GtkWidget    *print_label     = NULL;


static void    help_cb    (GtkAction  *action,
                           gpointer    data);
static void    new_cb     (GtkAction  *action,
                           gpointer    data);
static void    open_cb    (GtkAction  *action,
                           PlugInVals *vals);
static void    save_cb    (GtkAction  *action,
                           PlugInVals *vals);
static void    preview_cb (GtkAction *action,
                           gpointer   data);
static void    user_value_changed_cb (GtkAction *action,
                                      gpointer   data);

static void    properties_visible_cb (GtkAction *action,
                                     gpointer   data);

static GtkUIManager *create_ui_manager (GtkWidget  *window,
                                        PlugInVals *vals);

void
gluas_error (const gchar *message)
{

  /* no error_label, means running in non interactive mode, perhaps from a script,
   * let's spew a "compile error"
   */
  if (!error_label)
    {
      fprintf (stderr, "%s\n", message);
      return;
    }

  if (message && error_label)
    {
      int line_no;
      char buf[4096]="fnord";

      if (strstr(message,":")) {
         GtkTextIter iter;

         snprintf (buf, 4096, "Line %s", strstr(message,":")+1);
         line_no = atoi (strstr(message,":")+1);

         gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (script_buffer), &iter, line_no-1);
         gtk_text_buffer_place_cursor (GTK_TEXT_BUFFER (script_buffer), &iter);
         gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (script_view), &iter, 0.1, FALSE, 0, 0);

      }

      gtk_label_set_label (GTK_LABEL (error_label), buf);
      gtk_widget_show (error_frame);


      
    }
  else
    {
      gtk_widget_hide (error_frame);
    }
}

void
gluas_print (const gchar *message)
{

  /* no print_label, means running in non interactive mode, perhaps from a script,
   * let's spew to stdout instead of our own container,.
   */
  if (!print_label)
    {
      fprintf (stdout, "%s\n", message);
      return;
    }

  if (message && print_label)
    {
      gtk_label_set_label (GTK_LABEL (print_label), message);
      gtk_widget_show (print_frame);
    }
  else
    {
      gtk_widget_hide (print_frame);
    }
}




gboolean
cb_destroy_event (GtkWidget *dlg, gpointer data)
{
  return TRUE;
}

gboolean
dialog (gint32              image_ID,
        GimpDrawable       *drawable,
        PlugInVals         *vals,
        PlugInImageVals    *image_vals,
        PlugInDrawableVals *drawable_vals,
        PlugInUIVals       *ui_vals)
{
  GtkWidget      *dlg;
  GtkWidget      *vbox;
  GtkUIManager   *ui_manager;
  GtkWidget      *toolbar;
  GtkWidget      *scroll;

  gboolean        run;
  
  ui_state       = ui_vals;
  state          = vals;
  src_image_ID   = image_ID;
  src_drawable   = drawable;

  gimp_ui_init ("gluas", FALSE);

  /* using a gtk_dialog, to make it easier to remove ESC' cancelling
   * effect, useful for vim users
   */
  dlg = gtk_dialog_new_with_buttons ("gluas",
                          NULL, 
                          GTK_DIALOG_DESTROY_WITH_PARENT,

                          GTK_STOCK_CANCEL, GTK_RESPONSE_OK+100,  /*< evil hack */
                          GTK_STOCK_OK,     GTK_RESPONSE_OK,
                          NULL);

  /* store reference to dialog, in global variable used by
   * file open/save callbacks to know which window they are
   * transients for
   */ 
  toplevel = dlg;
  vbox     = GTK_DIALOG (dlg)->vbox;
  
  /* buttons / toolbar */

  ui_manager = create_ui_manager (dlg, vals);
  toolbar    = gtk_ui_manager_get_widget (ui_manager, "/gluas-toolbar");
  gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_ICONS);
  gtk_box_pack_start (GTK_BOX (vbox),
                      toolbar, FALSE, FALSE, 0);

  /* Properties expander */
  properties_frame = gimp_frame_new ("Properties");

  gtk_box_pack_start (GTK_BOX (vbox), properties_frame, FALSE, FALSE, 0);

  {
    GtkWidget   *vbox;
    GtkWidget   *table;
    GtkObject   *adj;
    gint         row = 0;

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (properties_frame), vbox);
    
    table = gtk_table_new (3, 3, FALSE);
    gtk_container_set_border_width (GTK_CONTAINER (table), 4);
    gtk_table_set_col_spacings (GTK_TABLE (table), 4);
    gtk_table_set_row_spacings (GTK_TABLE (table), 2);

    adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row++,
          "User value", SCALE_WIDTH, SPIN_BUTTON_WIDTH,
          vals->user_value, -1.0, 1.0, 0.01, 0.1, 3,
          FALSE, -1000, 1000,
          "A user controlable value, accessible from lua as the global variable 'user_value'", NULL);
    g_signal_connect (adj, "value_changed",
          G_CALLBACK (gimp_double_adjustment_update),
          &vals->user_value);

    g_signal_connect (adj, "value_changed",
          G_CALLBACK (user_value_changed_cb),
          &vals->frames);
    gtk_range_set_update_policy (GIMP_SCALE_ENTRY_SCALE (adj), GTK_UPDATE_DISCONTINUOUS);

    adj = gimp_scale_entry_new (GTK_TABLE (table), 0, row++,
          "Renderings", SCALE_WIDTH, SPIN_BUTTON_WIDTH,
          vals->frames, 0, 100, 1, 10, 0,
          FALSE, 0, 1000,
"Number of layers to render, 0 means modify original layer in place. "
"When rendering multiple layers, the user value will step from 0.0 to 1.0."
"This can be used to create animations, suitable for saving from The GIMP."
          , NULL);
    g_signal_connect (adj, "value_changed",
          G_CALLBACK (gimp_int_adjustment_update),
          &vals->frames);

    gtk_box_pack_start_defaults (GTK_BOX (vbox), table);

    if (0){
    GtkWidget *button;

    button = gtk_check_button_new_with_mnemonic ("preview");

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);

    gtk_box_pack_start_defaults (GTK_BOX (vbox), button);
    }
    
  }

  /* error frame */

  error_frame    = gimp_frame_new ("Problem running program");
  error_label    = gtk_label_new ("");

  gtk_label_set_selectable (GTK_LABEL (error_label), TRUE);
  gtk_misc_set_alignment (GTK_MISC (error_label), 0,0);

  gtk_container_add (GTK_CONTAINER (error_frame), error_label);
  gtk_box_pack_start (GTK_BOX (vbox), error_frame, FALSE, FALSE, 0);

  /* print frame */

  print_frame    = gimp_frame_new ("Output");
  print_label    = gtk_label_new ("");

  gtk_label_set_selectable (GTK_LABEL (print_label), TRUE);
  gtk_misc_set_alignment (GTK_MISC (print_label), 0,0);

  gtk_container_add (GTK_CONTAINER (print_frame), print_label);
  gtk_box_pack_start (GTK_BOX (vbox), print_frame, FALSE, FALSE, 0);

  /* editor */

  scroll        = gtk_scrolled_window_new (NULL, NULL);
#ifdef USE_GTKSOURCEVIEW
  {
    GtkSourceLanguagesManager *lm;
    GtkSourceLanguage         *lang;
    
    lm = gtk_source_languages_manager_new ();
    lang = gtk_source_languages_manager_get_language_from_mime_type (lm, "text/x-lua");

    script_view   = gtk_source_view_new ();
    script_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (script_view));

    if (lang)
      {
        gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (script_buffer), lang);
        gtk_source_buffer_set_highlight (GTK_SOURCE_BUFFER (script_buffer), TRUE);
      }

    gtk_source_view_set_show_line_numbers (GTK_SOURCE_VIEW (script_view), TRUE);
    gtk_source_view_set_auto_indent       (GTK_SOURCE_VIEW (script_view), TRUE);
    gtk_source_view_set_tabs_width        (GTK_SOURCE_VIEW (script_view), 4);
  }
#else
  script_view   = gtk_text_view_new ();
  script_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (script_view));
#endif

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  gtk_text_buffer_set_text (script_buffer, ui_vals->script, -1);
  gtk_container_add (GTK_CONTAINER (scroll), script_view);

  gtk_widget_set_size_request (scroll, 420, 256);

  gtk_box_pack_start (GTK_BOX (vbox), scroll, TRUE, TRUE, 0);

  {
    PangoFontDescription *font = pango_font_description_new ();
    pango_font_description_set_family (font, "monospace");
    gtk_widget_modify_font (script_view, font);
  }

  /* */

  gtk_widget_show_all (vbox);

  gtk_widget_hide (error_frame);
  gtk_widget_hide (print_frame);

  if (!ui_state->properties_ui_visible)
    gtk_widget_hide (properties_frame);

  /* we retrieve the active layer before running the dialog, since the user
   * might change it while our ui is running
   */
  active_layer = gimp_image_get_active_layer (src_image_ID);

  run = (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK);

  /* make the global value for the error label NULL, to make sure the error
   * handler doesn't reference freed memory
   */
  error_frame = NULL;
  error_label = NULL;

  print_frame = NULL;
  print_label = NULL;

  if (run)
    {
      /* save ui state */

      gchar       *script;
      GtkTextIter  start, end;

      gtk_text_buffer_get_bounds (script_buffer, &start, &end);
      script = gtk_text_buffer_get_text (script_buffer, &start, &end, FALSE);

      if (script)
        {
          strncpy (ui_state->script, script, SCRIPT_MAX_LEN);
          g_free (script);
        }
    }

  gtk_widget_destroy (dlg);
  
  preview_layer_destroy ();
  gimp_displays_flush ();  /* update ui state, removing preview drawable */

  return run;
}

static GtkUIManager *
create_ui_manager (GtkWidget *window, PlugInVals *vals)
{
  static GtkActionEntry actions[] =
  {
    { "gluas-menu", NULL, "gluas dummy menu"},

    { "gluas-help", GTK_STOCK_HELP,
      NULL, "F1",
      "Help (F1)",
      G_CALLBACK (help_cb) },

    { "gluas-new", GTK_STOCK_NEW,
      NULL, "<control>N",
      "New script (Ctrl+N)",
      G_CALLBACK (new_cb) },

     { "gluas-open", GTK_STOCK_OPEN,
      NULL, "<control>O",
      "Open an a script from file. (Ctrl+O)",
      G_CALLBACK (open_cb) },

     { "gluas-save", GTK_STOCK_SAVE,
      NULL, "<control>S",
      "Save current script. (Ctrl+S)",
      G_CALLBACK (save_cb) },

     { "gluas-preview", GTK_STOCK_EXECUTE,
      "Render", "F5",
      "Run a test render. (F5)",
      G_CALLBACK (preview_cb) }
  };
  static GtkToggleActionEntry toggle_actions[]=
  {
    {"gluas-properties-visible", GTK_STOCK_PROPERTIES,
    "Properties", "F6",
    "Toggle visibility of properties dialog (F6)",
    G_CALLBACK (properties_visible_cb),
    FALSE}
  };

  GtkUIManager   *ui_manager = gtk_ui_manager_new ();
  GtkActionGroup *group      = gtk_action_group_new ("Actions");

  gtk_action_group_set_translation_domain (group, NULL);

  gtk_action_group_add_actions (group,
                                actions,
                                G_N_ELEMENTS (actions),
                                vals);
  gtk_action_group_add_toggle_actions (group,
                                       toggle_actions,
                                       G_N_ELEMENTS (toggle_actions),
                                       vals);

  gtk_window_add_accel_group (GTK_WINDOW (window),
                              gtk_ui_manager_get_accel_group (ui_manager));

  gtk_ui_manager_insert_action_group (ui_manager, group, -1);
  gtk_accel_group_lock (gtk_ui_manager_get_accel_group (ui_manager));

  g_object_unref (group);

  gtk_ui_manager_add_ui_from_string
    (ui_manager,
     "<ui>"
     "  <menubar name='dummy-menubar'>"
     "   <menu action='gluas-menu'>"
     "    <menuitem action='gluas-new' />"
     "    <menuitem action='gluas-open' />"
     "    <menuitem action='gluas-save' />"
     "    <menuitem action='gluas-preview' />"
     "    <menuitem action='gluas-properties-visible' />"
     "   </menu>"
     "  </menubar>"
     "</ui>",
     -1, NULL);


  gtk_ui_manager_add_ui_from_string
    (ui_manager,
     "<ui>"
     "  <toolbar name='gluas-toolbar'>"
     "    <toolitem action='gluas-help' />"
     "    <toolitem action='gluas-new' />"
     "    <toolitem action='gluas-open' />"
     "    <toolitem action='gluas-save' />"
     "    <toolitem action='gluas-preview' />"
     "    <toolitem action='gluas-properties-visible' />"
     "  </toolbar>"
     "</ui>",
     -1, NULL);

  return ui_manager;
}

static void
help_cb (GtkAction  *action,
         gpointer    data)
{
  GtkWidget *dialog;
  
  gchar *authors[] =
     {
      "Programming:\n\tØyvind Kolås <oeyvindk@hig.no>",
      "win32 binaries:\n\tMichael Schumacher <schumaml@gmx.de>",
      NULL
     };

  dialog = gtk_about_dialog_new ();
  g_object_set (G_OBJECT (dialog),
   "name",          "gluas",
   "authors",       authors,
   "version",       GLUAS_VERSION_STRING,
   "website",       "http://pippin.gimp.org/plug-ins/gluas/",
   "website-label", "Gluas homepage and user guide.",
   "comments",      "A gimp and lua based image processing prototyping framework.",
   "copyright",     "Øyvind Kolås © 2005",
   NULL);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK);
}

static void
new_cb (GtkAction  *action,
        gpointer    data)
{
  gtk_text_buffer_set_text (script_buffer, GLUAS_DEFAULT_SCRIPT, -1);
}

static void
open_cb (GtkAction  *action,
         PlugInVals *vals)
{
  GtkWidget  *dialog;

  dialog = gtk_file_chooser_dialog_new ("Load lua script",
                                        GTK_WINDOW (toplevel),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,

                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OPEN,   GTK_RESPONSE_OK,

                                        NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  if (vals->file[0]!='\0')
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog), vals->file);
  
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
      gchar *buf;

      strncpy (vals->file,
               gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog)),
               PATH_MAX
              );

      g_file_get_contents (vals->file, &buf, NULL, NULL);

      if (buf)
        {
          gtk_text_buffer_set_text (script_buffer, buf, -1);
          g_free (buf);
        }
    }
  gtk_widget_destroy (dialog);
}

static void
save_cb (GtkAction  *action,
         PlugInVals *vals)
{
  GtkWidget  *dialog;

  dialog = gtk_file_chooser_dialog_new ("Save current lua script",
                                        GTK_WINDOW (toplevel),
                                        GTK_FILE_CHOOSER_ACTION_SAVE,

                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_SAVE,   GTK_RESPONSE_OK,

                                        NULL);
  
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  if (vals->file[0]!='\0')
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog), vals->file);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
      gchar *buf;

      strncpy (vals->file,
               gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog)),
               PATH_MAX
              );

      {
        FILE *file = fopen (vals->file, "w");
        if (file)
          {
            GtkTextIter start, end;
            gchar       *buffer;

            gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (script_buffer), &start, &end);
            buffer = gtk_text_buffer_get_text (GTK_TEXT_BUFFER (script_buffer), &start, &end, FALSE);
  
            if (buffer)
              {
                fwrite (buffer, 1, strlen (buffer), file);
                g_free (buffer);
              }

            fclose (file);
          }
        else
          {
            g_message ("unable to open file for writing");
          }
      }
      g_file_get_contents (vals->file, &buf, NULL, NULL);

      if (buf)
        {
          gtk_text_buffer_set_text (script_buffer, buf, -1);
          g_free (buf);
        }
    }
  gtk_widget_destroy (dialog);
}

static void
preview_cb (GtkAction *action,
            gpointer   data)
{
  GtkTextIter  start, end;
  gchar        *buffer;
  gint32        preview_layer;
  GimpDrawable *preview_drawable;

  preview_layer    = preview_layer_create (src_image_ID, active_layer);
  preview_drawable = gimp_drawable_get (preview_layer);

  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (script_buffer), &start, &end);
  buffer = gtk_text_buffer_get_text (GTK_TEXT_BUFFER (script_buffer), &start, &end, FALSE);
  
  if (buffer)
    {
      gluas_print (NULL);
      drawable_lua_do_buffer (preview_drawable, buffer, state->user_value);
      g_free (buffer);
    }

  gimp_displays_flush ();
}


static void
user_value_changed_cb (GtkAction *action,
                       gpointer   data)
{
  preview_cb (NULL, NULL);
}


static void    properties_visible_cb (GtkAction *action,
                                     gpointer   data)
{
  gboolean visible;

  visible = !ui_state->properties_ui_visible;

  if (visible)
    gtk_widget_show (properties_frame);
  else
    gtk_widget_hide (properties_frame);
  
  ui_state->properties_ui_visible = visible;
}
