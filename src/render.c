/* gluas rendering code, the actual embedded lua interpreter
 *
 * gluas plug-in 
 * Copyright (C) 2004 �yvind Kol�s <pippin@gimp.org>
 *               2016 Marco Sch�pl <schoepl@informatik.hu-berlin.de>
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
#include <libgimpcolor/gimpcolor.h>

#include "main.h"
#include "render.h"
#include "interface.h"

#include "cpercep.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define TILE_CACHE_SIZE   16
#define SCALE_WIDTH      125
#define ENTRY_WIDTH       50

static int l_set_rgba  (lua_State * lua);
static int l_get_rgba  (lua_State * lua);
static int l_set_rgb   (lua_State * lua);
static int l_get_rgb   (lua_State * lua);
static int l_set_hsl   (lua_State * lua);
static int l_get_hsl   (lua_State * lua);
static int l_set_hsv   (lua_State * lua);
static int l_get_hsv   (lua_State * lua);
static int l_set_value (lua_State * lua);
static int l_get_value (lua_State * lua);
static int l_set_alpha (lua_State * lua);
static int l_get_alpha (lua_State * lua);
static int l_set_lab   (lua_State * lua);
static int l_get_lab   (lua_State * lua);
static int l_in_width  (lua_State * lua);
static int l_in_height (lua_State * lua);

static int l_progress  (lua_State * lua);
static int l_flush     (lua_State * lua);
static int l_print     (lua_State * lua);

static const luaL_Reg gluas_functions[] =
{
    {"set_rgba",    l_set_rgba},
    {"get_rgba",    l_get_rgba},
    {"set_rgb",     l_set_rgb},
    {"get_rgb",     l_get_rgb},
    {"set_hsl",     l_set_hsl},
    {"get_hsl",     l_get_hsl},
    {"set_hsv",     l_set_hsv},
    {"get_hsv",     l_get_hsv},
    {"set_lab",     l_set_lab},
    {"get_lab",     l_get_lab},
    {"set_value",   l_set_value},
    {"get_value",   l_get_value},
    {"set_alpha",   l_set_alpha},
    {"get_alpha",   l_get_alpha},
    {"in_width",    l_in_width},
    {"in_height",   l_in_height},
    {"progress",    l_progress},
    {"flush",       l_flush},
    {"print",       l_print},
    {NULL,          NULL}
};

static void
register_functions (lua_State      *L,
                    const luaL_Reg *l)
{
	lua_getglobal (L, "_G");
#if (LUA_VERSION_NUM < 502)
	luaL_register (L, NULL, l);
#else
	luaL_setfuncs (L, l, 0);
#endif
}

void
drawable_lua_do_file (GimpDrawable *drawable,
                      const gchar  *file,
                      gdouble       user_value);

void
drawable_lua_do_buffer (GimpDrawable *drawable,
                        const gchar  *buffer,
                        gdouble       user_value);

void
render (gint32              image_ID,
        GimpDrawable       *drawable,
        PlugInVals         *vals,
        PlugInImageVals    *image_vals,
        PlugInDrawableVals *drawable_vals) 
{
  drawable_lua_do_file (drawable, vals->file, vals->user_value);
}

typedef struct Priv
{
  gint              bpp;
  GimpDrawable     *drawable;
  GimpPixelFetcher *pft;
  GimpPixelRgn      pr,dpr;

  gint              bx1, by1;
  gint              bx2, by2;    /* mask bounds */

  gint              width;
  gint              height;
  
  lua_State        *L;
}
Priv;

