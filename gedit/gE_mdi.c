/* vi:set ts=8 sts=0 sw=8:
 *
 * gEdit
 * Copyright (C) 1998, 1999 Alex Roberts and Evan Lawrence
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
#include "gE_prefslib.h"
#include "gE_document.h"
#include "gE_files.h"
#include "commands.h"
#include "search.h"
#include "gE_mdi.h"


static void 	  gE_document_class_init (gE_document_class *);
static void 	  gE_document_init (gE_document *);
static GtkWidget *gE_document_create_view (GnomeMDIChild *);
static void	  gE_document_destroy (GtkObject *);
static void	  gE_document_real_changed (gE_document *, gpointer);
static gchar *gE_document_get_config_string (GnomeMDIChild *child);

/* MDI Menus Stuff */

#define GE_DATA		1
#define GE_WINDOW	2

GnomeUIInfo gedit_edit_menu [] = {
        GNOMEUIINFO_MENU_CUT_ITEM(edit_cut_cb, (gpointer) GE_DATA),

        GNOMEUIINFO_MENU_COPY_ITEM(edit_copy_cb, (gpointer) GE_DATA),

	GNOMEUIINFO_MENU_PASTE_ITEM(edit_paste_cb, (gpointer) GE_DATA),

	GNOMEUIINFO_MENU_SELECT_ALL_ITEM(edit_selall_cb, (gpointer) GE_DATA),


	GNOMEUIINFO_SEPARATOR,

	{ GNOME_APP_UI_ITEM, N_("Find _Line..."),
	  N_("Search for a line"),
	  goto_line_cb, (gpointer) GE_WINDOW, NULL,
	  GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_SEARCH },

	GNOMEUIINFO_MENU_FIND_ITEM(search_cb, (gpointer) GE_DATA),

	GNOMEUIINFO_MENU_FIND_AGAIN_ITEM(search_again_cb, (gpointer) GE_DATA),

	GNOMEUIINFO_MENU_REPLACE_ITEM(search_replace_cb, (gpointer) GE_DATA),
	
	GNOMEUIINFO_END
};

GnomeUIInfo view_menu[] = {
	GNOMEUIINFO_ITEM_NONE (N_("_Add View"),
					   N_("Add a new view of the document"), gE_add_view),
	GNOMEUIINFO_ITEM_NONE (N_("_Remove View"),
					   N_("Remove view of the document"), gE_remove_view),
	GNOMEUIINFO_END
};

GnomeUIInfo doc_menu[] = {
	GNOMEUIINFO_MENU_EDIT_TREE(gedit_edit_menu),
	GNOMEUIINFO_MENU_VIEW_TREE(view_menu),
	GNOMEUIINFO_END
};


enum {
	LAST_SIGNAL
};

static gint gE_document_signals [LAST_SIGNAL];

typedef void (*gE_document_signal) (GtkObject *, gpointer, gpointer);

static GnomeMDIChildClass *parent_class = NULL;

static void gE_document_marshal (GtkObject		*object,
						 GtkSignalFunc	func,
						 gpointer		func_data,
						 GtkArg		*args)
{
	gE_document_signal rfunc;
	
	rfunc = (gE_document_signal) func;
	
	(* rfunc)(object, GTK_VALUE_POINTER(args[0]), func_data);
}

GtkType gE_document_get_type ()
{
	static GtkType doc_type = 0;
	
	if (!doc_type) 
	  {
	  	static const GtkTypeInfo doc_info = {
	  		"gE_document",
	  		sizeof (gE_document),
	  		sizeof (gE_document_class),
	  		(GtkClassInitFunc) gE_document_class_init,
	  		(GtkObjectInitFunc) gE_document_init,
	  		(GtkArgSetFunc) NULL,
	  		(GtkArgGetFunc) NULL,
	  	};
	  	doc_type = gtk_type_unique (gnome_mdi_child_get_type (), &doc_info);
	  }
	  
	  return doc_type;
}

