/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gedit-commands.c
 * This file is part of gedit
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi 
 * Copyright (C) 2002, 2003 Paolo Maggi 
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
 * Modified by the gedit Team, 1998-2003. See the AUTHORS file for a 
 * list of people on the gedit Team.  
 * See the ChangeLog files for a list of changes. 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-help.h>
#include <libgnome/gnome-url.h>

#include "gedit-commands.h"
#include "gedit2.h"
#include "gedit-mdi-child.h"
#include "gedit-debug.h"
#include "gedit-view.h"
#include "gedit-file.h"
#include "gedit-print.h"
#include "gedit-search-bar.h"
#include "dialogs/gedit-dialogs.h"
#include "dialogs/gedit-preferences-dialog.h"
#include "dialogs/gedit-page-setup-dialog.h"

void 
gedit_cmd_file_new (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	gedit_debug (DEBUG_COMMANDS, "verbname: %s", verbname);

	gedit_file_new ();
}

void 
gedit_cmd_file_open (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	BonoboMDIChild *active_child;
	
	gedit_debug (DEBUG_COMMANDS, "");

	active_child = bonobo_mdi_get_active_child (BONOBO_MDI (gedit_mdi));

	gedit_file_open ((GeditMDIChild*) active_child);
}

void 
gedit_cmd_file_save (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GeditMDIChild *active_child;
	GtkWidget *active_view;

	gedit_debug (DEBUG_COMMANDS, "");

	active_view = gedit_get_active_view ();

	active_child = GEDIT_MDI_CHILD (bonobo_mdi_get_active_child (BONOBO_MDI (gedit_mdi)));
	if (active_child == NULL)
		return;
	
	if (active_view != NULL)
		gtk_widget_grab_focus (active_view);

	gedit_file_save (active_child, TRUE);
}

void 
gedit_cmd_file_save_as (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GeditMDIChild *active_child;
	
	gedit_debug (DEBUG_COMMANDS, "");

	active_child = GEDIT_MDI_CHILD (bonobo_mdi_get_active_child (BONOBO_MDI (gedit_mdi)));
	if (active_child == NULL)
		return;
	
	gedit_file_save_as (active_child);
}

void 
gedit_cmd_file_save_all (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	gedit_debug (DEBUG_COMMANDS, "");

	gedit_file_save_all ();
}

void 
gedit_cmd_file_revert (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GeditMDIChild *active_child;
	
	gedit_debug (DEBUG_COMMANDS, "");

	active_child = GEDIT_MDI_CHILD (bonobo_mdi_get_active_child (BONOBO_MDI (gedit_mdi)));
	if (active_child == NULL)
		return;
	
	gedit_file_revert (active_child);
}

void 
gedit_cmd_file_open_uri (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	gedit_debug (DEBUG_COMMANDS, "");

	gedit_dialog_open_uri ();
}

void
gedit_cmd_file_page_setup (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	BonoboWindow *active_window;

	gedit_debug (DEBUG_COMMANDS, "");

	active_window = gedit_get_active_window ();
	g_return_if_fail (active_window != NULL);

	gedit_show_page_setup_dialog (GTK_WINDOW (active_window));
}

void
gedit_cmd_file_print (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GeditDocument *doc;
	GtkWidget *active_view;
	
	gedit_debug (DEBUG_COMMANDS, "");

	doc = gedit_get_active_document ();
	if (doc == NULL)	
		return;
	
	active_view = gedit_get_active_view ();

	if (active_view != NULL)
		gtk_widget_grab_focus (active_view);

	gedit_print (doc);
}

void
gedit_cmd_file_print_preview (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GeditDocument *doc;

	gedit_debug (DEBUG_COMMANDS, "");

	doc = gedit_get_active_document ();
	if (doc == NULL)	
		return;
	
	gedit_print_preview (doc);
}

void 
gedit_cmd_file_close (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GtkWidget *active_view;
	
	gedit_debug (DEBUG_COMMANDS, "");

	active_view = gedit_get_active_view ();
	if (active_view == NULL)
		return;

	gedit_file_close (active_view);
}

void 
gedit_cmd_file_close_all (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	gedit_debug (DEBUG_COMMANDS, "");

	gedit_file_close_all ();
}

void 
gedit_cmd_file_exit (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	gedit_debug (DEBUG_COMMANDS, "");

	gedit_file_exit ();	
}

void 
gedit_cmd_edit_undo (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GtkWidget *active_view;
	GeditDocument *active_document;

	active_view = gedit_get_active_view ();
	g_return_if_fail (active_view);

	active_document = gedit_view_get_document (GEDIT_VIEW (active_view));
	g_return_if_fail (active_document);

	gedit_document_undo (active_document);

	gedit_view_scroll_to_cursor (GEDIT_VIEW (active_view));

	gtk_widget_grab_focus (active_view);
}

