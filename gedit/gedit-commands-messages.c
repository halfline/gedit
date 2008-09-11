/*
 * gedit-commands-messages.c
 * This file is part of gedit
 *
 * Copyright (C) 2008  Jesse van den Kieboom  <jesse@icecrew.nl>
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

#include "gedit-commands.h"

#include "gedit-app.h"
#include "gedit-window.h"
#include "gedit-encodings.h"
#include "gedit-message-bus.h"

#include <gdk/gdkx.h>
#include <string.h>

#define BUS_CONNECT(domain, name, callback) gedit_message_bus_connect (bus, domain, name, callback, NULL, NULL)

static GdkDisplay *
display_open_if_needed (const gchar *name)
{
	GSList *displays;
	GSList *l;
	GdkDisplay *display = NULL;

	displays = gdk_display_manager_list_displays (gdk_display_manager_get ());

	for (l = displays; l != NULL; l = l->next)
	{
		if (strcmp (gdk_display_get_name ((GdkDisplay *) l->data), name) == 0)
		{
			display = l->data;
			break;
		}
	}

	g_slist_free (displays);

	return display != NULL ? display : gdk_display_open (name);
}

static GeditWindow *
get_window_from_message (GeditMessage *message)
{
	gboolean new_window = FALSE;
	GeditApp *app;
	const gchar *invalid_key;
	
	/* optional parameters, used to specify correct workspace/viewport */
	GdkScreen *screen = NULL;
	gint screen_number = -1;
	gint workspace = -1;
	gint viewport_x = -1;
	gint viewport_y = -1;
	gchar *display_name = NULL;

	if (!gedit_message_get_type_safe (message,
				          &invalid_key,
				          "new_window", G_TYPE_BOOLEAN, &new_window,
				          "screen_number", G_TYPE_INT, &screen_number,
				          "workspace", G_TYPE_INT, &workspace,
				          "viewport_x", G_TYPE_INT, &viewport_x,
				          "viewport_y", G_TYPE_INT, &viewport_y,
				          "display_name", G_TYPE_STRING, &display_name,
				          NULL))
	{
		g_warning ("Message contains invalid value for key `%s'", invalid_key);
		g_free (display_name);
		return NULL;
	}

	app = gedit_app_get_default ();
	
	/* get correct screen using the display_name and screen_number */
	if (display_name != NULL)
	{
		GdkDisplay *display;
		
		display = display_open_if_needed (display_name);
		screen = gdk_display_get_screen (display, screen_number == -1 ? 0 : screen_number);
	}
	
	if (new_window)
		return gedit_app_create_window (app, screen);
	else if (screen != NULL)
		return _gedit_app_get_window_in_viewport (app,
							  screen,
							  workspace == -1 ? 0 : workspace,
							  viewport_x == -1 ? 0 : viewport_x,
							  viewport_y == -1 ? 0 : viewport_y);
	else
		return gedit_app_get_active_window (app);
}

static void
set_interaction_time_and_present (GeditWindow *window,
				  guint32      startup_timestamp)
{
	/* set the proper interaction time on the window.
	 * Fall back to roundtripping to the X server when we
	 * don't have the timestamp, e.g. when launched from
	 * terminal. We also need to make sure that the window
	 * has been realized otherwise it will not work. lame.
	 */
	if (!GTK_WIDGET_REALIZED (window))
		gtk_widget_realize (GTK_WIDGET (window));

	if (startup_timestamp <= 0)
		startup_timestamp = gdk_x11_get_server_time (GTK_WIDGET (window)->window);

	gdk_x11_window_set_user_time (GTK_WIDGET (window)->window,
				      startup_timestamp);

	gtk_window_present (GTK_WINDOW (window));
}

