/*
 * gedit.c
 * This file is part of gedit
 *
 * Copyright (C) 2005 - Paolo Maggi 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, 
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the gedit Team, 2005. See the AUTHORS file for a 
 * list of people on the gedit Team.  
 * See the ChangeLog files for a list of changes. 
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include <glib/gi18n.h>
#include <glib/goption.h>
#include <gdk/gdkx.h>

#include "gedit-app.h"
#include "gedit-commands.h"
#include "gedit-debug.h"
#include "gedit-encodings.h"
#include "gedit-metadata-manager.h"
#include "gedit-plugins-engine.h"
#include "gedit-prefs-manager-app.h"
#include "gedit-session.h"
#include "gedit-utils.h"
#include "gedit-window.h"

#include "eggsmclient.h"
#include "gedit-message-bus.h"

#ifdef ENABLE_DBUS
#include "dbus/gedit-dbus.h"
#endif

/* command line */
static gchar *encoding_charset = NULL;
static gboolean new_window_option = FALSE;
static gboolean new_document_option = FALSE;
static gchar **remaining_args = NULL;
static gboolean standalone_option = FALSE;

static const GOptionEntry options [] =
{
	{ "encoding", '\0', 0, G_OPTION_ARG_STRING, &encoding_charset,
	  N_("Set the character encoding to be used to open the files listed on the command line"), N_("ENCODING")},

	{ "new-window", '\0', 0, G_OPTION_ARG_NONE, &new_window_option,
	  N_("Create a new toplevel window in an existing instance of gedit"), NULL },

	{ "new-document", '\0', 0, G_OPTION_ARG_NONE, &new_document_option,
	  N_("Create a new document in an existing instance of gedit"), NULL },

	{ "standalone", '\0', 0, G_OPTION_ARG_NONE, &standalone_option,
	  N_("Start gedit as a separate standalone process"), NULL },

	{ G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &remaining_args,
	  NULL, N_("[FILE...]") }, /* collects file arguments */

	{NULL}
};

static void
gedit_get_command_line_data (GSList **files,
			     gint    *line_position)
{
	gint i;

	*files = NULL;
	*line_position = 0;

	if (!remaining_args)
		return;

	for (i = 0; remaining_args[i]; i++) 
	{
		if (*remaining_args[i] == '+')
		{
			if (*(remaining_args[i] + 1) == '\0')
				/* goto the last line of the document */
				*line_position = G_MAXINT;
			else
				*line_position = atoi (remaining_args[i] + 1);
		}
		else
		{
			gchar *canonical_uri;
			
			canonical_uri = gedit_utils_make_canonical_uri_from_shell_arg (remaining_args[i]);
			
			if (canonical_uri != NULL)
				*files = g_slist_prepend (*files, 
							  canonical_uri);
			else
				g_print (_("%s: malformed file name or URI.\n"),
					 remaining_args[i]);
		} 
	}

	*files = g_slist_reverse (*files);
}

static guint32
get_startup_timestamp ()
{
	const gchar *startup_id_env;
	gchar *startup_id = NULL;
	gchar *time_str;
	gchar *end;
	gulong retval = 0;

	/* we don't unset the env, since startup-notification
	 * may still need it */
	startup_id_env = g_getenv ("DESKTOP_STARTUP_ID");
	if (startup_id_env == NULL)
		goto out;

	startup_id = g_strdup (startup_id_env);

	time_str = g_strrstr (startup_id, "_TIME");
	if (time_str == NULL)
		goto out;

	errno = 0;

	/* Skip past the "_TIME" part */
	time_str += 5;

	retval = strtoul (time_str, &end, 0);
	if (end == time_str || errno != 0)
		retval = 0;

 out:
	g_free (startup_id);

	return (retval > 0) ? retval : 0;
}