void 
gedit_cmd_edit_redo (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GtkWidget *active_view;
	GeditDocument *active_document;

	active_view = gedit_get_active_view ();
	g_return_if_fail (active_view);

	active_document = gedit_view_get_document (GEDIT_VIEW (active_view));
	g_return_if_fail (active_document);

	gedit_document_redo (active_document);

	gedit_view_scroll_to_cursor (GEDIT_VIEW (active_view));

	gtk_widget_grab_focus (active_view);
}

void 
gedit_cmd_edit_cut (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GtkWidget *active_view;

	active_view = gedit_get_active_view ();
	g_return_if_fail (active_view);

	gedit_view_cut_clipboard (GEDIT_VIEW (active_view)); 

	gtk_widget_grab_focus (active_view);
}

void 
gedit_cmd_edit_copy (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GtkWidget *active_view;

	active_view = gedit_get_active_view ();
	g_return_if_fail (active_view);

	gedit_view_copy_clipboard (GEDIT_VIEW (active_view));

	gtk_widget_grab_focus (active_view);
}

void 
gedit_cmd_edit_paste (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GtkWidget *active_view;

	active_view = gedit_get_active_view ();
	g_return_if_fail (active_view);

	gedit_view_paste_clipboard (GEDIT_VIEW (active_view));

	gtk_widget_grab_focus (active_view);
}

void 
gedit_cmd_edit_clear (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GtkWidget *active_view;

	active_view = gedit_get_active_view ();
	g_return_if_fail (active_view);

	gedit_view_delete_selection (GEDIT_VIEW (active_view));

	gtk_widget_grab_focus (active_view);
}

void
gedit_cmd_edit_select_all (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GtkTextIter start, end;
	GtkWidget *active_view;
	GeditDocument *active_doc;

	active_view = gedit_get_active_view ();
	g_return_if_fail (active_view);

	active_doc = gedit_get_active_document ();
	g_return_if_fail (active_doc);

	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (active_doc), &start, &end);
	gtk_text_buffer_select_range (GTK_TEXT_BUFFER (active_doc), &start, &end);

	gtk_widget_grab_focus (active_view);
}

void 
gedit_cmd_search_find (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GtkWidget *active_view;

	gedit_debug (DEBUG_COMMANDS, "");

	active_view = gedit_get_active_view ();

	if (active_view != NULL)
		gtk_widget_grab_focus (active_view);

	gedit_search_bar_find ();
}

static void 
search_find_again (GeditDocument *doc, gchar *last_searched_text, gboolean backward)
{
	gpointer data;
	gboolean found;
	gboolean was_wrap_around;
	gint flags = 0;

	data = g_object_get_qdata (G_OBJECT (doc), gedit_was_wrap_around_quark ());
	if (data == NULL)
	{
		was_wrap_around = TRUE;
	}
	else
	{
		was_wrap_around = GPOINTER_TO_BOOLEAN (data);
	}

	GEDIT_SEARCH_SET_FROM_CURSOR (flags, TRUE);
	
	if (!backward)
		found = gedit_document_find_next (doc, flags);
	else
		found = gedit_document_find_prev (doc, flags);
		
	if (!found && was_wrap_around)
	{
		GEDIT_SEARCH_SET_FROM_CURSOR (flags, FALSE);
		
		if (!backward)
			found = gedit_document_find_next (doc, flags);
		else
			found = gedit_document_find_prev (doc, flags);
	}

	if (!found)
	{	
		GtkWidget *message_dlg;

		message_dlg = gtk_message_dialog_new (
			GTK_WINDOW (gedit_get_active_window ()),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_INFO,
			GTK_BUTTONS_OK,
			_("The text \"%s\" was not found."), last_searched_text);

		gtk_dialog_set_default_response (GTK_DIALOG (message_dlg), GTK_RESPONSE_OK);

		gtk_window_set_resizable (GTK_WINDOW (message_dlg), FALSE);

		gtk_dialog_run (GTK_DIALOG (message_dlg));
		gtk_widget_destroy (message_dlg);
	}
	else
	{
		GtkWidget *active_view;

		active_view = gedit_get_active_view ();
		g_return_if_fail (active_view != NULL);

		gedit_view_scroll_to_cursor (GEDIT_VIEW (active_view));
	}
}

void 
gedit_cmd_search_find_next (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GeditDocument *doc;
	gchar* last_searched_text;
	
	gedit_debug (DEBUG_COMMANDS, "");

	doc = gedit_get_active_document ();
	g_return_if_fail (doc);

	last_searched_text = gedit_document_get_last_searched_text (doc);

	if (last_searched_text != NULL)
	{
		search_find_again (doc, last_searched_text, FALSE);
	}
	else
	{
		gedit_dialog_find ();
	}

	g_free (last_searched_text);
}

