# C plugin loader

SUBDIRS = bindings
loaderdir = $(libdir)/gedit-2/plugin-loaders

INCLUDES = \
	-I$(top_srcdir) 						\
	-I$(top_builddir)						\
	-I$(top_srcdir)/gedit						\
	-I$(top_builddir)/gedit						\
	$(GEDIT_CFLAGS) 						\
	$(WARN_CFLAGS)							\
	$(DISABLE_DEPRECATED_CFLAGS)					\
	$(PYGTK_CFLAGS)							\
	$(PYTHON_CFLAGS)						\
	-DGEDIT_LOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

loader_LTLIBRARIES = libpythonloader.la


NOINST_H_FILES = \
	gedit-plugin-loader-python.h	\
	gedit-plugin-python.h

libpythonloader_la_SOURCES = \
	gedit-plugin-loader-python.c 	\
	gedit-plugin-python.c
	$(NOINST_H_FILES)

libpythonloader_la_LDFLAGS = $(LOADER_LIBTOOL_FLAGS)
libpythonloader_la_LIBADD = \
	$(GEDIT_LIBS) 			\
	$(srcdir)/bindings/gedit.la

loader_in_files = pythonloader.gedit-plugin-loader.desktop.in

%.gedit-plugin-loader: %.gedit-plugin-loader.desktop.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

loader_DATA = $(loader_in_files:.gedit-plugin-loader.desktop.in=.gedit-plugin-loader)

EXTRA_DIST = \
	$(loader_in_files)

CLEANFILES = \
	$(loader_DATA)

DISTCLEANFILES = \
	$(loader_DATA)