static void
on_message_commands_open (GeditMessageBus *bus, 
			  GeditMessage    *message,
			  gpointer         userdata)
{
	GStrv uris = NULL;
	gchar *encoding_charset = NULL;
	gint line_position = 0;
	const GeditEncoding *encoding = NULL;
	GSList *uri_list = NULL;
	const gchar *invalid_key;
	GStrv ptr;
	guint32 startup_timestamp = 0;
	GeditWindow *window;

	if (gedit_message_has_key (message, "uris") && 
	    gedit_message_type_check (message, NULL, G_TYPE_STRV, "uris", NULL))
	{
		/* good, this is what we want, list of uris */
		gedit_message_get (message, "uris", &uris, NULL);
		
	}
	else if (gedit_message_has_key (message, "uri") &&
	         gedit_message_type_check (message, NULL, G_TYPE_STRING, "uri", NULL))
	{
		/* single uri is also supported, we put it in uris */
		uris = g_new0 (gchar *, 2);
		gedit_message_get (message, "uri", uris, NULL);
	}
	
	/* check if we got at least one uri */
	if (uris == NULL || *uris == NULL)
	{
		g_strfreev (uris);
		return;
	}
	
	window = get_window_from_message (message);
	
	if (window == NULL)
	{
		g_strfreev (uris);
		return;
	}
	
	/* get all the other parameters */
	if (!gedit_message_get_type_safe (message,
				          &invalid_key,
				          "encoding", G_TYPE_STRING, &encoding_charset,
				          "line_position", G_TYPE_INT, &line_position,
				          "startup_timestamp", G_TYPE_UINT, &startup_timestamp,
				          NULL))
	{
		g_warning ("Message commands.open contains invalid value for key `%s'", invalid_key);

		g_strfreev (uris);
		g_free (encoding_charset);
		return;
	}

	if (encoding_charset != NULL)
		encoding = gedit_encoding_get_from_charset (encoding_charset);

	ptr = uris;
	
	while (*ptr)
	{
		uri_list = g_slist_prepend (uri_list, *ptr);
		ptr++;
	}
	
	uri_list = g_slist_reverse (uri_list);
	
	_gedit_cmd_load_files_from_prompt (window, uri_list, encoding, line_position);
	
	/* set the proper interaction time on the window.
	 * Fall back to roundtripping to the X server when we
	 * don't have the timestamp, e.g. when launched from
	 * terminal. We also need to make sure that the window
	 * has been realized otherwise it will not work. lame.
	 */
	set_interaction_time_and_present (window, startup_timestamp);

	g_strfreev (uris);
	g_free (encoding_charset);

	g_slist_free (uri_list);
}

static void
on_message_commands_new_document (GeditMessageBus *bus,
				  GeditMessage    *message,
				  gpointer         userdata)
{
	GeditWindow *window;
	const gchar *invalid_key;
	guint32 startup_timestamp = 0;
	
	window = get_window_from_message (message);
	
	if (window == NULL)
		return;

	if (!gedit_message_get_type_safe (message,
				          &invalid_key,
				          "startup_timestamp", G_TYPE_UINT, &startup_timestamp,
				          NULL))
	{
		g_warning ("Message contains invalid value for key `%s'", invalid_key);
		return;
	}
	
	gedit_window_create_tab (window, TRUE);
	
	/* set the proper interaction time on the window.
	 * Fall back to roundtripping to the X server when we
	 * don't have the timestamp, e.g. when launched from
	 * terminal. We also need to make sure that the window
	 * has been realized otherwise it will not work. lame.
	 */
	set_interaction_time_and_present (window, startup_timestamp);
}

static void
on_message_commands_present (GeditMessageBus *bus,
			     GeditMessage    *message,
			     gpointer         userdata)
{
	GeditWindow *window;
	const gchar *invalid_key;
	guint32 startup_timestamp = 0;
	gboolean new_document = FALSE;
	GeditDocument *doc;

	window = get_window_from_message (message);
	
	if (window == NULL)
		return;

	if (!gedit_message_get_type_safe (message,
				          &invalid_key,
				          "new_document", G_TYPE_BOOLEAN, &new_document,
				          "startup_timestamp", G_TYPE_UINT, &startup_timestamp,
				          NULL))
	{
		g_warning ("Message contains invalid value for key `%s'", invalid_key);
		return;
	}
	
	
	doc = gedit_window_get_active_document (window);

	if (doc == NULL ||
	    !gedit_document_is_untouched (doc) ||
	    new_document)
		gedit_window_create_tab (window, TRUE);
	
	/* set the proper interaction time on the window.
	 * Fall back to roundtripping to the X server when we
	 * don't have the timestamp, e.g. when launched from
	 * terminal. We also need to make sure that the window
	 * has been realized otherwise it will not work. lame.
	 */
	set_interaction_time_and_present (window, startup_timestamp);
}

void
_gedit_commands_messages_register ()
{
	GeditMessageBus *bus;
	
	/* register message handlers on the message bus */
	bus = gedit_message_bus_get_default ();
	
	/* open list of uris, or single uri */
	BUS_CONNECT ("core", "open", on_message_commands_open);
	
	/* alias "open_uris", "open_uri", "load", "load_uris", "load_uri" */
	BUS_CONNECT ("core", "open_uris", on_message_commands_open);
	BUS_CONNECT ("core", "open_uri", on_message_commands_open);
	BUS_CONNECT ("core", "load", on_message_commands_open);
	BUS_CONNECT ("core", "load_uris", on_message_commands_open);
	BUS_CONNECT ("core", "load_uri", on_message_commands_open);
	
	/* new document message, used by gedit startup */
	BUS_CONNECT ("core", "new_document", on_message_commands_new_document);
	
	BUS_CONNECT ("core", "present", on_message_commands_present);
}