static void
drawable_lua_process (GimpDrawable *drawable,
                      const gchar  *file,
                      const gchar  *buffer,
                      gdouble       user_value)
{
    GimpRGB    background;
    lua_State *L;
    Priv p;

    cpercep_init_conversions ();
    gimp_progress_init ("gluas");

    /*  set the tile cache size  */
    gimp_tile_cache_ntiles (TILE_CACHE_SIZE);

    L = luaL_newstate ();
    luaL_openlibs (L);

    register_functions (L, gluas_functions);

    p.L = L;
    p.width = gimp_drawable_width (drawable->drawable_id);
    p.height = gimp_drawable_height (drawable->drawable_id);

    lua_pushnumber (L, (double) user_value);
    lua_setglobal (L, "user_value");
    lua_pushnumber (L, (double) p.width);
    lua_setglobal (L, "width");
    lua_pushnumber (L, (double) p.height);
    lua_setglobal (L, "height");

    lua_pushstring (L, "priv");
    lua_pushlightuserdata (L, &p);
    lua_settable (L, LUA_REGISTRYINDEX);

    p.drawable = drawable;
    p.bpp = gimp_drawable_bpp (drawable->drawable_id);

    p.pft = gimp_pixel_fetcher_new (drawable, FALSE);

    gimp_pixel_fetcher_set_edge_mode (p.pft, GIMP_PIXEL_FETCHER_EDGE_SMEAR);
    gimp_pixel_fetcher_set_bg_color (p.pft, &background); /* can be removed
                                                             as long as the
                                                             mode is smear.. */

    gimp_pixel_rgn_init (&(p.dpr), drawable, 0, 0, p.width, p.height, TRUE, TRUE);
    gimp_pixel_rgn_init (&(p.pr), drawable, 0, 0, p.width, p.height, FALSE, FALSE);

    gimp_drawable_mask_bounds (drawable->drawable_id, &(p.bx1), &(p.by1),
                               &(p.bx2), &(p.by2));

    
    {
      gpointer pr;
      for (pr=gimp_pixel_rgns_register (2, &(p.pr), &(p.dpr));
           pr !=NULL;
           pr = gimp_pixel_rgns_process (pr)
          )
        {
          memcpy (p.dpr.data, p.pr.data, p.dpr.rowstride *p.dpr.h);
        }
    }
   
    lua_pushnumber (L, (double) p.bx1);
    lua_setglobal (L, "bound_x0");
    lua_pushnumber (L, (double) p.bx2);
    lua_setglobal (L, "bound_x1");
    lua_pushnumber (L, (double) p.by1);
    lua_setglobal (L, "bound_y0");
    lua_pushnumber (L, (double) p.by2);
    lua_setglobal (L, "bound_y1");

    {
      gint status = 0;
   
      luaL_loadstring (L, "os.setlocale ('C', 'numeric')"); 
      
      if (file)
        status = luaL_loadfile (L, file);
      else if (buffer)
        status = luaL_loadbuffer (L, buffer, strlen (buffer), "buffer");

      if (status == 0)
        status = lua_pcall (L, 0, LUA_MULTRET, 0);
      
      gimp_drawable_flush (drawable);
      gimp_drawable_merge_shadow (drawable->drawable_id, TRUE);
      gimp_drawable_update (drawable->drawable_id, p.bx1, p.by1,
                            p.bx2 - p.bx1, p.by2 - p.by1);

      if (status != 0)
        gluas_error (lua_tostring (L, -1));
      else
        gluas_error (NULL);
    }

  gimp_pixel_fetcher_destroy (p.pft);
}

void
drawable_lua_do_file (GimpDrawable *drawable,
                      const gchar  *file,
                      gdouble       user_value)
{
  drawable_lua_process (drawable, file, NULL, user_value);
}

void
drawable_lua_do_buffer (GimpDrawable *drawable,
                        const gchar  *buffer,
                        gdouble       user_value)
{
  drawable_lua_process (drawable, NULL, buffer, user_value);
}

static void inline
get_rgba_pixel (void       *data,
                int         img_no,
                int         x,
                int         y,
                lua_Number  pixel[4])
{
  Priv *p;
  guchar byte_pixel[4];
  
  p = data;

  if (x < 0 || y < 0 || (int) x >= p->width || (int) y >= p->height)
    {
      int edge_duplicate = 0;
      lua_getglobal(p->L, "edge_duplicate");
      edge_duplicate = lua_toboolean(p->L, -1);
      lua_pop(p->L, 1);

      if (edge_duplicate)
        {
          if (x < 0)
              x = 0;
          if (y < 0)
              y = 0;
          if (x >= p->width)
              x = p->width - 1;
          if (y >= p->height)
              y = p->height - 1;
        }
      else
        {
          int i;
          for (i = 0; i < 4; i++)
            pixel[i] = 0.0;
          return;
        }
    }

  gimp_pixel_fetcher_get_pixel (p->pft, x,y, byte_pixel);

  {
    int i;
    for (i = 0; i < 4; i++)
      pixel[i] = byte_pixel[i] * (1.0/ 255.0);
  }
}

