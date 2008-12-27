#include "gedit-message-bus.h"

static void
fb_set_root_cb (GeditMessageBus *bus, GeditMessage *message, gpointer user_data)
{
	gchar *uri = NULL;

	gedit_message_get (message, "uri", &uri, NULL);
	g_message ("Setting root to: %s", uri);
	
	g_free (uri);
}

int
main (int argc, char *argv[])
{
	GeditMessageBus *bus;
	
	g_type_init ();
	
	bus = gedit_message_bus_get_default ();

	/* registering a new message on the bus */
	GeditMessageType *message_type = gedit_message_bus_register (bus, "/plugins/filebrowser", "set_root", 0, "uri", G_TYPE_STRING, NULL);
	
	/* register callback for the message type */
	gedit_message_bus_connect (bus, "/plugins/filebrowser", "set_root", fb_set_root_cb, NULL, NULL);

	/* sending the set_root message */
	gedit_message_bus_send_sync (bus, "/plugins/filebrowser", "set_root", "uri", "/hello/world", NULL);
	
	/* unregister message type */
	gedit_message_bus_unregister (bus, message_type);
	g_object_unref (bus);
	
	return 0;
}
