/* vi:set ts=8 sts=0 sw=8:
 *
 * gEdit
 * Copyright (C) 1998 Alex Roberts and Evan Lawrence
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <string.h>
#include <config.h>
#include <gnome.h>
#include <gtk/gtk.h>
#include <glib.h>

#include "main.h"
#include "gE_document.h"
#include "gE_files.h"
#include "gE_prefs_box.h"
#include "gE_plugin_api.h"
#include "commands.h"
#include "gE_mdi.h"
#include "gE_print.h"
#include "menus.h"
#include "toolbar.h"
#include "search.h"
#include "gE_prefs.h"

extern GList *plugins;
gE_window *window;

/* local definitions */

/* we could probably use a menu factory here */
typedef struct {
	char *name;
	GtkMenuCallback cb;
} popup_t;

/*static gint gE_destroy_window(GtkWidget *, GdkEvent *event, gE_data *data);*/
static void gE_window_create_popupmenu(gE_data *);
static void doc_swaphc_cb(GtkWidget *w, gpointer cbdata);
static gboolean gE_document_popup_cb(GtkWidget *widget,GdkEvent *ev); 
static void gE_msgbar_timeout_add(gE_window *window);

static popup_t popup_menu[] =
{
	{ N_("Cut"), edit_cut_cb },
	{ N_("Copy"), edit_copy_cb },
	{ N_("Paste"), edit_paste_cb },
	{ "<separator>", NULL },
/*	{ N_("Open in new window"), file_open_in_new_win_cb },*/
	{ N_("Save"), file_save_cb },
	{ N_("Close"), file_close_cb },
	{ N_("Print"), file_print_cb },
	{ "<separator>", NULL },
	{ N_("Open (swap) .c/.h file"), doc_swaphc_cb },
	{ NULL, NULL }
};

static char *lastmsg = NULL;
static gint msgbar_timeout_id;

/*gE_window */
void
gE_window_new(GnomeMDI *mdi, GnomeApp *app)
{
        GnomeUIInfo * gedit_menu;
	gE_window *w;
	gE_data *data;
	GtkWidget *box1, *box2, *tmp;
	gint *ptr; /* For plugin stuff. */

	static GtkTargetEntry drag_types[] =
	{
		{ "text/uri-list", 0, 0 },
	};
	static gint n_drag_types = sizeof (drag_types) / sizeof (drag_types [0]);

	/* various initializations */
	w = g_malloc0(sizeof(gE_window));
	w->search = g_malloc0(sizeof(gE_search));

	settings->auto_indent = TRUE;
	settings->show_tabs = TRUE;
/*	settings->tab_pos = GTK_POS_TOP;*/
	settings->splitscreen = FALSE;

	settings->show_status = TRUE;
	settings->have_toolbar = TRUE;

	ptr = g_new(int, 1);
	*ptr = ++last_assigned_integer;
	g_hash_table_insert(win_int_to_pointer, ptr, w);
	g_hash_table_insert(win_pointer_to_int, w, ptr);
	
/*	data = g_malloc0(sizeof(gE_data));

	w->window = gnome_app_new("gEdit", GEDIT_ID);

	data->window = w;*/
/*	gtk_signal_connect(GTK_OBJECT(w->window), "delete_event",
		GTK_SIGNAL_FUNC(gE_destroy_window), data);
	gtk_window_set_wmclass(GTK_WINDOW(w->window), "gEdit", "gedit");
	gtk_window_set_title(GTK_WINDOW(w->window), GEDIT_ID);*/
	gtk_widget_set_usize(GTK_WIDGET(app), 630, 390);
/*	gtk_window_set_policy(GTK_WINDOW(w->window), TRUE, TRUE, FALSE);
	gtk_container_border_width(GTK_CONTAINER(w->window), 0);*/
	box1 = gtk_vbox_new(FALSE, 0);

	/* popup menu (activated when clicking on mouse button 3) */
/*	gE_window_create_popupmenu(data);
*/
	/* main menu */
/*	gedit_menu = gE_menus_init(w, data);

	gnome_app_set_contents(GNOME_APP(w->window), box1);
*/
	/* toolbar */
/*	gE_create_toolbar(w, data);
*/
	/* add a new document to the window */
/*	gE_document_new(w);
	gtk_box_pack_start(GTK_BOX(box1), w->notebook, TRUE, TRUE, 0);
*/
	/* statusbar */
	w->statusbar = gnome_appbar_new (FALSE, TRUE, GNOME_PREFERENCES_USER);
	gnome_app_set_statusbar (app, GTK_WIDGET (w->statusbar));
/*	gE_msgbar_timeout_add(w);
*/
	gnome_app_install_menu_hints(app, gnome_mdi_get_menubar_info(app));

	/* line and column indicators */

	tmp = gtk_label_new(_("Column:"));
	gtk_box_pack_start(GTK_BOX(w->statusbar), tmp, FALSE, FALSE, 1);
	gtk_widget_show(tmp);

	w->col_label = gtk_label_new("0");
	gtk_box_pack_start(GTK_BOX(w->statusbar), w->col_label, FALSE, FALSE, 1);
	gtk_widget_set_usize(w->col_label, 40, 0);
	gtk_widget_show(w->col_label);

	tmp = gtk_button_new_with_label(_("Line"));
	gtk_signal_connect(GTK_OBJECT(tmp), "clicked",
		GTK_SIGNAL_FUNC(count_lines_cb), w);
	GTK_WIDGET_UNSET_FLAGS(tmp, GTK_CAN_FOCUS);
	gtk_box_pack_start(GTK_BOX(w->statusbar), tmp, FALSE, FALSE, 0);
	gtk_widget_show(tmp);

	w->statusbox = w->statusbar;

	/* finish up */
	gtk_widget_show(w->statusbar);
	gtk_widget_show(box1);
/*	gtk_widget_show(w->notebook);
	gtk_widget_show(w->window);
*/
	gE_get_settings ();

	gE_set_menu_toggle_states();

	g_list_foreach(plugins, (GFunc) add_plugins_to_window, app);

/*	recent_update(w);*/
	window_list = g_list_append(window_list, (gpointer) w);

/*	gE_window_refresh (w);
	*/
	gtk_drag_dest_set (GTK_WIDGET(app),
		GTK_DEST_DEFAULT_ALL,
		drag_types, n_drag_types,
		GDK_ACTION_COPY);
		
	gtk_signal_connect (GTK_OBJECT (app),
		"drag_data_received",
		GTK_SIGNAL_FUNC (filenames_dropped), NULL);
	
/*	gtk_signal_emit_by_name (GTK_OBJECT (w->window), "check_resize");
	gtk_widget_grab_focus (GTK_WIDGET (gE_document_current ()->text));*/
	return;
} /* gE_window_new */

