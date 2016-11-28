## Process this file with automake to produce Makefile.in

bin_PROGRAMS = gluas

bindir = $(GIMP_LIBDIR)/plug-ins

gluas_SOURCES =	\
	cpercep.c	\
	cpercep.h	\
	main.c		\
	main.h		\
	interface.h	\
	interface.c	\
	preview_layer.c	\
	preview_layer.h	\
	render.h	\
	render.c

AM_CPPFLAGS = \
	-DLOCALEDIR=\""$(LOCALEDIR)"\"	\
	-DDATADIR=\""$(DATADIR)"\"	\
	$(LUA_CFLAGS)			\
	$(GTKSOURCEVIEW_CFLAGS)

INCLUDES =\
	-I$(top_srcdir)		\
	@GIMP_CFLAGS@		\
	-I$(includedir)

LDADD = $(GIMP_LIBS) \
	$(LUA_LIBS) \
	$(GTKSOURCEVIEW_LIBS)