static GtkWidget *gE_document_create_view (GnomeMDIChild *child)
{
	GtkWidget *new_view;
	gE_document *doc;
	GtkWidget *table, *vscrollbar, *vpaned, *vbox;
	GtkStyle *style;
	gint *ptr; /* For plugin stuff. */
	
	doc = GE_DOCUMENT(child);

/*	ptr = g_new(int, 1);
BORK!!	*ptr = ++last_assigned_integer;
	g_hash_table_insert(doc_int_to_pointer, ptr, doc);
	g_hash_table_insert(doc_pointer_to_int, doc, ptr);
*/

	doc->font = gE_prefs_get_char("font");
	
	vpaned = gtk_vbox_new (TRUE, TRUE);
	
/*FIXME	doc->filename = NULL;
	doc->word_wrap = TRUE;
	doc->line_wrap = TRUE;
	doc->read_only = FALSE;
	doc->splitscreen = FALSE;*/

	/* Create the upper split screen */
	table = gtk_table_new(2, 2, FALSE);
	gtk_table_set_row_spacing(GTK_TABLE(table), 0, 2);
	gtk_table_set_col_spacing(GTK_TABLE(table), 0, 2);
	gtk_box_pack_start (GTK_BOX (vpaned), table, TRUE, TRUE, 1);
	gtk_widget_show(table);

	doc->text = gtk_text_new(NULL, NULL);
	gtk_text_set_editable(GTK_TEXT(doc->text), !doc->read_only);
	gtk_text_set_word_wrap(GTK_TEXT(doc->text), doc->word_wrap);
	gtk_text_set_line_wrap(GTK_TEXT(doc->text), doc->line_wrap);

/*FIXME	gtk_signal_connect_after(GTK_OBJECT(doc->text), "button_press_event",
		GTK_SIGNAL_FUNC(gE_event_button_press), window);

	gtk_signal_connect_after (GTK_OBJECT(doc->text), "insert_text",
		GTK_SIGNAL_FUNC(auto_indent_cb), window);
*/
	gtk_table_attach_defaults(GTK_TABLE(table), doc->text, 0, 1, 0, 1);

	style = gtk_style_new();
	gtk_widget_set_style(GTK_WIDGET(doc->text), style);

	gtk_widget_set_rc_style(GTK_WIDGET(doc->text));
	gtk_widget_ensure_style(GTK_WIDGET(doc->text));
	g_free (style);
		
/*FIXME	gtk_signal_connect_object(GTK_OBJECT(doc->text), "event",
GTK_SIGNAL_FUNC(gE_document_popup_cb), GTK_OBJECT((gE_window *)(mdi->active_window)->popup));*/

	doc->changed = FALSE;
	doc->changed_id = gtk_signal_connect(GTK_OBJECT(doc->text), "changed",
		GTK_SIGNAL_FUNC(doc_changed_cb), doc);
	gtk_widget_show(doc->text);
	gtk_text_set_point(GTK_TEXT(doc->text), 0);

	vbox = gtk_vbox_new (FALSE, FALSE);

	vscrollbar = gtk_vscrollbar_new(GTK_TEXT(doc->text)->vadj);
	gtk_box_pack_start (GTK_BOX (vbox), vscrollbar, TRUE, TRUE, 0);
	gtk_table_attach(GTK_TABLE(table), vbox, 1, 2, 0, 1,
		GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

	GTK_WIDGET_UNSET_FLAGS(vscrollbar, GTK_CAN_FOCUS);
	gtk_widget_show(vscrollbar);
	gtk_widget_show (vbox);

	gtk_signal_connect (GTK_OBJECT (doc->text), "insert_text",
		GTK_SIGNAL_FUNC(doc_insert_text_cb), (gpointer) doc);
	gtk_signal_connect (GTK_OBJECT (doc->text), "delete_text",
		GTK_SIGNAL_FUNC(doc_delete_text_cb), (gpointer) doc);

	/* Create the bottom split screen */
/*FIXME	table = gtk_table_new(2, 2, FALSE);
	gtk_table_set_row_spacing(GTK_TABLE(table), 0, 2);
	gtk_table_set_col_spacing(GTK_TABLE(table), 0, 2);

	gtk_box_pack_start (GTK_BOX (vpaned), table, TRUE, TRUE, 1);
	gtk_widget_show(table);

	doc->split_screen = gtk_text_new(NULL, NULL);
	gtk_text_set_editable(GTK_TEXT(doc->split_screen), !doc->read_only);
	gtk_text_set_word_wrap(GTK_TEXT(doc->split_screen), doc->word_wrap);
	gtk_text_set_line_wrap(GTK_TEXT(doc->split_screen), doc->line_wrap);

	gtk_signal_connect_after(GTK_OBJECT(doc->split_screen),
		"button_press_event",
		GTK_SIGNAL_FUNC(gE_event_button_press), window);

	gtk_signal_connect_after(GTK_OBJECT(doc->split_screen),
		"insert_text", GTK_SIGNAL_FUNC(auto_indent_cb), window);

	gtk_table_attach_defaults(GTK_TABLE(table),
		doc->split_screen, 0, 1, 0, 1);

	doc->split_parent = GTK_WIDGET (doc->split_screen)->parent;
*/
	style = gtk_style_new();
  	gdk_font_unref (style->font);
 	 style->font = gdk_font_load (doc->font);
  	if (style->font == NULL)
  	 {
  	 style->font = gdk_font_load ("-adobe-courier-medium-r-normal-*-*-120-*-*-m-*-iso8859-1");
  	 	doc->font = "-adobe-courier-medium-r-normal-*-*-120-*-*-m-*-iso8859-1";
  	 } 
  	 
 	 gtk_widget_push_style (style);     
/*FIXME	 gtk_widget_set_style(GTK_WIDGET(doc->split_screen), style);*/
   	 gtk_widget_set_style(GTK_WIDGET(doc->text), style);
   	 gtk_widget_pop_style ();

/*FIXME	gtk_signal_connect_object(GTK_OBJECT(doc->split_screen), "event",
		GTK_SIGNAL_FUNC(gE_document_popup_cb), GTK_OBJECT(window->popup));

	gtk_widget_show(doc->split_screen);
	gtk_text_set_point(GTK_TEXT(doc->split_screen), 0);

	vscrollbar = gtk_vscrollbar_new(GTK_TEXT(doc->split_screen)->vadj);

	gtk_table_attach(GTK_TABLE(table), vscrollbar, 1, 2, 0, 1,
		GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

	GTK_WIDGET_UNSET_FLAGS(vscrollbar, GTK_CAN_FOCUS);
	gtk_widget_show(vscrollbar);

	gtk_signal_connect (GTK_OBJECT (doc->split_screen), "insert_text",
		GTK_SIGNAL_FUNC(doc_insert_text_cb), (gpointer) doc);
	gtk_signal_connect (GTK_OBJECT (doc->split_screen), "delete_text",
		GTK_SIGNAL_FUNC(doc_delete_text_cb), (gpointer) doc);
		
	settings->splitscreen = gE_prefs_get_int("splitscreen");
	if (settings->splitscreen == FALSE)
	  gtk_widget_hide (GTK_WIDGET (doc->split_screen)->parent);
*/
	gtk_widget_show (vpaned);

/*FIXME	gE_documents = g_list_append(gE_documents, doc);*/


	gtk_widget_grab_focus(doc->text);

	return vpaned;
}

static void gE_document_destroy (GtkObject *obj)
{
	gE_document *doc;
	
	doc = GE_DOCUMENT(obj);
	
	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy)
			(GTK_OBJECT (doc));
}

