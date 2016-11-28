/* global variable and structure header for gluas
 * Copyright (C) 2004  Øyvind Kolås <pipipn@gimp.org>
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of the Author of the
 * Software shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization from the Author.
 */

#ifndef __MAIN_H__
#define __MAIN_H__

#ifndef PATH_MAX
#define PATH_MAX         _MAX_PATH
#endif

#define SCRIPT_MAX_LEN  65535

#include "config.h"

#define GLUAS_VERSION_STRING PACKAGE_VERSION

typedef struct
{
  gchar file[PATH_MAX];
  gint    frames;
  gdouble user_value;
} PlugInVals;

typedef struct
{
  gint32    image_id;
} PlugInImageVals;

typedef struct
{
  gint32    drawable_id;
} PlugInDrawableVals;

typedef struct
{
  gboolean properties_ui_visible;
  gchar    script[SCRIPT_MAX_LEN];
  gchar    file[PATH_MAX];
} PlugInUIVals;


/*  Default values  */

extern const PlugInVals         default_vals;
extern const PlugInImageVals    default_image_vals;
extern const PlugInDrawableVals default_drawable_vals;
extern const PlugInUIVals       default_ui_vals;

#define GLUAS_DEFAULT_SCRIPT \
"-- threshold filter\n"\
"\n"\
"level = 0.5\n"\
"\n"\
"for y=0, height-1 do\n"\
"\tfor x=0, width-1 do\n"\
"\t\tv = get_value (x,y)\n"\
"\t\tif v>level then\n"\
"\t\t\tv=1.0\n"\
"\t\telse\n"\
"\t\t\tv=0.0\n"\
"\t\tend\n"\
"\t\tset_value (x,y,v)\n"\
"\tend\n"\
"\tprogress (y/height)\n"\
"end\n"\
"\n"\
"-- gluas functions:\n"\
"-- ================\n"\
"--\n"\
"-- pixel setters and getters:\n"\
"--\n"\
"-- set_hsl (x,y,h,s,l)\n"\
"-- set_hsv (x,y,h,s,v)\n"\
"-- set_lab (x,y,l,a,b)\n"\
"-- set_rgb (x,y,r,g,b)\n"\
"-- set_rgba (x,y,r,g,b,a)\n"\
"-- set_alpha (x,y,alpha)\n"\
"-- set_value (x,y,value)\n"\
"-- a       = get_alpha (x,y)\n"\
"-- v       = get_value (x,y)\n"\
"-- h,s,l   = get_hsl   (x,y)\n"\
"-- h,s,v   = get_hsv   (x,y)\n"\
"-- l,a,b   = get_lab   (x,y)\n"\
"-- r,g,b   = get_rgb   (x,y)\n"\
"-- r,g,b,a = get_rgba  (x,y)\n"\
"--\n"\
"-- special functions:\n"\
"--\n"\
"-- flush() - commit changes to image being processed\n"\
"-- process(ratio) - sets progress bar,. 0.0 = none 1.0 = full\n"

#endif /* __MAIN_H__ */
