/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Paolo Borelli
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
 * Modified by the gedit Team, 2004. See the AUTHORS file for a 
 * list of people on the gedit Team.  
 * See the ChangeLog files for a list of changes. 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gedit-search-bar.h"
#include "gedit2.h"
#include "gedit-mdi.h"
#include "gedit-document.h"
#include "gedit-mdi-child.h"
#include "gedit-view.h"
#include "gedit-debug.h"
#include "gedit-utils.h"
#include "gedit-menus.h"

static void gedit_search_bar_class_init (GeditSearchBarClass *klass);
static void gedit_search_bar_init 	(GeditSearchBar	*bar);

struct _GeditSearchBarPrivate
{
	int a;
};

static GQuark was_entire_word_id = 0;
static GQuark was_case_sensitive_id = 0;

static GeditSearchBarBaseClass *parent_class = NULL;

GType
gedit_search_bar_get_type (void)
{
	static GType type = 0;

  	if (type == 0)
    	{
      		static const GTypeInfo info =
      		{
        		sizeof (GeditSearchBarClass),
        		NULL,		/* base_init */
        		NULL,		/* base_finalize */
        		(GClassInitFunc) gedit_search_bar_class_init,
        		NULL,           /* class_finalize */
        		NULL,           /* class_data */
        		sizeof (GeditSearchBar),
        		0,              /* n_preallocs */
        		(GInstanceInitFunc) gedit_search_bar_init
      		};

      		type = g_type_register_static (GEDIT_TYPE_SEARCH_BAR_BASE,
                			       "GeditSearchBar",
                                       	       &info,
                                       	       0);
    	}

	return type;
}

