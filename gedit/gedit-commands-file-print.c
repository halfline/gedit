/*
 * gedit-commands-file-print.c
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gedit-commands.h"
#include "gedit-window.h"
#include "gedit-debug.h"

static GtkPageSetup *page_setup = NULL;
static GtkPrintSettings *settings = NULL;

static void
do_page_setup (GtkWindow *window)
{
	GtkPageSetup *new_page_setup;

	new_page_setup = gtk_print_run_page_setup_dialog (window,
							  page_setup,
							  settings);

	if (page_setup)
		g_object_unref (page_setup);
  
	page_setup = new_page_setup;
}

//static void
//status_changed_cb (GtkPrintOperation *op,
//		   gpointer user_data)
//{
//  if (gtk_print_operation_is_finished (op))
//    {
//      active_prints = g_list_remove (active_prints, op);
//      g_object_unref (op);
//    }

//  update_statusbar ();
//}

//static GtkWidget *
//create_custom_widget (GtkPrintOperation *operation,
//		      gpointer          *data)
//{
//  GtkWidget *vbox, *hbox, *font, *label;

//  gtk_print_operation_set_custom_tab_label (operation, "Other");
//  vbox = gtk_vbox_new (FALSE, 0);
//  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);

//  hbox = gtk_hbox_new (FALSE, 8);
//  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
//  gtk_widget_show (hbox);

//  label = gtk_label_new ("Font:");
//  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
//  gtk_widget_show (label);
//  
//  font = gtk_font_button_new_with_font  (data->font);
//  gtk_box_pack_start (GTK_BOX (hbox), font, FALSE, FALSE, 0);
//  gtk_widget_show (font);
//  data->font_button = font;

//  return vbox;
//}

//static void
//custom_widget_apply (GtkPrintOperation *operation,
//		     GtkWidget *widget,
//		     PrintData *data)
//{
//  const char *selected_font;
//  selected_font = gtk_font_button_get_font_name  (GTK_FONT_BUTTON (data->font_button));
//  g_free (data->font);
//  data->font = g_strdup (selected_font);
//}

void
_gedit_cmd_file_page_setup (GtkAction   *action,
			    GeditWindow *window)
{
	gedit_debug (DEBUG_COMMANDS);

	do_page_setup (GTK_WINDOW (window));
}

void
_gedit_cmd_file_print_preview (GtkAction   *action,
			       GeditWindow *window)
{
	GeditTab *tab;

	gedit_debug (DEBUG_COMMANDS);

	tab = gedit_window_get_active_tab (window);
	if (tab == NULL)
		return;

	_gedit_tab_print_preview (tab);
}

void
_gedit_cmd_file_print (GtkAction   *action,
		       GeditWindow *window)
{
	GeditTab *tab;

	gedit_debug (DEBUG_COMMANDS);

	tab = gedit_window_get_active_tab (window);
	if (tab == NULL)
		return;

	_gedit_tab_print (tab);
}