static void inline
set_rgba_pixel (void       *data,
                int         x,
                int         y,
                lua_Number  pixel[4])
{
  Priv   *p;
  gint    i;
  guchar  byte_pixel[4];

  p = data;

  if (x < p->bx1 || y < p->by1 || x > p->bx2 || y > p->by2)
      return;     /* outside selection, ignore */
  if (x < 0 || y < 0 || x >= p->width || y >= p->height)
      return;    /* out of drawable bounds */

  for (i = 0; i < 4; i++)
    {
      int val = pixel[i] * 255.0;
      if (val<0)
        val=0;
      else if (val>255)
        val=255;
      byte_pixel[i]=val;
    }
  
  gimp_pixel_rgn_set_pixel (&(p->dpr), byte_pixel, x, y);
}

static int
l_in_width (lua_State * lua)
{
  Priv *p;

  lua_pushstring(lua, "priv");
  lua_gettable(lua, LUA_REGISTRYINDEX);
  p = lua_touserdata(lua, -1);
  lua_pop(lua, 1);
  lua_pushnumber(lua, (double)p->width);
  return 1;
}

static int
l_in_height(lua_State * lua)
{
  Priv *p;

  lua_pushstring(lua, "priv");
  lua_gettable(lua, LUA_REGISTRYINDEX);
  p = lua_touserdata(lua, -1);
  lua_pop(lua, 1);
  lua_pushnumber(lua, (double)p->height);

  return 1;
}

static int
l_flush (lua_State * lua)
{
  Priv *p;

  lua_pushstring(lua, "priv");
  lua_gettable(lua, LUA_REGISTRYINDEX);
  p = lua_touserdata(lua, -1);

  gimp_drawable_flush (p->drawable);
  gimp_drawable_merge_shadow (p->drawable->drawable_id, FALSE);

  return 0;
}

static int
l_progress(lua_State * lua)
{
  lua_Number percent;

  if (!lua_gettop(lua))
    return 0;
  percent = lua_tonumber(lua, -1);

  gimp_progress_update((double) percent);
  return 0;
}

static int
l_print (lua_State * lua)
{
  if (!lua_gettop(lua))
    return 0;
  gluas_print (lua_tostring(lua, -1));

  return 0;
}


static int l_set_rgba (lua_State * lua)
{
    Priv *p;
    lua_Number pixel[4];
    lua_Number x, y;

    lua_pushstring (lua, "priv");
    lua_gettable (lua, LUA_REGISTRYINDEX);
    p = lua_touserdata (lua, -1);
    lua_pop (lua, 1);

    if (lua_gettop (lua) != 6)
      {
        lua_pushstring(lua,
                       "incorrect number of arguments to set_rgba (x, y, r, g, b, a)\n");
        lua_error (lua);
        return 0;
      }

    x        = lua_tonumber (lua, -6);
    y        = lua_tonumber (lua, -5);
    pixel[0] = lua_tonumber (lua, -4);
    pixel[1] = lua_tonumber (lua, -3);
    pixel[2] = lua_tonumber (lua, -2);
    pixel[3] = lua_tonumber (lua, -1);

    set_rgba_pixel (p, x, y, pixel);
    return 0;
}

static int l_get_rgba (lua_State * lua)
{
  Priv *p;
  lua_Number x, y;
  lua_Number pixel[4];
  lua_Number img_no = 0;

  lua_pushstring (lua, "priv");
  lua_gettable (lua, LUA_REGISTRYINDEX);
  p = lua_touserdata (lua, -1);
  lua_pop(lua, 1);

  switch (lua_gettop (lua))
    {
    case 3:
      img_no = lua_tonumber(lua, -3);
      break;
    case 2:
      img_no = 0;
      break;
    default:
      lua_pushstring (lua, "incorrect number of arguments to get_rgba (x, y)\n");
      lua_error (lua);
      break;
  }

  x = lua_tonumber(lua, -2);
  y = lua_tonumber(lua, -1);

  get_rgba_pixel (p, img_no, x, y, pixel);
  
  lua_pushnumber (lua, pixel[0]);
  lua_pushnumber (lua, pixel[1]);
  lua_pushnumber (lua, pixel[2]);
  lua_pushnumber (lua, pixel[3]);

  return 4;
}

