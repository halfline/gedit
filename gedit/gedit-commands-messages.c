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

static void
on_message_commands_open (GeditMessageBus *bus, 
			  GeditMessage    *message,
			  gpointer         userdata)
{
	GStrv uris = NULL;
	gchar *encoding_charset = NULL;
	gint line_position = 0;
	gboolean new_window = FALSE;
	const GeditEncoding *encoding = NULL;
	GeditWindow *window;
	GeditApp *app;
	GSList *uri_list = NULL;
	GStrv ptr;
	guint32 startup_timestamp = 0;
	
	/* optional parameters, used to specify correct workspace/viewport */
	GdkScreen *screen = NULL;
	gint screen_number = -1;
	gint workspace = -1;
	gint viewport_x = -1;
	gint viewport_y = -1;
	gchar *display_name = NULL;

	gedit_message_get (message,
			   "uris", &uris,
			   "encoding", &encoding_charset,
			   "line_position", &line_position,
			   "new_window", &new_window,
			   "screen_number", &screen_number,
			   "workspace", &workspace,
			   "viewport_x", &viewport_x,
			   "viewport_y", &viewport_y,
			   "display_name", &display_name,
			   "startup_timestamp", &startup_timestamp,
			   NULL);

	if (uris == NULL)
		return;
	
	if (*uris == NULL)
	{
		g_strfreev (uris);
		return;
	}

	if (encoding_charset != NULL)
		encoding = gedit_encoding_get_from_charset (encoding_charset);

	app = gedit_app_get_default ();
	
	/* get correct screen using the display_name and screen_number */
	if (display_name != NULL)
	{
		GdkDisplay *display;
		
		display = display_open_if_needed (display_name);
		screen = gdk_display_get_screen (display, screen_number == -1 ? 0 : screen_number);
	}
	
	if (new_window)
		window = gedit_app_create_window (app, screen);
	else if (screen != NULL)
		window = _gedit_app_get_window_in_viewport (app,
							    screen,
							    workspace == -1 ? 0 : workspace,
							    viewport_x == -1 ? 0 : viewport_x,
							    viewport_y == -1 ? 0 : viewport_y);
	else
		window = gedit_app_get_active_window (app);

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
	if (!GTK_WIDGET_REALIZED (window))
		gtk_widget_realize (GTK_WIDGET (window));

	if (startup_timestamp <= 0)
		startup_timestamp = gdk_x11_get_server_time (GTK_WIDGET (window)->window);

	gdk_x11_window_set_user_time (GTK_WIDGET (window)->window,
				      startup_timestamp);

	gtk_window_present (GTK_WINDOW (window));

	g_strfreev (uris);
	g_free (encoding_charset);
	g_slist_free (uri_list);
}

void
_gedit_commands_messages_register ()
{
	GeditMessageBus *bus;
	
	/* register message handlers on the message bus */
	bus = gedit_message_bus_get_default ();
	
	BUS_CONNECT ("commands", "open", on_message_commands_open);
}
