INCLUDES =							\
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
	-I$(top_srcdir)/gedit 					\
	-I$(top_builddir)/gedit					\
	$(GEDIT_CFLAGS) 					\
	$(DBUS_CFLAGS)						\
	$(WARN_CFLAGS)						\
	$(DISABLE_DEPRECATED_CFLAGS)

noinst_LTLIBRARIES = libdbus.la

libdbus_la_SOURCES = 			\
	gedit-dbus.c			\
	gedit-dbus.h			\
	gedit-dbus-message.c		\
	gedit-dbus-message.h

BUILT_SOURCES = gedit-dbus-message-glue.h gedit-dbus-glue.h

gedit-dbus-message-glue.h: gedit-dbus-message.xml
	$(LIBTOOL) --mode=execute dbus-binding-tool --prefix=gedit_dbus_message --mode=glib-server --output=gedit-dbus-message-glue.h $(srcdir)/gedit-dbus-message.xml

gedit-dbus-glue.h: gedit-dbus.xml
	$(LIBTOOL) --mode=execute dbus-binding-tool --prefix=gedit_dbus --mode=glib-server --output=gedit-dbus-glue.h $(srcdir)/gedit-dbus.xml

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = gedit-dbus-message.xml gedit-dbus.xml