void 
gedit_cmd_search_find_prev (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GeditDocument *doc;
	gchar* last_searched_text;
	
	gedit_debug (DEBUG_COMMANDS, "");

	doc = gedit_get_active_document ();
	g_return_if_fail (doc);

	last_searched_text = gedit_document_get_last_searched_text (doc);

	if (last_searched_text != NULL)
	{
		search_find_again (doc, last_searched_text, TRUE);
	}
	else
	{
		gedit_dialog_find ();
	}

	g_free (last_searched_text);
}

void 
gedit_cmd_search_replace (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GtkWidget *active_view;
	
	gedit_debug (DEBUG_COMMANDS, "");

	active_view = gedit_get_active_view ();

	if (active_view != NULL)
		gtk_widget_grab_focus (active_view);

	gedit_dialog_replace ();
}

void 
gedit_cmd_search_goto_line (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	gedit_debug (DEBUG_COMMANDS, "");

	gedit_dialog_goto_line ();
}

void
gedit_cmd_settings_preferences (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	BonoboWindow *active_window;

	gedit_debug (DEBUG_COMMANDS, "");

	active_window = gedit_get_active_window ();
	g_return_if_fail (active_window != NULL);

	gedit_show_preferences_dialog (GTK_WINDOW (active_window));
}

void
gedit_cmd_documents_move_to_new_window (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GtkWidget *view;

	gedit_debug (DEBUG_COMMANDS, "");

	view = gedit_get_active_view ();
	g_return_if_fail (view != NULL);

	bonobo_mdi_move_view_to_new_window (BONOBO_MDI (gedit_mdi), view);
}

void 
gedit_cmd_help_contents (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	GError *error = NULL;

	gedit_debug (DEBUG_COMMANDS, "");

	gnome_help_display ("gedit.xml", NULL, &error);
	
	if (error != NULL)
	{
		g_warning (error->message);

		g_error_free (error);
	}
}

static void
activate_url (GtkAboutDialog *about, const gchar *url, gpointer data)
{
	gnome_url_show (url, NULL);
}

void 
gedit_cmd_help_about (BonoboUIComponent *uic, gpointer user_data, const gchar* verbname)
{
	static GtkWidget *about = NULL;

	GdkPixbuf *logo;

	const gchar *authors[] = {
		"Paolo Maggi <paolo@gnome.org>",
		"Paolo Borelli <pborelli@katamail.com>",
		"James Willcox <jwillcox@gnome.org>",
		"Chema Celorio", 
		"Federico Mena Quintero <federico@ximian.com>",
		NULL
	};

	const gchar *documenters[] = {
		"Sun GNOME Documentation Team <gdocteam@sun.com>",
		"Eric Baudais <baudais@okstate.edu>",
		NULL
	};

	const gchar *copyright = "Copyright \xc2\xa9 1998-2000 Evan Lawrence, Alex Robert\n"
				 "Copyright \xc2\xa9 2000-2002 Chema Celorio, Paolo Maggi\n"
				 "Copyright \xc2\xa9 2003-2005 Paolo Maggi";

	const gchar *comments = _("gedit is a small and lightweight text editor for the GNOME Desktop");

	/* Translators: This is a special message that shouldn't be translated
                literally. It is used in the about box to give credits to
                the translators.
                Thus, you should translate it to your name and email address.
                You can also include other translators who have contributed to
                this translation; in that case, please write them on separate
                lines seperated by newlines (\n). */
	const gchar *translator_credits = _("translator-credits");

	gedit_debug (DEBUG_COMMANDS, "");

	if (about != NULL)
	{
		gtk_window_set_transient_for (GTK_WINDOW (about),
					      GTK_WINDOW (gedit_get_active_window ()));
		gtk_window_present (GTK_WINDOW (about));

		return;
	}

	logo = gdk_pixbuf_new_from_file (GNOME_ICONDIR "/gedit-logo.png", NULL);

	gtk_about_dialog_set_url_hook (activate_url, NULL, NULL);

	
	about = g_object_new (GTK_TYPE_ABOUT_DIALOG,
			      "name", _("gedit"),
			      "version", VERSION,
			      "copyright", copyright,
			      "comments", comments,
			      "website", "http://www.gedit.org",
			      "authors", authors,
			      "documenters", documenters,
			      "translator_credits", translator_credits,
			      "logo", logo,
			      NULL);

	gtk_window_set_destroy_with_parent (GTK_WINDOW (about), TRUE);

	g_signal_connect (about, "response", G_CALLBACK (gtk_widget_destroy), NULL);
	g_signal_connect (about, "destroy", G_CALLBACK (gtk_widget_destroyed), &about);

	gtk_window_set_transient_for (GTK_WINDOW (about),
				      GTK_WINDOW (gedit_get_active_window ()));
	gtk_window_present (GTK_WINDOW (about));

	if (logo)
		g_object_unref (logo);
}