static void 
gedit_search_bar_finalize (GObject *object)
{
	GeditSearchBar *bar;

	bar = GEDIT_SEARCH_BAR (object);

	if (bar->priv != NULL)
	{
		g_free (bar->priv);
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void 
gedit_search_bar_destroy (GtkObject *object)
{
	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
update_menu_items_sensitivity (void)
{
	BonoboWindow *active_window = NULL;
	GeditDocument *doc = NULL;
	BonoboUIComponent *ui_component;
	gchar *lst;

	gedit_debug (DEBUG_SEARCH, "");

	active_window = gedit_get_active_window ();
	g_return_if_fail (active_window != NULL);

	ui_component = bonobo_mdi_get_ui_component_from_window (active_window);
	g_return_if_fail (ui_component != NULL);

	doc = gedit_get_active_document ();
	g_return_if_fail (doc != NULL);

	lst = gedit_document_get_last_searched_text (doc);

	gedit_menus_set_verb_sensitive (ui_component, "/commands/SearchFindAgain", lst != NULL);	
	g_free (lst);
}

static void
do_find (GeditSearchBarBase *bar, const gchar *string, GeditSearchBarFlags flags)
{
	GeditMDIChild *active_child;
	GeditView* active_view;
	GeditDocument *doc;
	gboolean found;
	gboolean case_sensitive;
	gboolean entire_word;
	gboolean search_backward;
	gint search_flags = 0;

	gedit_debug (DEBUG_SEARCH, "");

	if (bonobo_mdi_get_active_child (BONOBO_MDI (gedit_mdi)) == NULL)
		return;

	active_child = GEDIT_MDI_CHILD (bonobo_mdi_get_active_child (BONOBO_MDI (gedit_mdi)));
	g_return_if_fail (active_child != NULL);

	active_view = GEDIT_VIEW (bonobo_mdi_get_active_view (BONOBO_MDI (gedit_mdi)));
	g_return_if_fail (active_view != NULL);

	doc = active_child->document;
	g_return_if_fail (doc != NULL);

	if (strlen (string) <= 0)
		return;

	/* retrieve search settings from the dialog */
	case_sensitive = (flags & GEDIT_SEARCH_BAR_CASE_SENSITIVE) ? TRUE : FALSE;
	entire_word = (flags & GEDIT_SEARCH_BAR_ENTIRE_WORD) ? TRUE : FALSE;
	search_backward = (flags & GEDIT_SEARCH_BAR_FORWARD) ? FALSE : TRUE;

	/* setup quarks for next invocation */
	g_object_set_qdata (G_OBJECT (doc), was_entire_word_id, GBOOLEAN_TO_POINTER (entire_word));
	g_object_set_qdata (G_OBJECT (doc), was_case_sensitive_id, GBOOLEAN_TO_POINTER (case_sensitive));

	/* setup search parameter bitfield */
	GEDIT_SEARCH_SET_FROM_CURSOR (search_flags, TRUE);
	GEDIT_SEARCH_SET_CASE_SENSITIVE (search_flags, case_sensitive);
	GEDIT_SEARCH_SET_BACKWARDS (search_flags, search_backward);
	GEDIT_SEARCH_SET_ENTIRE_WORD (search_flags, entire_word);

	/* run search */
	found = gedit_document_find (doc, string, search_flags);

	/* wrap around */
	if (!found)
	{
		GEDIT_SEARCH_SET_FROM_CURSOR (search_flags, FALSE);
		found = gedit_document_find (doc, string, search_flags);

		// FIXME statusbar
	}

	if (found)
		gedit_view_scroll_to_cursor (active_view);
	else
		; // statusbar

	update_menu_items_sensitivity ();
}

static void
gedit_search_bar_class_init (GeditSearchBarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

  	parent_class = g_type_class_peek_parent (klass);

  	object_class->finalize = gedit_search_bar_finalize;

	GTK_OBJECT_CLASS (klass)->destroy = gedit_search_bar_destroy;

	GEDIT_SEARCH_BAR_BASE_CLASS (klass)->find = do_find;
}

static void
gedit_search_bar_init (GeditSearchBar *bar)
{
	bar->priv = g_new0 (GeditSearchBarPrivate, 1);
}

GtkWidget *
gedit_search_bar_new (void)
{
	GtkWidget *w;

	w = gtk_widget_new (GEDIT_TYPE_SEARCH_BAR, NULL);

	return w;
}


void
gedit_search_bar_find (void)
{
	GtkWidget *bar;
	BonoboWindow *active_window;
	GeditDocument *doc;
	gchar *last_searched_text;
	gint selection_start, selection_end;
	gboolean selection_exists;
	gboolean was_entire_word;
	gboolean was_case_sensitive;
	gpointer data;
	GeditSearchBarFlags flags = 0;

	active_window = gedit_get_active_window ();
	g_return_if_fail (active_window != NULL);

	bar = gedit_mdi_get_search_bar_from_window (active_window);

	doc = gedit_get_active_document ();
	g_return_if_fail (doc != NULL);

	selection_exists = gedit_document_get_selection (doc, &selection_start, &selection_end);
	if (selection_exists && (selection_end - selection_start < 80)) 
	{
		gchar *selection_text;

		selection_text = gedit_document_get_chars (doc, selection_start, selection_end);
		gedit_search_bar_base_set_search_string (GEDIT_SEARCH_BAR_BASE (bar),
							 selection_text);

		g_free (selection_text);
	} 
	else 
	{
		last_searched_text = gedit_document_get_last_searched_text (doc);
		if (last_searched_text != NULL)
		{
			gedit_search_bar_base_set_search_string (GEDIT_SEARCH_BAR_BASE (bar),
								 last_searched_text);
			g_free (last_searched_text);	
		}
	}

	if (!was_entire_word_id)
		was_entire_word_id = g_quark_from_static_string ("GeditWasEntireWord");

	if (!was_case_sensitive_id)
		was_case_sensitive_id = g_quark_from_static_string ("GeditWasCaseSensitive");

	data = g_object_get_qdata (G_OBJECT (doc), was_entire_word_id);
	if (data == NULL)
		was_entire_word = FALSE;
	else
		was_entire_word = GPOINTER_TO_BOOLEAN (data);

	data = g_object_get_qdata (G_OBJECT (doc), was_case_sensitive_id);
	if (data == NULL)
		was_case_sensitive = FALSE;
	else
		was_case_sensitive = GPOINTER_TO_BOOLEAN (data);

	if (was_case_sensitive)
		flags |= GEDIT_SEARCH_BAR_CASE_SENSITIVE;
	if (was_entire_word)
		flags |= GEDIT_SEARCH_BAR_ENTIRE_WORD;

	gedit_search_bar_base_set_search_flags (GEDIT_SEARCH_BAR_BASE (bar), flags);

	gtk_widget_show (bar);
	gtk_widget_grab_focus (bar);
}