static void gE_document_class_init (gE_document_class *class)
{
	GtkObjectClass 		*object_class;
	GnomeMDIChildClass	*child_class;
	
	object_class = (GtkObjectClass*)class;
	child_class = GNOME_MDI_CHILD_CLASS (class);
	
	/* blarg.. signals stuff.. doc_changed? FIXME */
	
	gtk_object_class_add_signals (object_class, gE_document_signals, LAST_SIGNAL);
	
	object_class->destroy = gE_document_destroy;
	
	child_class->create_view = (GnomeMDIChildViewCreator)(gE_document_create_view);
	child_class->get_config_string = (GnomeMDIChildConfigFunc)(gE_document_get_config_string);
	
	class->document_changed = gE_document_real_changed;
	
	parent_class = gtk_type_class (gnome_mdi_child_get_type ());
	
}

static void gE_document_init (gE_document *doc)
{
	/* FIXME: This prolly needs work.. */
	
	doc->filename = NULL;
	doc->word_wrap = TRUE;
	doc->line_wrap = TRUE;
	doc->read_only = FALSE;
	doc->splitscreen = FALSE;
	
	gnome_mdi_child_set_menu_template (GNOME_MDI_CHILD (doc), doc_menu);
}

gE_document *gE_document_new ()
{
	gE_document *doc;
	
	int i;
	
	/* FIXME: Blarg!! */
	
	if ((doc = gtk_type_new (gE_document_get_type ())))
	  {
	    gnome_mdi_child_set_name(GNOME_MDI_CHILD(doc), _(UNTITLED));
	    
	    return doc;
	  }
	
	g_print ("Eeek.. bork!\n");
	gtk_object_destroy (GTK_OBJECT(doc));
	
	return NULL;
}

