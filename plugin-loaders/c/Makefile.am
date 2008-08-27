# C plugin loader

loaderdir = $(libdir)/gedit-2/plugin-loaders

INCLUDES = \
	-I$(top_srcdir) 						\
	$(GEDIT_CFLAGS) 						\
	$(WARN_CFLAGS)							\
	$(DISABLE_DEPRECATED_CFLAGS)					\
	-DGEDIT_LOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

loader_LTLIBRARIES = libcloader.la

NOINST_H_FILES = \
	gedit-plugin-loader-c.h

libcloader_la_SOURCES = \
	gedit-plugin-loader-c.c 	\
	$(NOINST_H_FILES)

libcloader_la_LDFLAGS = $(LOADER_LIBTOOL_FLAGS)
libcloader_la_LIBADD = $(GEDIT_LIBS)

loader_in_files = cloader.gedit-plugin-loader.desktop.in

%.gedit-plugin-loader: %.gedit-plugin-loader.desktop.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

loader_DATA = $(loader_in_files:.gedit-plugin-loader.desktop.in=.gedit-plugin-loader)

EXTRA_DIST = \
	$(loader_in_files)

CLEANFILES = \
	$(loader_DATA)

DISTCLEANFILES = \
	$(loader_DATA)
