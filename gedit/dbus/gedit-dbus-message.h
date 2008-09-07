#ifndef __GEDIT_DBUS_MESSAGE_H__
#define __GEDIT_DBUS_MESSAGE_H__

#include <glib-object.h>
#include <gedit/gedit-message.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_DBUS_MESSAGE			(gedit_dbus_message_get_type ())
#define GEDIT_DBUS_MESSAGE(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_DBUS_MESSAGE, GeditDBusMessage))
#define GEDIT_DBUS_MESSAGE_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_DBUS_MESSAGE, GeditDBusMessage const))
#define GEDIT_DBUS_MESSAGE_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), GEDIT_TYPE_DBUS_MESSAGE, GeditDBusMessageClass))
#define GEDIT_IS_DBUS_MESSAGE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEDIT_TYPE_DBUS_MESSAGE))
#define GEDIT_IS_DBUS_MESSAGE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_DBUS_MESSAGE))
#define GEDIT_DBUS_MESSAGE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GEDIT_TYPE_DBUS_MESSAGE, GeditDBusMessageClass))

typedef struct _GeditDBusMessage	GeditDBusMessage;
typedef struct _GeditDBusMessageClass	GeditDBusMessageClass;
typedef struct _GeditDBusMessagePrivate	GeditDBusMessagePrivate;

struct _GeditDBusMessage {
	GObject parent;
	
	GeditDBusMessagePrivate *priv;
};

struct _GeditDBusMessageClass {
	GObjectClass parent_class;
};

GType gedit_dbus_message_get_type (void) G_GNUC_CONST;

void  gedit_dbus_message_emit (GeditDBusMessage *dbusmessage,
			       GeditMessage *message);

G_END_DECLS

#endif /* __GEDIT_DBUS_MESSAGE_H__ */