static int l_set_rgb (lua_State * lua)
{
    Priv *p;
    lua_Number pixel[4];
    lua_Number x, y;

    lua_pushstring (lua, "priv");
    lua_gettable (lua, LUA_REGISTRYINDEX);
    p = lua_touserdata (lua, -1);
    lua_pop (lua, 1);

    if (lua_gettop (lua) != 5)
      {
        lua_pushstring(lua,
                       "incorrect number of arguments to set_rgb (x, y, r, g, b)\n");
        lua_error (lua);
        return 0;
      }

    x = lua_tonumber (lua, -5);
    y = lua_tonumber (lua, -4);

    pixel[0] = lua_tonumber (lua, -3);
    pixel[1] = lua_tonumber (lua, -2);
    pixel[2] = lua_tonumber (lua, -1);
    pixel[3] = 1.0;
    
    set_rgba_pixel (p, x, y, pixel);

    return 0;
}

static int l_get_rgb (lua_State * lua)
{
  Priv *p;
  lua_Number x, y;
  lua_Number pixel[4];
  lua_Number img_no = 0;

  lua_pushstring (lua, "priv");
  lua_gettable (lua, LUA_REGISTRYINDEX);
  p = lua_touserdata (lua, -1);
  lua_pop(lua, 1);

  switch (lua_gettop (lua))
    {
    case 3:
      img_no = lua_tonumber(lua, -3);
      break;
    case 2:
      img_no = 0;
      break;
    default:
      lua_pushstring (lua, "incorrect number of arguments to get_rgb (x, y, [, image_no])\n");
      lua_error (lua);
      break;
  }
  x = lua_tonumber(lua, -2);
  y = lua_tonumber(lua, -1);

  get_rgba_pixel (p, img_no, x, y, pixel);

  lua_pushnumber (lua, pixel[0]);
  lua_pushnumber (lua, pixel[1]);
  lua_pushnumber (lua, pixel[2]);

  return 3;
}

static int l_set_value (lua_State * lua)
{
    Priv *p;
    lua_Number x, y, v;
    lua_Number pixel[4];

    lua_pushstring (lua, "priv");
    lua_gettable (lua, LUA_REGISTRYINDEX);
    p = lua_touserdata (lua, -1);
    lua_pop (lua, 1);

    if (lua_gettop (lua) != 3)
      {
        lua_pushstring(lua,
                       "incorrect number of arguments to set_value (x, y, value)\n");
        lua_error (lua);
        return 0;
      }

    x = lua_tonumber (lua, -3);
    y = lua_tonumber (lua, -2);
    v = lua_tonumber (lua, -1);

    pixel[0] = pixel[1] = pixel[2] = v;
    pixel[3] = 1.0;

    set_rgba_pixel (p, x, y, pixel);
    return 0;
}

int l_get_value (lua_State * lua)
{
  Priv *p;
  lua_Number pixel[4];
  lua_Number x,y;
  lua_Number img_no = 0;

  lua_pushstring (lua, "priv");
  lua_gettable (lua, LUA_REGISTRYINDEX);
  p = lua_touserdata (lua, -1);
  lua_pop(lua, 1);

  switch (lua_gettop (lua))
    {
    case 3:
      img_no = lua_tonumber(lua, -3);
      break;
    case 2:
      img_no = 0;
      break;
    default:
      lua_pushstring (lua, "incorrect number of arguments to get_value (x, y [, image_no])\n");
      lua_error (lua);
      break;
  }
  x = lua_tonumber(lua, -2);
  y = lua_tonumber(lua, -1);

  get_rgba_pixel (p, img_no, x, y, pixel);

  lua_pushnumber (lua, (pixel[0]+pixel[1]+pixel[2]) * (1.0/3));

  return 1;
}

static int l_set_alpha (lua_State * lua)
{
    Priv *p;
    lua_Number pixel[4];
    lua_Number x, y, a;

    lua_pushstring (lua, "priv");
    lua_gettable (lua, LUA_REGISTRYINDEX);
    p = lua_touserdata (lua, -1);
    lua_pop (lua, 1);

    if (lua_gettop (lua) != 3)
      {
        lua_pushstring(lua,
                       "incorrect number of arguments to set_alpha (x, y, a)\n");
        lua_error (lua);
        return 0;
      }

    x = lua_tonumber (lua, -3);
    y = lua_tonumber (lua, -2);
    a = lua_tonumber (lua, -1);

    get_rgba_pixel (p, 0, x, y, pixel);
    pixel[3] = a;
    set_rgba_pixel (p, x, y, pixel);

    return 0;
}

