noinst_LTLIBRARIES += gedit/gcode/libgcode.la

gedit_gcode_libgcode_la_CPPFLAGS =	\
	-I$(top_srcdir)/gedit/gcode	\
	-I$(top_builddir)/gedit/gcode

gedit_gcode_libgcode_la_CFLAGS =	\
	$(GEDIT_CFLAGS)			\
	$(GTK_MAC_CFLAGS)		\
	$(WARN_CFLAGS)			\
	$(DISABLE_DEPRECATED_CFLAGS)	\
	$(INTROSPECTION_CFLAGS)

gedit_gcode_libgcode_la_LIBADD =	\
	$(GEDIT_LIBS)			\
	$(GTK_MAC_LIBS)			\
	$(INTROSPECTION_LIBS)

gedit_gcode_libgcode_la_LDFLAGS = -no-undefined

gedit_gcode_libgcode_la_SOURCES =			\
	gedit/gcode/gcode.h				\
	gedit/gcode/gcode-debug.c			\
	gedit/gcode/gcode-debug.h			\
	gedit/gcode/gcode-document.c			\
	gedit/gcode/gcode-document.h			\
	gedit/gcode/gcode-metadata-manager.c		\
	gedit/gcode/gcode-metadata-manager.h		\
	gedit/gcode/gcode-utils.c			\
	gedit/gcode/gcode-utils.h
