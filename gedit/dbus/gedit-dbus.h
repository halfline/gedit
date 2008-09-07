#ifndef __GEDIT_DBUS_H__
#define __GEDIT_DBUS_H__

#include <glib-object.h>
#include <dbus/dbus-glib.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_DBUS			(gedit_dbus_get_type ())
#define GEDIT_DBUS(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_DBUS, GeditDBus))
#define GEDIT_DBUS_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_DBUS, GeditDBus const))
#define GEDIT_DBUS_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), GEDIT_TYPE_DBUS, GeditDBusClass))
#define GEDIT_IS_DBUS(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEDIT_TYPE_DBUS))
#define GEDIT_IS_DBUS_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_DBUS))
#define GEDIT_DBUS_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GEDIT_TYPE_DBUS, GeditDBusClass))

typedef struct _GeditDBus		GeditDBus;
typedef struct _GeditDBusClass		GeditDBusClass;
typedef struct _GeditDBusPrivate	GeditDBusPrivate;

struct _GeditDBus {
	GObject parent;
	
	GeditDBusPrivate *priv;
};

struct _GeditDBusClass {
	GObjectClass parent_class;
};

GType gedit_dbus_get_type (void) G_GNUC_CONST;
GeditDBus *gedit_dbus_new(void);

gboolean gedit_dbus_send (GeditDBus    *bus,
			  const gchar  *domain,
			  const gchar  *name,
			  GHashTable   *message,
			  GError      **error);

gboolean gedit_dbus_send_sync (GeditDBus             *bus,
			       const gchar           *domain,
			       const gchar           *name,
			       GHashTable            *message,
			       DBusGMethodInvocation *invocation);

gboolean gedit_dbus_initialize (void);

G_END_DECLS

#endif /* __GEDIT_DBUS_H__ */