gE_document *gE_document_new_with_file (gchar *filename)
{
	gE_document *doc;
	char *nfile, *name;

	name = filename;
/*	if ((doc = gE_document_new()))*/
	if ((doc = gtk_type_new (gE_document_get_type ())))
	  {

   	    gnome_mdi_child_set_name(GNOME_MDI_CHILD(doc), _(filename));

/*	    nfile = g_malloc(strlen(name)+1);
	    strcpy(nfile, name);*/

	    gnome_mdi_add_child (mdi, GNOME_MDI_CHILD (doc));
	    gnome_mdi_add_view (mdi, GNOME_MDI_CHILD (doc));

    	    gE_file_open (GE_DOCUMENT(doc), filename);

    	    	    
	    return doc;
	  }
	g_print ("Eeek.. bork!\n");
	gtk_object_destroy (GTK_OBJECT(doc));
	
	return NULL;

} /* gE_document_new_with_file */

static void gE_document_real_changed(gE_document *doc, gpointer change_data)
{
	/* FIXME! */
}

gE_document *gE_document_current()
{
	gint cur;
	gE_document *current_document;
	current_document = NULL;

	/* FIXME: This is borked! I know it! */
	g_print ("gE_document_current()\n");
		
	if (mdi->active_child)
	  current_document = GE_DOCUMENT(mdi->active_child);
 

	return current_document;
}

static gchar *gE_document_get_config_string (GnomeMDIChild *child)
{
	/* FIXME: Is this correct? */
	return g_strdup (GE_DOCUMENT(child)->filename);
}

GnomeMDIChild *gE_document_new_from_config (const gchar *file)
{
	gE_document *doc;
	
	doc = gE_document_new_with_file (file);
	
	return GNOME_MDI_CHILD (doc);
}

void gE_add_view (GtkWidget *w, gpointer data)
{
	GnomeMDIChild *child = GNOME_MDI_CHILD (data);
	
	gnome_mdi_add_view (mdi, child);
}

void gE_remove_view (GtkWidget *w, gpointer data)
{
	if (mdi->active_view)
	  gnome_mdi_remove_view (mdi, mdi->active_view, FALSE);
}

/* Various MDI Callbacks */

gint remove_doc_cb (GnomeMDI *mdi, gE_document *doc)
{
	GnomeMessageBox *msgbox;
	int ret;
	char *fname, *msg;
	gE_data *data = g_malloc (sizeof(gE_data));


/*	fname = (doc->filename) ? g_basename(doc->filename) : _(UNTITLED);*/
	fname = GNOME_MDI_CHILD (doc)->name;
	msg =   (char *)g_malloc(strlen(fname) + 52);
	sprintf(msg, _(" '%s' has been modified. Do you wish to save it?"), fname);
	
	g_print ("remove_doc_cb\n");
	g_print ("has doc changed? %d\n",doc->changed);

	if (doc->changed)
	  {
	    msgbox = GNOME_MESSAGE_BOX (gnome_message_box_new (
	    							  msg,
	    							  GNOME_MESSAGE_BOX_QUESTION,
	    							  GNOME_STOCK_BUTTON_YES,
	    							  GNOME_STOCK_BUTTON_NO,
	    							  GNOME_STOCK_BUTTON_CANCEL,
	    							  NULL));
	    gnome_dialog_set_default (GNOME_DIALOG (msgbox), 2);
	    ret = gnome_dialog_run_and_close (GNOME_DIALOG (msgbox));
	    	    
/*	    switch (ret)
	      {
	        case 0:
	                 file_save_cb (NULL, data);
	                 g_print("blargh\n");
	        case 1:
	                 return TRUE;
	        default:
	                 return FALSE;
	      }*/
	      if (ret == 0)
	        {
	          /*file_save_cb (NULL, data);*/
	          if (gE_document_current()->filename)
	            file_save_cb (NULL, data);
	          
	           

	        }  
	      else if (ret == 2)
	       return FALSE;
	  }
	  
	return TRUE;
}

void view_changed_cb (GnomeMDI *mdi, GtkWidget *old_view)
{
	GnomeApp *app;
	GnomeUIInfo *uiinfo;
	gE_document *doc;
	GtkWidget *shell, *item;
	gint group_item, pos;
	gchar *p, *label;
	
	if (mdi->active_view == NULL)
	  return;
	  
	app = gnome_mdi_get_app_from_view (mdi->active_view);
	
	p = g_strconcat (GNOME_MENU_VIEW_PATH, label, NULL);
	shell = gnome_app_find_menu_pos (app->menubar, p, &pos);
	if (shell)
	  {
	    item = g_list_nth_data (GTK_MENU_SHELL (shell)->children, pos -1);
	    
	    if (item)
	      gtk_menu_shell_activate_item (GTK_MENU_SHELL (shell), item, TRUE);
   
	  }
	g_free (p);
}