static GeditMessage *
make_message (GSList *files, gint line_position, guint32 startup_timestamp)
{
	GSList *file;
	gchar **uris;
	gint i = 0;
	GeditMessage *message;
	GdkScreen *screen;
	GdkDisplay *display;
	gint viewport_x;
	gint viewport_y;
	
	/* insert uris as G_TYPE_STRV list */
	uris = g_new0(gchar *, g_slist_length (files) + 1);
	
	for (file = files; file; file = file->next)
		uris[i++] = g_strdup ((gchar *)file->data);

	/* initialize message with correct keys and types */
	message = gedit_message_new ("core", "open",
			   	     "uris", G_TYPE_STRV,
			   	     "new_window", G_TYPE_BOOLEAN,
			   	     "new_document", G_TYPE_BOOLEAN,
			   	     "startup_timestamp", G_TYPE_UINT,
			   	     "line_position", G_TYPE_INT,
			   	     "encoding", G_TYPE_STRING,
			   	     "screen_number", G_TYPE_INT,
			   	     "workspace", G_TYPE_INT,
			   	     "viewport_x", G_TYPE_INT,
			   	     "viewport_y", G_TYPE_INT,
			   	     "display_name", G_TYPE_STRING,
			   	     NULL);
	
	screen = gdk_screen_get_default ();
	display = gdk_screen_get_display (screen);
	gedit_utils_get_current_viewport (screen, &viewport_x, &viewport_y);
	
	/* now set values for the message */
	gedit_message_set (message,
			   "uris", uris,
			   "new_window", new_window_option,
			   "new_document", new_document_option,
			   "startup_timestamp", startup_timestamp,
			   "line_position", line_position,
			   "encoding", encoding_charset,
			   "screen_number", gdk_screen_get_number (screen),
			   "workspace", gedit_utils_get_current_workspace (screen),
			   "viewport_x", viewport_x,
			   "viewport_y", viewport_y,
			   "display_name", gdk_display_get_name (display),
			   NULL);

	return message;
}

static gboolean
dispatch_dbus (guint32 startup_timestamp)
{
	GSList *files;
	gint line_position;
	DBusGProxy *bus_proxy;
	DBusGConnection *gbus;
	GError *error = NULL;
	GeditMessage *message;
	GType gtype;
	gboolean ret = TRUE;
	
	/* get files and line position */
	gedit_get_command_line_data (&files, &line_position);
	
	gbus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	
	if (!gbus)
	{
		g_warning ("Could not dispatch to dbus: %s", error->message);
		g_error_free (error);
		return FALSE;
	}

	/* Get the remote gedit message bus */
	bus_proxy = dbus_g_proxy_new_for_name (gbus, 
					       "org.gnome.gedit",
					       "/org/gnome/gedit",
					       "org.gnome.gedit.MessageBus");
	
	if (!bus_proxy)
	{
		g_warning ("Could not get gedit dbus proxy object");
		return FALSE;
	}

	message = make_message (files, line_position, startup_timestamp);
	gtype = dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE);

	if (!dbus_g_proxy_call (bus_proxy, "Send", &error,
			  	G_TYPE_STRING, "core",
			  	G_TYPE_STRING, g_slist_length (files) != 0 ? "open" : "present",
				gtype, gedit_message_get_hash (message),
			  	G_TYPE_INVALID,
			  	G_TYPE_INVALID))
	{
		g_warning ("Sending message over dbus failed: %s", error->message);
		g_error_free (error);

		ret = FALSE;
	}

	if (new_document_option && g_slist_length (files) != 0)
	{
		/* send also a new_document message */
		if (!dbus_g_proxy_call (bus_proxy, "Send", &error,
			  		G_TYPE_STRING, "core",
			  		G_TYPE_STRING, "new_document",
					gtype, gedit_message_get_hash (message),
			  		G_TYPE_INVALID,
			  		G_TYPE_INVALID))
		{
			g_warning ("Sending message over dbus failed: %s", error->message);
			g_error_free (error);

			ret = FALSE;
		}
	}

	/* cleanup */
	g_object_unref (message);
	g_slist_foreach (files, (GFunc)g_free, NULL);
	g_slist_free (files);
	
	return ret;
}

