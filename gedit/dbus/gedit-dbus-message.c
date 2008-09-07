#include "gedit-dbus-message.h"

#include <dbus/dbus-glib.h>
#include "gedit-dbus-message-glue.h"

#define GEDIT_DBUS_MESSAGE_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GEDIT_TYPE_DBUS_MESSAGE, GeditDBusMessagePrivate))

/*struct _GeditDBusMessagePrivate
{
};*/

/* signals */
enum
{
	MESSAGE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GeditDBusMessage, gedit_dbus_message, G_TYPE_OBJECT)

static void
gedit_dbus_message_finalize (GObject *object)
{
	G_OBJECT_CLASS (gedit_dbus_message_parent_class)->finalize (object);
}

static void
gedit_dbus_message_class_init (GeditDBusMessageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	object_class->finalize = gedit_dbus_message_finalize;

	signals[MESSAGE] =
		g_signal_new ("message",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, 
			      dbus_g_type_get_map ("GHashTable", 
			      			   G_TYPE_STRING, 
			      			   G_TYPE_VALUE));

	dbus_g_object_type_install_info (GEDIT_TYPE_DBUS_MESSAGE, 
					 &dbus_glib_gedit_dbus_message_object_info);

	//g_type_class_add_private (object_class, sizeof (GeditDBusMessagePrivate));
}

static void
gedit_dbus_message_init (GeditDBusMessage *self)
{
	//self->priv = GEDIT_DBUS_MESSAGE_GET_PRIVATE(self);
}

void
gedit_dbus_message_emit (GeditDBusMessage *dbusmessage,
			 GeditMessage     *message)
{
	g_signal_emit (dbusmessage, signals[MESSAGE], 0, gedit_message_get_hash (message));
}
