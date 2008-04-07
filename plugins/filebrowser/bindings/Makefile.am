noinst_LTLIBRARIES = \
	filebrowser.la

nodist_filebrowser_la_SOURCES = \
	filebrowser.c

filebrowser_la_LDFLAGS = \
	-module -avoid-version

filebrowser_la_LIBADD = \
	$(PYTHON_LIB_LOC)       \
	$(PYTHON_LIBS)		\
	$(PYTHON_EXTRA_LIBS)	\
	$(PYGTK_LIBS)

filebrowser_la_CFLAGS = \
	-I$(top_srcdir)			\
	-I$(top_builddir)		\
	-I$(top_srcdir)/gedit		\
	-I$(top_builddir)/gedit		\
	$(GEDIT_CFLAGS)			\
	$(NO_STRICT_ALIASING_CFLAGS)	\
	$(PYGTK_CFLAGS)			\
	$(PYTHON_CFLAGS)		\
	$(AM_CFLAGS)

$(builddir)/../gedit-file-browser-enum-types.h:
	cd $(builddir)/../ && $(MAKE) gedit-file-browser-enum-types.h

filebrowser.c: filebrowser.defs filebrowser.override $(builddir)/../gedit-file-browser-enum-types.h
	( cd $(srcdir) && $(PYGTK_CODEGEN) \
		--override $*.override \
		--register $(top_builddir)/bindings/python/gedit.defs \
		--register $(PYGTK_DEFSDIR)/gtk-types.defs \
		--register $(PYGTK_DEFSDIR)/gtk-base.defs \
		--prefix py$* $(<F) ) > $@ 

BINDING_HEADERS_BUILDDIR_IN = 

BINDING_HEADERS_SRCDIR_IN = \
	plugins/filebrowser/gedit-file-browser-error.h \
	plugins/filebrowser/gedit-file-bookmarks-store.h \
	plugins/filebrowser/gedit-file-browser-store.h \
	plugins/filebrowser/gedit-file-browser-view.h \
	plugins/filebrowser/gedit-file-browser-widget.h \
	plugins/filebrowser/gedit-file-browser-plugin.h

BINDING_HEADERS_SRCDIR := $(addprefix $(top_srcdir)/,$(BINDING_HEADERS_SRCDIR_IN))
BINDING_HEADERS_BUILDDIR := $(addprefix $(top_builddir)/,$(BINDING_HEADERS_BUILDDIR_IN))

regenerate-python-binding:
	$(PYGTK_H2DEF) $(sort $(BINDING_HEADERS_SRCDIR) $(BINDING_HEADERS_BUILDDIR)) > filebrowser.defs.new
	
BUILT_SOURCES = \
	filebrowser.c

EXTRA_DIST = \
	filebrowser.override		\
	filebrowser.defs

CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