static int l_get_alpha (lua_State * lua)
{
  Priv *p;
  lua_Number x, y;
  lua_Number pixel[4];
  lua_Number img_no = 0;

  lua_pushstring (lua, "priv");
  lua_gettable (lua, LUA_REGISTRYINDEX);
  p = lua_touserdata (lua, -1);
  lua_pop(lua, 1);

  switch (lua_gettop (lua))
    {
    case 3:
      img_no = lua_tonumber(lua, -3);
      break;
    case 2:
      img_no = 0;
      break;
    default:
      lua_pushstring (lua, "incorrect number of arguments to get_alpha (x, y [,image])\n");
      lua_error (lua);
      break;
  }
  x = lua_tonumber(lua, -2);
  y = lua_tonumber(lua, -1);

  get_rgba_pixel (p, img_no, x, y, pixel);
  lua_pushnumber (lua, pixel[3]);

  return 1;
}

static int l_set_lab (lua_State * lua)
{
    Priv *p;
    lua_Number pixel[4];
    lua_Number x, y, l, a, b;

    lua_pushstring (lua, "priv");
    lua_gettable (lua, LUA_REGISTRYINDEX);
    p = lua_touserdata (lua, -1);
    lua_pop (lua, 1);

    if (lua_gettop (lua) != 5)
      {
        lua_pushstring(lua,
                       "incorrect number of arguments to set_lab (x, y, l, a, b)\n");
        lua_error (lua);
        return 0;
      }

    x = lua_tonumber (lua, -5);
    y = lua_tonumber (lua, -4);
    l = lua_tonumber (lua, -3);
    a = lua_tonumber (lua, -2);
    b = lua_tonumber (lua, -1);

    /* pixel assumed to be of type double */
    
    get_rgba_pixel (p, 0, x, y, pixel);
    cpercep_space_to_rgb (l, a, b, &(pixel[0]), &(pixel[1]), &(pixel[2]));
      {
      int i;
      for (i=0;i<3;i++)
        pixel[i] *= (1.0/255.0);
      }
    set_rgba_pixel (p, x, y, pixel);

    return 0;
}

static int l_get_lab (lua_State * lua)
{
  Priv *p;
  lua_Number x, y;
  lua_Number img_no = 0;

  lua_Number pixel[4];
  double lab_pixel[3];

  lua_pushstring (lua, "priv");
  lua_gettable (lua, LUA_REGISTRYINDEX);
  p = lua_touserdata (lua, -1);
  lua_pop(lua, 1);

  switch (lua_gettop (lua))
    {
    case 3:
      img_no = lua_tonumber(lua, -3);
      break;
    case 2:
      img_no = 0;
      break;
    default:
      lua_pushstring (lua, "incorrect number of arguments to get_rgb (x, y, [, image_no])\n");
      lua_error (lua);
      break;
  }
  x = lua_tonumber(lua, -2);
  y = lua_tonumber(lua, -1);

  get_rgba_pixel (p, img_no, x, y, pixel);

  cpercep_rgb_to_space (pixel[0] * 255.0,
                        pixel[1] * 255.0,
                        pixel[2] * 255.0,

                        &(lab_pixel[0]),
                        &(lab_pixel[1]),
                        &(lab_pixel[2]));

  lua_pushnumber (lua, lab_pixel[0]);
  lua_pushnumber (lua, lab_pixel[1]);
  lua_pushnumber (lua, lab_pixel[2]);

  return 3;
}

static int l_set_hsl (lua_State * lua)
{
    Priv *p;
    lua_Number pixel[4];
    lua_Number x, y, h, s, l;

    lua_pushstring (lua, "priv");
    lua_gettable (lua, LUA_REGISTRYINDEX);
    p = lua_touserdata (lua, -1);
    lua_pop (lua, 1);

    if (lua_gettop (lua) != 5)
      {
        lua_pushstring(lua,
                       "incorrect number of arguments to set_lab (x, y, l, a, b)\n");
        lua_error (lua);
        return 0;
      }

    x = lua_tonumber (lua, -5);
    y = lua_tonumber (lua, -4);
    h = lua_tonumber (lua, -3);
    s = lua_tonumber (lua, -2);
    l = lua_tonumber (lua, -1);

    get_rgba_pixel (p, 0, x, y, pixel);
    
    {
      GimpRGB rgb;
      GimpHSL hsl;

      hsl.h = h;
      hsl.s = s;
      hsl.l = l;
     
      gimp_hsl_to_rgb (&hsl, &rgb); 

      pixel[0]=rgb.r;
      pixel[1]=rgb.g;
      pixel[2]=rgb.b;
    }

    set_rgba_pixel (p, x, y, pixel);
    return 0;
}

