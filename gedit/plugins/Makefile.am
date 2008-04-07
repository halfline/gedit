INCLUDES = 							\
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
	-I$(top_srcdir)/gedit 					\
	-I$(top_builddir)/gedit					\
	$(GEDIT_CFLAGS) 					\
	$(WARN_CFLAGS)						\
	$(DISABLE_DEPRECATED_CFLAGS)				\
	-DDATADIR=\""$(datadir)"\"				\
	-DGEDIT_DATADIR=\""$(datadir)/gedit-2"\"		\
	-DGEDIT_LOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DGEDIT_UI_DIR=\""$(datadir)/gedit-2/ui/"\"		\
	-DGEDIT_PLUGINDIR=\""$(libdir)/gedit-2/plugins"\"	\
	-DGEDIT_GLADEDIR=\""$(datadir)/gedit-2/glade/"\"	\
	-DGEDIT_ICONDIR=\""$(datadir)/gedit-2/icons"\"

if ENABLE_PYTHON
INCLUDES += 					\
	$(NO_STRICT_ALIASING_CFLAGS)		\
	$(PYGTK_CFLAGS)				\
	$(PYTHON_CFLAGS)			\
	$(AM_CFLAGS)
endif

noinst_LTLIBRARIES = libplugins.la

NOINST_H_FILES =				\
	gedit-plugin-info-priv.h		\
	gedit-plugin-manager.h			\
	gedit-module.h

if ENABLE_PYTHON
NOINST_H_FILES += 				\
	gedit-python-module.h			\
	gedit-python-plugin.h 
endif

INST_H_FILES = 					\
	gedit-plugin.h				\
	gedit-plugin-info.h			\
	gedit-plugins-engine.h

if ENABLE_PYTHON
INST_H_FILES +=					\
	gedit-plugin-python-utils.h
endif

headerdir = $(prefix)/include/gedit-@GEDIT_API_VERSION@/gedit/plugins

header_DATA = 					\
	$(INST_H_FILES)

libplugins_la_SOURCES = 			\
	gedit-module.c				\
	gedit-plugin.c				\
	gedit-plugin-info.c			\
	gedit-plugin-manager.c			\
	gedit-plugins-engine.c			\
	$(NOINST_H_FILES)			\
	$(INST_H_FILES)

if ENABLE_PYTHON
libplugins_la_SOURCES += 			\
	gedit-python-module.c			\
	gedit-python-plugin.c			\
	gedit-plugin-python-utils.c
endif	
