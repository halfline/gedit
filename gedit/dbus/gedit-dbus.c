#include "gedit-dbus.h"

#include <gedit/gedit-message-bus.h>

#include "gedit-dbus-message.h"
#include "gedit-dbus-glue.h"

#define GEDIT_DBUS_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GEDIT_TYPE_DBUS, GeditDBusPrivate))

/*struct _GeditDBusPrivate
{
};*/

G_DEFINE_TYPE (GeditDBus, gedit_dbus, G_TYPE_OBJECT)

static void
gedit_dbus_finalize (GObject *object)
{
	G_OBJECT_CLASS (gedit_dbus_parent_class)->finalize (object);
}

static void
gedit_dbus_class_init (GeditDBusClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	object_class->finalize = gedit_dbus_finalize;

	dbus_g_object_type_install_info (GEDIT_TYPE_DBUS, 
					 &dbus_glib_gedit_dbus_object_info);
	
	//g_type_class_add_private (object_class, sizeof(GeditDBusPrivate));
}

static void
gedit_dbus_init (GeditDBus *self)
{
	//self->priv = GEDIT_DBUS_GET_PRIVATE (self);
}

static void
on_dispatch_message (GeditMessageBus *bus,
		     GeditMessage    *message,
		     DBusGConnection *gbus)
{
	/* register new object on dbus */
	GObject *msg = g_object_new (GEDIT_TYPE_DBUS_MESSAGE, NULL);
	gchar *path;
	
	path = g_strdup_printf ("/org/gnome/gedit/message/%s/%s", 
				gedit_message_get_domain (message),
				gedit_message_get_name (message));

	dbus_g_connection_register_g_object (gbus, path, msg);
	gedit_dbus_message_emit (GEDIT_DBUS_MESSAGE (msg), message);
	
	g_object_unref (msg);
}

gboolean
gedit_dbus_send (GeditDBus    *bus,
		 const gchar  *domain,
		 const gchar  *name,
		 GHashTable   *message,
		 GError	     **error)
{
	/* propagate the message over the internal bus */
	GeditMessage *msg;
	
	msg = gedit_message_new_hash (domain, name, message);
	gedit_message_bus_send_message (gedit_message_bus_get_default(),
					msg);

	g_object_unref (msg);

	return TRUE;
}

gboolean
gedit_dbus_send_sync (GeditDBus             *bus,
		      const gchar           *domain,
		      const gchar           *name,
		      GHashTable            *message,
		      DBusGMethodInvocation *invocation)
{
	/* propagate the message over the internal bus */
	GeditMessage *msg;
	
	msg = gedit_message_new_hash (domain, name, message);
	gedit_message_bus_send_message_sync (gedit_message_bus_get_default(),
					     msg);

	/* return message */
	dbus_g_method_return (invocation, gedit_message_get_hash (msg));
	g_object_unref (msg);

	return TRUE;
}

gboolean
gedit_dbus_initialize (GError **error)
{
	DBusGProxy *bus_proxy;
	DBusGConnection *gbus;
	guint request_name_result;
	GeditMessageBus *bus;
	
	if (error)
		*error = NULL;
	
	gbus = dbus_g_bus_get (DBUS_BUS_SESSION, error);
	
	if (!gbus)
		return FALSE;

	/* Register unique name */
	bus_proxy = dbus_g_proxy_new_for_name (gbus, 
					       "org.freedesktop.DBus",
					       "/org/freedesktop/DBus",
					       "org.freedesktop.DBus");
	
	if (!dbus_g_proxy_call (bus_proxy, "RequestName", error,
			  	G_TYPE_STRING, "org.gnome.gedit",
			  	G_TYPE_UINT, DBUS_NAME_FLAG_DO_NOT_QUEUE,
			  	G_TYPE_INVALID,
			  	G_TYPE_UINT, &request_name_result,
			  	G_TYPE_INVALID))
	{
		g_object_unref (bus_proxy);
		return FALSE;
	}
	
	if (request_name_result == DBUS_REQUEST_NAME_REPLY_EXISTS)
	{
		/* there is already a master gedit process */
		g_object_unref (bus_proxy);
		return FALSE;
	}

	/* we are the master gedit process */	
	dbus_g_connection_register_g_object (gbus, "/org/gnome/gedit", g_object_new (GEDIT_TYPE_DBUS, NULL));
		
	/* CHECK: maybe add a reference to the bus? */
	bus = gedit_message_bus_get_default ();
	g_signal_connect (bus, "dispatch", G_CALLBACK (on_dispatch_message), gbus);
	
	g_object_unref (bus_proxy);
	
	return TRUE;
}