void gE_window_set_auto_indent (gint auto_indent)
{
	settings->auto_indent = auto_indent;
}

void gE_window_set_status_bar (gint show_status)
{
	settings->show_status = show_status;
	if (show_status)
		gtk_widget_show (GTK_WIDGET(window->statusbar)->parent);
	else
		gtk_widget_hide (GTK_WIDGET (window->statusbar)->parent);
}

/* gE_document_new: Relocated to gE_mdi.[ch] */

/* gE_document_new_with_file: Relocated to gE_mdi[ch] */

/* gE_docuemnt_current: Relocated to gE_mdi.[ch] */

void gE_document_set_split_screen (gE_document *doc, gint split_screen)
{
	/*if (!doc->split_parent)
		return;

	if (split_screen)
	  {
	   	gtk_widget_show (doc->split_parent);
	   	settings->splitscreen = TRUE;
	  }
	else
	  {
		gtk_widget_hide (doc->split_parent);
		settings->splitscreen = FALSE;
	  }*/
}


void gE_document_set_word_wrap (gE_document *doc, gint word_wrap)
{
	doc->word_wrap = word_wrap;
	gtk_text_set_word_wrap (GTK_TEXT (doc->text), doc->word_wrap);
}

void gE_document_set_line_wrap (gE_document *doc, gint line_wrap)
{
	doc->line_wrap = line_wrap;
	gtk_text_set_line_wrap (GTK_TEXT (doc->text), doc->line_wrap);
}

void gE_document_set_read_only (gE_document *doc, gint read_only)
{
gchar RO_label[255];
gchar *fname;

	doc->read_only = read_only;
	gtk_text_set_editable (GTK_TEXT (doc->text), !doc->read_only);
	
	if(read_only)
	{
	  sprintf(RO_label, "RO - %s", GNOME_MDI_CHILD(doc)->name);
	  gnome_mdi_child_set_name (GNOME_MDI_CHILD (doc), RO_label);
	}
	else
	{
	 if (doc->filename)
	   gnome_mdi_child_set_name (GNOME_MDI_CHILD(doc),
	   					     g_basename(doc->filename));
	 else
	   gnome_mdi_child_set_name (GNOME_MDI_CHILD(doc), _(UNTITLED));
	}
	 if (doc->split_screen)
		gtk_text_set_editable
			(GTK_TEXT (doc->split_screen), !doc->read_only);
}



