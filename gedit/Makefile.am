## Process this file with automake to produce Makefile.in

appicondir=$(datadir)/pixmaps

appicon_DATA = gnome-gedit.png

INCLUDES = -I$(includedir) -I$(top_srcdir)		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-DLOCALEDIR=\"$(datadir)/locale\"		\
	-I$(top_srcdir)/intl -I$(top_builddir)/intl	\
	$(GNOME_INCLUDEDIR)

## This need to be two distinct directories
plugindir = $(libexecdir)/go/plugins
pluginlibdir = $(libexecdir)/gedit/plugins

geditincludedir = $(includedir)/gedit

gtkrcdir = $(datadir)
gtkrc_DATA = geditrc

EXTRA_gedit_SOURCES = gE_plugin.h gE_plugin.c

if WITH_GMODULE_PLUGINS
geditinclude_HEADERS = gE_plugin.h
GMODULE_PLUGIN_SRCLIST = gE_plugin.h gE_plugin.c
else
GMODULE_PLUGIN_SRCLIST =
endif

bin_PROGRAMS = gedit
noinst_PROGRAMS = testcorba

BUILT_SOURCES = \
	desktop-editor.h	\
	desktop-editor-common.c	\
	desktop-editor-skels.c	\
	desktop-editor-stubs.c	\
	desktop-textviewer.h	\
	desktop-textviewer-common.c	\
	desktop-textviewer-skels.c	\
	desktop-textviewer-stubs.c

testcorba_SOURCES = \
	openfile.c	\
	desktop-editor-common.c	\
	desktop-editor-stubs.c	\
	desktop-editor.h	\
	desktop-textviewer.h	\
	desktop-textviewer-common.c	\
	desktop-textviewer-stubs.c

gedit_SOURCES = 	\
	gedit.c		\
	commands.c	\
	commands.h	\
	gE_document.c	\
	gE_document.h   \
	gE_mdi.c	\
	gE_mdi.h	\
	gE_about.c	\
	gE_about.h      \
	gE_files.c	\
	gE_files.h      \
	gE_prefs.c	\
	gE_prefslib.c	\
	gE_prefslib.h	\
	gE_prefs_box.c	\
	gE_prefs_box.h	\
	toolbar.c	\
	plugin.c        \
	gE_plugin_api.c	\
	gE_print.c	\
	gE_print.h	\
	gE_prefs.h	\
	main.h		\
	toolbar.h	\
	plugin.h        \
	gE_plugin_api.h	\
	menus.h		\
	menus.c		\
	search.h	\
	search.c	\
	dialog.h	\
	dialog.c	\
	$(GMODULE_PLUGIN_SRCLIST)	\
	editor.c	\
	$(BUILT_SOURCES)


gedit_LDADD = $(GNOME_LIBDIR) $(GNOMEUI_LIBS)  \
	      $(INTLLIBS) $(ORBIT_LIBS) $(GNORBA_LIBS)

testcorba_LDADD = $(GNOME_LIBDIR) $(GNOMEUI_LIBS) $(ORBIT_LIBS) \
	      $(INTLLIBS) $(GNORBA_LIBS)

$(BUILT_SOURCES): $(datadir)/idl/desktop-editor.idl
	$(ORBIT_IDL) $(datadir)/idl/desktop-editor.idl
	$(ORBIT_IDL) $(datadir)/idl/desktop-textviewer.idl

EXTRA_DIST = gE-icon-1.xpm gE-icon-2.xpm gE_icon.xpm $(gtkrc_DATA) $(appicon_DATA)

CPPFLAGS = \
	-DBINDIR=\""$(bindir)"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DPLUGINDIR=\""$(plugindir)"\" \
	-DPLUGINLIBDIR=\""$(pluginlibdir)"\" \
	-Wall -Wno-unused -ansi -D_IN_GEDIT
#	-DDEBUG