int
main (int argc, char *argv[])
{
	GOptionContext *context;
	GeditPluginsEngine *engine;
	GeditWindow *window;
	GeditApp *app;
	gboolean restored = FALSE;
	GError *error = NULL;
	guint32 startup_timestamp;
	
	/* Init glib threads asap */
	g_thread_init (NULL);

	/* Setup debugging */
	gedit_debug_init ();
	gedit_debug_message (DEBUG_APP, "Startup");
	
	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, GEDIT_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	startup_timestamp = get_startup_timestamp();

	/* Setup command line options */
	context = g_option_context_new (_("- Edit text files"));
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);
	g_option_context_add_group (context, gtk_get_option_group (FALSE));
	g_option_context_add_group (context, egg_sm_client_get_option_group ());

	gtk_init (&argc, &argv);

	if (!g_option_context_parse (context, &argc, &argv, &error))
	{
	        g_print(_("%s\nRun '%s --help' to see a full list of available command line options.\n"),
			error->message, argv[0]);
		g_error_free (error);
		return 1;
	}

#ifdef ENABLE_DBUS
	if (!standalone_option)
	{
		gboolean ret;
		
		gedit_debug_message (DEBUG_APP, "Start dbus service");
		ret = gedit_dbus_initialize (&error);
		
		if (error)
		{
			g_warning ("Could not initialize dbus service: %s", error->message);
			g_error_free (error);
		}
		
		if (!ret)
		{
			/* this means there is already a master gedit process */
			gedit_debug_message (DEBUG_APP, "I'm a client");
			
			if (dispatch_dbus (startup_timestamp))
			{
				/* dispatch complete, we can safely exit now */
				gdk_notify_startup_complete ();

				g_free (encoding_charset);
				g_strfreev (remaining_args);
				exit(0);
			}
		}
	}
#endif
	/* register command messages on the message bus */
	_gedit_commands_messages_register ();

	gedit_debug_message (DEBUG_APP, "Set icon");
	
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
					   GEDIT_ICONDIR);

	/* Set default icon */
	gtk_window_set_default_icon_name ("accessories-text-editor");

	/* Load user preferences */
	gedit_debug_message (DEBUG_APP, "Init prefs manager");
	gedit_prefs_manager_app_init ();

	/* Init plugins engine */
	gedit_debug_message (DEBUG_APP, "Init plugins");
	engine = gedit_plugins_engine_get_default ();

	gtk_about_dialog_set_url_hook (gedit_utils_activate_url, NULL, NULL);
	
	/* Initialize session management */
	gedit_debug_message (DEBUG_APP, "Init session manager");		
	gedit_session_init ();

	if (gedit_session_is_restored ())
		restored = gedit_session_load ();

	if (!restored)
	{
		GSList *files;
		gint line_position;

		gedit_debug_message (DEBUG_APP, "Analyze command line data");
		gedit_get_command_line_data (&files, &line_position);
		
		gedit_debug_message (DEBUG_APP, "Get default app");
		app = gedit_app_get_default ();

		gedit_debug_message (DEBUG_APP, "Create main window");
		window = gedit_app_create_window (app, NULL);

		if (files != NULL)
		{
			const GeditEncoding *encoding = NULL;

			if (encoding_charset)
			{
				encoding = gedit_encoding_get_from_charset (encoding_charset);

				if (encoding == NULL)
					g_print (_("%s: invalid encoding.\n"),
						 encoding_charset);
			}
	
			gedit_debug_message (DEBUG_APP, "Load files");
			_gedit_cmd_load_files_from_prompt (window, 
							   files, 
							   encoding, 
							   line_position);
		}
		else
		{
			gedit_debug_message (DEBUG_APP, "Create tab");
			gedit_window_create_tab (window, TRUE);
		}
		
		gedit_debug_message (DEBUG_APP, "Show window");
		gtk_widget_show (GTK_WIDGET (window));

		g_slist_foreach (files, (GFunc) g_free, NULL);
		g_slist_free (files);
	}

	gedit_debug_message (DEBUG_APP, "Start gtk-main");		
	gtk_main();

	/* We kept the original engine reference here. So let's unref it to
	 * finalize it properly. 
	 */
	g_object_unref (engine);
	gedit_prefs_manager_app_shutdown ();
	gedit_metadata_manager_shutdown ();
	
	g_free (encoding_charset);
	g_strfreev (remaining_args);

	return 0;
}