void
child_switch (GnomeMDI *mdi, gE_document *doc)
{
/*FIXME	gE_document *doc;
	gchar *title;

	g_assert(window != NULL);
	g_assert(window->window != NULL);

	if (gE_documents == NULL)
		return;	

	gtk_widget_grab_focus(gE_document_current()->text);
	title = g_malloc0(strlen(GEDIT_ID) +
		strlen(gE_document_current()->filename) + 4);
	sprintf(title, "%s - %s",
		gE_document_current()->filename,
		GEDIT_ID);
	gtk_window_set_title(GTK_WINDOW(window->window), title);
	g_free(title);*/

}
/*	umm.. FIXME?
static gint
gE_destroy_window (GtkWidget *widget, GdkEvent *event, gE_data *data)
{
	window_close_cb(widget, data);
	return TRUE;
}
*/

/*
 * PUBLIC: gE_messagebar_set
 *
 * sets the message/status bar.  remembers the last message set so that
 * a duplicate one won't be set on top of the current one.
 */
void
gE_msgbar_set(char *msg)
{
/*	if (lastmsg == NULL || strcmp(lastmsg, msg)) {
		gnome_appbar_set_status (GNOME_APPBAR (window->statusbar), msg);
		if (lastmsg)
			g_free(lastmsg);
		lastmsg = g_strdup(msg);
		gtk_timeout_remove(msgbar_timeout_id);
		gE_msgbar_timeout_add(window);
	}
*/
	gnome_app_flash (mdi->active_window, msg);
}



/*
 * PRIVATE: gE_window_create_popupmenu
 *
 * Creates the popupmenu for the editor 
 */
static void
gE_window_create_popupmenu(gE_data *data)
{
	GtkWidget *tmp;
	popup_t *pp = popup_menu;
	gE_window *window = data->window;

	window->popup = gtk_menu_new();
	while (pp && pp->name != NULL) {
		if (strcmp(pp->name, "<separator>") == 0)
			tmp = gtk_menu_item_new();
		else
			tmp = gtk_menu_item_new_with_label(N_(pp->name));

		gtk_menu_append(GTK_MENU(window->popup), tmp);
		gtk_widget_show(tmp);

		if (pp->cb)
			gtk_signal_connect(GTK_OBJECT(tmp), "activate",
				GTK_SIGNAL_FUNC(pp->cb), data);
		pp++;
	}
} /* gE_window_create_popupmenu */


/*
 * PRIVATE: gE_document_popup_cb
 *
 * shows popup when user clicks on 3 button on mouse
 */
static gboolean
gE_document_popup_cb(GtkWidget *widget, GdkEvent *ev)
{
	if (ev->type == GDK_BUTTON_PRESS) {
		GdkEventButton *event = (GdkEventButton *)ev;
		if (event->button == 3) {
			gtk_menu_popup(GTK_MENU(widget), NULL, NULL, NULL, NULL,
				event->button, event->time);
			return TRUE;
		}
	}
	return FALSE;
} /* gE_document_popup_cb */


/*
 * PRIVATE: doc_swaphc_cb
 *
 * if .c file is open open .h file 
 *
 * TODO: if a .h file is open, do we swap to a .c or a .cpp?  we should put a
 * check in there.  if both exist, then probably open both files.
 */
static void
doc_swaphc_cb(GtkWidget *wgt, gpointer cbdata)
{
	size_t len;
	char *newfname;
	gE_document *doc;
	gE_window *w;
	gE_data *data = (gE_data *)cbdata;

	g_assert(data != NULL);
	w = data->window;
	g_assert(w != NULL);
	doc = gE_document_current();
	if (!doc || !doc->filename)
		return;

	newfname = NULL;
	len = strlen(doc->filename);
	while (len) {
		if (doc->filename[len] == '.')
			break;
		len--;
	};

	len++;
	if (doc->filename[len] == 'h') {
		newfname = g_strdup(doc->filename);
		newfname[len] = 'c';
	} else if (doc->filename[len] == 'H') {
		newfname = g_strdup(doc->filename);
		newfname[len] = 'C';
	} else if (doc->filename[len] == 'c') {
		newfname = g_strdup(doc->filename);
		newfname[len] = 'h';
		if (len < strlen(doc->filename) &&
			strcmp(doc->filename + len, "cpp") == 0)
			newfname[len+1] = '\0';
	} else if (doc->filename[len] == 'C') {
		newfname = g_strdup(doc->filename);
		if (len < strlen(doc->filename) &&
				strcmp(doc->filename + len, "CPP") == 0) {
			newfname[len] = 'H';
			newfname[len+1] = '\0';
		} else
			newfname[len] = 'H';
	}

	if (!newfname)
		return;

	/* hmm maybe whe should check if the file exist before we try
	 * to open.  this will be fixed later.... */
	doc = gE_document_new_with_file (newfname);
} /* doc_swaphc_cb */

