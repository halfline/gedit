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

#include <math.h>

#include <pango/pangocairo.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gtk/gtkprintoperation.h>
#include <gtksourceview/gtksourceprintcompositor.h>

#include "gedit-commands.h"
#include "gedit-print-preview.h"
#include "gedit-window.h"
#include "gedit-debug.h"

#define LINE_NUMBERS_FONT_NAME   "Monospace 6"
#define HEADER_FONT_NAME   "Serif Italic 12"
#define FOOTER_FONT_NAME   "SansSerif Bold 8"

static GtkPageSetup *page_setup = NULL;
static GtkPrintSettings *settings = NULL;
static GList *active_prints = NULL;

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

static void
status_changed_cb (GtkPrintOperation *op,
		   gpointer user_data)
{
  if (gtk_print_operation_is_finished (op))
    {
      active_prints = g_list_remove (active_prints, op);
      g_object_unref (op);
    }

//  update_statusbar ();
}

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

static gboolean 
run_preview (GtkPrintOperation        *op,
	     GtkPrintOperationPreview *gtk_preview,
	     GtkPrintContext          *context,
	     GtkWindow                *parent,
	     GeditTab                 *tab)
{
	GtkWidget *window;
	GtkWidget *preview;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (window), 400, 600);

	preview = gedit_print_preview_new (op, gtk_preview, context);
	gtk_container_add (GTK_CONTAINER (window), preview);

	gtk_widget_show_all (window);

	return TRUE;
}

static void
print_done (GtkPrintOperation *op,
	    GtkPrintOperationResult res,
	    gpointer *data)
{
  GError *error = NULL;

  if (res == GTK_PRINT_OPERATION_RESULT_ERROR)
    {

      GtkWidget *error_dialog;
      
      gtk_print_operation_get_error (op, &error);

      error_dialog = gtk_message_dialog_new (NULL, //GTK_WINDOW (main_window),
					     GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_MESSAGE_ERROR,
					     GTK_BUTTONS_CLOSE,
					     "Error printing file:\n%s",
					     error ? error->message : "no details");
      g_signal_connect (error_dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
      gtk_widget_show (error_dialog);
    }
  else if (res == GTK_PRINT_OPERATION_RESULT_APPLY)
    {
      if (settings != NULL)
	g_object_unref (settings);
      settings = g_object_ref (gtk_print_operation_get_print_settings (op));
    }

//  g_object_unref (print_data->buffer);
//  g_free (print_data->font);
//  g_free (print_data);
  
  if (!gtk_print_operation_is_finished (op))
    {
      g_object_ref (op);
      active_prints = g_list_append (active_prints, op);
//      update_statusbar ();
      
      /* This ref is unref:ed when we get the final state change */
      g_signal_connect (op, "status_changed",
			G_CALLBACK (status_changed_cb), NULL);
    }
}

static void
begin_print (GtkPrintOperation        *operation, 
	     GtkPrintContext          *context,
	     GtkSourcePrintCompositor *compositor)
{
	g_debug ("begin_print");
}

static gboolean
paginate (GtkPrintOperation        *operation, 
	  GtkPrintContext          *context,
	  GtkSourcePrintCompositor *compositor)
{
	g_print ("Pagination progress: %.2f %%\n", gtk_source_print_compositor_get_pagination_progress (compositor) * 100.0);
	
	if (gtk_source_print_compositor_paginate (compositor, context))
	{
		gint n_pages;

		g_assert (gtk_source_print_compositor_get_pagination_progress (compositor) == 1.0);
		g_print ("Pagination progress: %.2f %%\n", gtk_source_print_compositor_get_pagination_progress (compositor) * 100.0);
		        
		n_pages = gtk_source_print_compositor_get_n_pages (compositor);
		gtk_print_operation_set_n_pages (operation, n_pages);


		
		return TRUE;
	}
     
	return FALSE;
}

static void
draw_page (GtkPrintOperation        *operation,
	   GtkPrintContext          *context,
	   gint                      page_nr,
	   GtkSourcePrintCompositor *compositor)
{
	g_debug ("draw_page %d", page_nr);

	gtk_source_print_compositor_draw_page (compositor, context, page_nr);
}

static void
end_print (GtkPrintOperation        *operation, 
	   GtkPrintContext          *context,
	   GtkSourcePrintCompositor *compositor)
{
	g_object_unref (compositor);
}

void
do_print_or_preview (GeditWindow             *window,
		     GtkPrintOperationAction  print_action)
{
	GeditTab *tab;
	GeditView *view;
	GeditDocument *doc;
	GtkSourcePrintCompositor *compositor;
	GtkPrintOperation *operation;
	gchar *name;

	tab = gedit_window_get_active_tab (window);
	if (tab == NULL)
		return;

	view = gedit_tab_get_view (tab);
	if (view == NULL)
		return;

	doc = gedit_tab_get_document (tab);
	if (doc == NULL)
		return;

	compositor = gtk_source_print_compositor_new (GTK_SOURCE_BUFFER (doc));

	gtk_source_print_compositor_set_tab_width (compositor,
						   gtk_source_view_get_tab_width (GTK_SOURCE_VIEW (view)));
	gtk_source_print_compositor_set_wrap_mode (compositor,
						   gtk_text_view_get_wrap_mode (GTK_TEXT_VIEW (view)));

	gtk_source_print_compositor_set_print_line_numbers (compositor, 5);

	/* To test line numbers font != text font */
	gtk_source_print_compositor_set_line_numbers_font_name (compositor,
								LINE_NUMBERS_FONT_NAME);

	gtk_source_print_compositor_set_header_format (compositor,
						       TRUE,
						       "Printed on %A",
						       "test-widget",
						       "%F");

	name = gedit_document_get_short_name_for_display (doc);
	gtk_source_print_compositor_set_footer_format (compositor,
						       TRUE,
						       "%T",
						       name,
						       "Page %N/%Q");
	g_free (name);

	gtk_source_print_compositor_set_print_header (compositor, TRUE);
	gtk_source_print_compositor_set_print_footer (compositor, TRUE);

	gtk_source_print_compositor_set_header_font_name (compositor,
							  HEADER_FONT_NAME);

	gtk_source_print_compositor_set_footer_font_name (compositor,
							  FOOTER_FONT_NAME);
	
	operation = gtk_print_operation_new ();
	
  	g_signal_connect (operation, "begin-print", 
			  G_CALLBACK (begin_print), compositor);
  	g_signal_connect (operation, "paginate", 
			  G_CALLBACK (paginate), compositor);		          
	g_signal_connect (operation, "draw-page", 
			  G_CALLBACK (draw_page), compositor);
	g_signal_connect (operation, "preview",
			  G_CALLBACK (run_preview), tab);
//	g_signal_connect (operation, "create_custom_widget",
//			  G_CALLBACK (create_custom_widget), print_data);
//	g_signal_connect (operation, "custom_widget_apply",
//			  G_CALLBACK (custom_widget_apply), print_data);
	g_signal_connect (operation, "end-print", 
			  G_CALLBACK (end_print), compositor);
	g_signal_connect (operation, "done",
			  G_CALLBACK (print_done), compositor);

	gtk_print_operation_run (operation, 
				 print_action,
				 GTK_WINDOW (window),
				 NULL);

	g_object_unref (operation);
}

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
	gedit_debug (DEBUG_COMMANDS);

	do_print_or_preview (window, GTK_PRINT_OPERATION_ACTION_PREVIEW);
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

