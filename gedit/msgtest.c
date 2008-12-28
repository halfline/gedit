#include "gedit-message-bus.h"

static void
fb_set_root_cb (GeditMessageBus *bus, GeditMessage *message, gpointer user_data)
{
	gchar *uri = NULL;
	
	gedit_message_get (message, "uri", &uri, NULL);
	g_message ("Setting root to: %s", uri);
	
	if (gedit_message_has_key (message, "result"))
		gedit_message_set (message, "result", TRUE, NULL);
	
	g_free (uri);
}

int
main (int argc, char *argv[])
{
	GeditMessageBus *bus;
	
	g_type_init ();
	
	bus = gedit_message_bus_get_default ();

	/* registering a new message on the bus */
	GeditMessageType *message_type = gedit_message_bus_register (bus, 
								     "/plugins/filebrowser", "set_root", 
								     1, 
								     "uri", G_TYPE_STRING, 
								     "result", G_TYPE_BOOLEAN,
								     NULL);
	
	/* register callback for the message type */
	guint id = gedit_message_bus_connect (bus, 
					      "/plugins/filebrowser", "set_root", 
					      fb_set_root_cb, 
					      NULL, NULL);

	gedit_message_bus_send (bus,
				"/plugins/filebrowser", "set_root",
				"uri", "/hello/world/async",
				NULL);

	/* sending the set_root message */
	gboolean result = FALSE;
	GeditMessage *message = gedit_message_bus_send_sync (bus, 
							     "/plugins/filebrowser", "set_root", 
							     "uri", "/hello/world", 
							     "result", result,
							     NULL);

	gedit_message_get (message, "result", &result, NULL);
	g_message ("Result: %d", result);
	
	g_main_context_iteration (NULL, FALSE);
	
	/* unregister message type */
	gedit_message_bus_unregister (bus, message_type);
	gedit_message_bus_disconnect (bus, id);
	
	g_object_unref (message);
	g_object_unref (bus);

	return 0;
}
