dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.54)

dnl Name your plug-in here
m4_define([plugin_name], [gluas])

dnl These three define the plug-in version number
m4_define([plugin_major_version], [0])
m4_define([plugin_minor_version], [1])
m4_define([plugin_micro_version], [21])

m4_define([plugin_version],
          [plugin_major_version.plugin_minor_version.plugin_micro_version])

AC_INIT([plugin_name], [plugin_version])

AC_DEFINE(PLUGIN_NAME, PACKAGE_NAME, [Plug-In name])
AC_DEFINE(PLUGIN_VERSION, PACKAGE_VERSION, [Plug-In version])

AC_DEFINE(PLUGIN_MAJOR_VERSION, plugin_major_version, [Plug-In major version])
AC_DEFINE(PLUGIN_MINOR_VERSION, plugin_minor_version, [Plug-In minor version])
AC_DEFINE(PLUGIN_MICRO_VERSION, plugin_micro_version, [Plug-In micro version])

AC_CONFIG_SRCDIR([src/main.c])
AM_CONFIG_HEADER(config.h)

AM_INIT_AUTOMAKE(no-define)

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC


ACLOCAL="$ACLOCAL $ACLOCAL_FLAGS"

AM_MAINTAINER_MODE


dnl Use -Wall if we have gcc.
changequote(,)dnl
if test "x$GCC" = "xyes"; then
  case " $CFLAGS " in
  *[\ \	]-Wall[\ \	]*) ;;
  *) CFLAGS="$CFLAGS -Wall" ;;
  esac
fi
changequote([,])dnl


PKG_CHECK_MODULES(GIMP, gimp-2.0 gimpui-2.0)

AC_SUBST(GIMP_CFLAGS)
AC_SUBST(GIMP_LIBS)

GIMP_LIBDIR=`$PKG_CONFIG --variable=gimplibdir gimp-2.0`
AC_SUBST(GIMP_LIBDIR)

AC_MSG_CHECKING([if GTK+ is version 2.3.0 or newer])
if $PKG_CONFIG --atleast-version=2.3.0 gtk+-2.0; then
  have_gtk_2_3=yes
else
  have_gtk_2_3=no
fi
AC_MSG_RESULT($have_gtk_2_3)

if test "x$have_gtk_2_3" != "xyes"; then
  CPPFLAGS="$CPPFLAGS -DG_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED"
fi

m4_define([lua_required_version], [5.1.0])

LUA_REQUIRED_VERSION=lua_required_version
AC_SUBST(LUA_REQUIRED_VERSION)


###############
# Check for Lua
###############

have_lua="no"
PKG_CHECK_MODULES(LUA, lua >= lua_required_version,
  have_lua="yes",
  [PKG_CHECK_MODULES(LUA, lua5.1 >= lua_required_version,
     have_lua="yes",
     [PKG_CHECK_MODULES(LUA, lua5.2 >= lua_required_version,
       have_lua="yes",
       [PKG_CHECK_MODULES(LUA, lua5.3 >= lua_required_version,
         have_lua="yes",
         [PKG_CHECK_MODULES(LUA, luajit >= 2.0.4,
          have_lua="yes",
          have_lua="no  (usable lua not found)")])])])])

AC_SUBST(LUA_CFLAGS)
AC_SUBST(LUA_LIBS)



AC_ARG_ENABLE(gtksourceview,
  [  --disable-gtksourceview dont use gtksourceview for syntax highlighting (default=auto)],
  [use_gtksourceview=$enableval], [use_gtksourceview=yes])

if test "x$use_gtksourceview" = "xyes"; then
    PKG_CHECK_MODULES(GTKSOURCEVIEW, gtksourceview-2.0 >= 2.0.0,
        [use_gtksourceview=yes],
        [use_gtksourceview="no (code will not be syntax highlighted, see http:///gtksourceview.sourceforge.net)"]
    )
fi

if test "x$use_gtksourceview" = "xyes"; then
    AM_CONDITIONAL(USE_GTKSOURCEVIEW, true)
    AC_SUBST(GTKSOURCEVIEW_CFLAGS)
    AC_SUBST(GTKSOURCEVIEW_LIBS)
    AC_DEFINE(USE_GTKSOURCEVIEW,,[Syntax highlighting])
else
    AM_CONDITIONAL(USE_GTKSOURCEVIEW, false)
fi

AC_CONFIG_FILES([
Makefile
src/Makefile
])

AC_OUTPUT