/* the end */

#ifdef WITH_GMODULE_PLUGINS

gE_document
*gE_document_new_container(gE_window *w, gchar *title, gint with_split_screen)
{
	gE_document *doc;
	GtkWidget *table, *vscrollbar, *vpaned, *vbox;

	GtkStyle *style;
	gint *ptr; /* For plugin stuff. */

	doc = g_malloc0(sizeof(gE_document));

	ptr = g_new(int, 1);
	*ptr = ++last_assigned_integer;
	g_hash_table_insert (doc_int_to_pointer, ptr, doc);
	g_hash_table_insert (doc_pointer_to_int, doc, ptr);

	doc->window = w;

	if (w->notebook == NULL) {
		w->notebook = gtk_notebook_new ();
		gtk_notebook_set_scrollable (GTK_NOTEBOOK (w->notebook), TRUE);
	}
	
	vpaned = gtk_vbox_new (TRUE, TRUE);
	
	doc->tab_label = gtk_label_new (title);
	GTK_WIDGET_UNSET_FLAGS (doc->tab_label, GTK_CAN_FOCUS);
	doc->filename = NULL;
	doc->word_wrap = TRUE;
	doc->line_wrap = TRUE;
	doc->read_only = FALSE;
	gtk_widget_show (doc->tab_label);

	/* Create the upper split screen */
	table = gtk_table_new (2, 2, FALSE);
	gtk_table_set_row_spacing (GTK_TABLE (table), 0, 2);
	gtk_table_set_col_spacing (GTK_TABLE (table), 0, 2);
	gtk_box_pack_start (GTK_BOX (vpaned), table, TRUE, TRUE, 1);
	gtk_widget_show (table);

	/* Create it, but never gtk_widget_show () it. */
	doc->text = gtk_text_new (NULL, NULL);

	doc->viewport = gtk_viewport_new (NULL, NULL);
	gtk_table_attach_defaults (GTK_TABLE (table), doc->viewport, 0, 1, 0, 1);

	vbox = gtk_vbox_new (FALSE, FALSE);

	vscrollbar = gtk_vscrollbar_new
		(GTK_VIEWPORT (doc->viewport)->vadjustment);
	gtk_box_pack_start (GTK_BOX (vbox), vscrollbar, TRUE, TRUE, 0);
	gtk_table_attach(GTK_TABLE(table), vbox, 1, 2, 0, 1,
			 GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

	GTK_WIDGET_UNSET_FLAGS (vscrollbar, GTK_CAN_FOCUS);
	gtk_widget_show (vscrollbar);
	gtk_widget_show (vbox);

	doc->split_screen = gtk_text_new (NULL, NULL);

	if (with_split_screen) {
		/* Create the bottom split screen */
		table = gtk_table_new (2, 2, FALSE);
		gtk_table_set_row_spacing (GTK_TABLE (table), 0, 2);
		gtk_table_set_col_spacing (GTK_TABLE (table), 0, 2);
		
		gtk_box_pack_start (GTK_BOX (vpaned), table, TRUE, TRUE, 1);
		gtk_widget_show (table);

		doc->split_viewport = gtk_viewport_new (NULL, NULL);
		gtk_table_attach_defaults (GTK_TABLE (table),
					   doc->split_viewport,
					   0, 1, 0, 1);
		doc->split_parent = GTK_WIDGET (doc->split_viewport)->parent;

		vscrollbar = gtk_vscrollbar_new
			(GTK_VIEWPORT (doc->split_viewport)->vadjustment);

		gtk_table_attach (GTK_TABLE (table), vscrollbar, 1, 2, 0, 1,
				  GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

		GTK_WIDGET_UNSET_FLAGS (vscrollbar, GTK_CAN_FOCUS);
		gtk_widget_show (vscrollbar);
		gtk_widget_hide (GTK_WIDGET (doc->split_viewport)->parent);
	}
	
	gtk_widget_show (vpaned);
	gtk_notebook_append_page(GTK_NOTEBOOK(w->notebook), vpaned,
				 doc->tab_label);

	w->documents = g_list_append(w->documents, doc);

	gtk_notebook_set_page(GTK_NOTEBOOK(w->notebook),
		g_list_length(GTK_NOTEBOOK(w->notebook)->children) - 1);

	gtk_widget_grab_focus(doc->text);

	return doc;
}

#endif /* WITH_GMODULE_PLUGINS */

