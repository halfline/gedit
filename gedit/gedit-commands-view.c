/*
 * gedit-view-commands.c
 * This file is part of gedit
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi
 * Copyright (C) 2002-2005 Paolo Maggi
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
 * Modified by the gedit Team, 1998-2005. See the AUTHORS file for a
 * list of people on the gedit Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include "gedit-commands.h"
#include "gedit-debug.h"
#include "gedit-window.h"
#include "gedit-window-private.h"
#include "gedit-highlight-mode-dialog.h"

void
_gedit_cmd_view_toggle_fullscreen_mode (GSimpleAction *action,
                                        GVariant      *parameter,
                                        gpointer       user_data)
{
	GeditWindow *window = GEDIT_WINDOW (user_data);

	gedit_debug (DEBUG_COMMANDS);

	if (_gedit_window_is_fullscreen (window))
		_gedit_window_unfullscreen (window);
	else
		_gedit_window_fullscreen (window);
}

void
_gedit_cmd_view_leave_fullscreen_mode (GSimpleAction *action,
                                       GVariant      *parameter,
                                       gpointer       user_data)
{
	GeditWindow *window = GEDIT_WINDOW (user_data);

	_gedit_window_unfullscreen (window);
}

static void
on_language_selected (GeditHighlightModeDialog *dlg,
                      GtkSourceLanguage        *language,
                      GeditWindow              *window)
{
	GeditDocument *doc;

	doc = gedit_window_get_active_document (window);

	if (!doc)
		return;

	gedit_document_set_language (doc, language);
}

void
_gedit_cmd_view_highlight_mode (GSimpleAction *action,
                                GVariant      *parameter,
                                gpointer       user_data)
{
	GtkWindow *window = GTK_WINDOW (user_data);
	GtkWidget *dlg;

	dlg = gedit_highlight_mode_dialog_new (window);
	g_signal_connect (dlg, "language-selected",
	                  G_CALLBACK (on_language_selected), window);

	gtk_widget_show (GTK_WIDGET (dlg));
}

/* ex:set ts=8 noet: */
