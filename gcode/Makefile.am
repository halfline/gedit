noinst_LTLIBRARIES += gcode/libgcode.la

gcode_libgcode_la_CPPFLAGS =		\
	-I$(top_srcdir)/gcode		\
	-I$(top_builddir)/gcode

gcode_libgcode_la_CFLAGS =		\
	$(GEDIT_CFLAGS)			\
	$(GTK_MAC_CFLAGS)		\
	$(WARN_CFLAGS)			\
	$(DISABLE_DEPRECATED_CFLAGS)	\
	$(INTROSPECTION_CFLAGS)

gcode_libgcode_la_LIBADD =		\
	$(GEDIT_LIBS)			\
	$(GTK_MAC_LIBS)			\
	$(INTROSPECTION_LIBS)

gcode_libgcode_la_LDFLAGS = -no-undefined

gcode_libgcode_la_SOURCES =			\
	gcode/gcode.h				\
	gcode/gcode-debug.c			\
	gcode/gcode-debug.h			\
	gcode/gcode-metadata-manager.c		\
	gcode/gcode-metadata-manager.h		\
	gcode/gcode-utils.c			\
	gcode/gcode-utils.h