static int l_get_hsl (lua_State * lua)
{
  Priv *p;
  lua_Number x, y;
  lua_Number img_no = 0;

  lua_Number pixel[4];

  lua_pushstring (lua, "priv");
  lua_gettable (lua, LUA_REGISTRYINDEX);
  p = lua_touserdata (lua, -1);
  lua_pop(lua, 1);

  switch (lua_gettop (lua))
    {
    case 3:
      img_no = lua_tonumber(lua, -3);
      break;
    case 2:
      img_no = 0;
      break;
    default:
      lua_pushstring (lua, "incorrect number of arguments to get_rgb ([image_no,] x, y)\n");
      lua_error (lua);
      break;
  }
  x = lua_tonumber(lua, -2);
  y = lua_tonumber(lua, -1);

    get_rgba_pixel (p, img_no, x, y, pixel);

    {
      GimpRGB rgb;
      GimpHSL hsl;

      rgb.r = pixel[0];
      rgb.g = pixel[1];
      rgb.b = pixel[2];

      gimp_rgb_to_hsl (&rgb, &hsl); 

      lua_pushnumber (lua, hsl.h );
      lua_pushnumber (lua, hsl.s );
      lua_pushnumber (lua, hsl.l );
    }

  return 3;
}

static int l_set_hsv (lua_State * lua)
{
    Priv *p;
    lua_Number pixel[4];
    lua_Number x, y, h, s, v;

    lua_pushstring (lua, "priv");
    lua_gettable (lua, LUA_REGISTRYINDEX);
    p = lua_touserdata (lua, -1);
    lua_pop (lua, 1);

    if (lua_gettop (lua) != 5)
      {
        lua_pushstring(lua,
                       "incorrect number of arguments to set_lab (x, y, l, a, b)\n");
        lua_error (lua);
        return 0;
      }

    x = lua_tonumber (lua, -5);
    y = lua_tonumber (lua, -4);
    h = lua_tonumber (lua, -3);
    s = lua_tonumber (lua, -2);
    v = lua_tonumber (lua, -1);

    get_rgba_pixel (p, 0, x, y, pixel);
    
    {
      GimpRGB rgb;
      GimpHSV hsv;

      hsv.h = h;
      hsv.s = s;
      hsv.v = v;
     
      gimp_hsv_to_rgb (&hsv, &rgb); 

      pixel[0]=rgb.r;
      pixel[1]=rgb.g;
      pixel[2]=rgb.b;
    }

    set_rgba_pixel (p, x, y, pixel);
    return 0;
}

static int l_get_hsv (lua_State * lua)
{
  Priv *p;
  lua_Number x, y;
  lua_Number img_no = 0;

  lua_Number pixel[4];

  lua_pushstring (lua, "priv");
  lua_gettable (lua, LUA_REGISTRYINDEX);
  p = lua_touserdata (lua, -1);
  lua_pop(lua, 1);

  switch (lua_gettop (lua))
    {
    case 3:
      img_no = lua_tonumber(lua, -3);
      break;
    case 2:
      img_no = 0;
      break;
    default:
      lua_pushstring (lua, "incorrect number of arguments to get_rgb ([image_no,] x, y)\n");
      lua_error (lua);
      break;
  }
  x = lua_tonumber(lua, -2);
  y = lua_tonumber(lua, -1);

    get_rgba_pixel (p, img_no, x, y, pixel);

    {
      GimpRGB rgb;
      GimpHSV hsv;

      rgb.r = pixel[0];
      rgb.g = pixel[1];
      rgb.b = pixel[2];

      gimp_rgb_to_hsv (&rgb, &hsv); 

      lua_pushnumber (lua, hsv.h );
      lua_pushnumber (lua, hsv.s );
      lua_pushnumber (lua, hsv.v );
    }

  return 3;
}
