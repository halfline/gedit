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

#include "gedit-search-bar-base.h"
#include "gedit-marshal.h"

enum {
	FIND,
	FIND_ALL,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void gedit_search_bar_base_class_init (GeditSearchBarBaseClass *klass);
static void gedit_search_bar_base_init       (GeditSearchBarBase      *bar);

struct _GeditSearchBarBasePrivate
{
	GtkWidget *entry;
	GtkWidget *match_case;
	GtkWidget *entire_word;
	GtkWidget *next;
	GtkWidget *prev;
	GtkWidget *all;
	GtkWidget *close;
};

static GtkHBoxClass *parent_class = NULL;

GType
gedit_search_bar_base_get_type (void)
{
	static GType type = 0;

  	if (type == 0)
    	{
      		static const GTypeInfo info =
      		{
        		sizeof (GeditSearchBarBaseClass),
        		NULL,		/* base_init */
        		NULL,		/* base_finalize */
        		(GClassInitFunc) gedit_search_bar_base_class_init,
        		NULL,           /* class_finalize */
        		NULL,           /* class_data */
        		sizeof (GeditSearchBarBase),
        		0,              /* n_preallocs */
        		(GInstanceInitFunc) gedit_search_bar_base_init
      		};

      		type = g_type_register_static (GTK_TYPE_HBOX,
                			       "GeditSearchBarBase",
                                       	       &info,
                                       	       0);
    	}

	return type;
}

static void 
gedit_search_bar_base_finalize (GObject *object)
{
	GeditSearchBarBase *bar;

	bar = GEDIT_SEARCH_BAR_BASE (object);

	if (bar->priv != NULL)
	{
		g_free (bar->priv);
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void 
gedit_search_bar_base_destroy (GtkObject *object)
{
	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
find_cb (GeditSearchBarBase *bar, const gchar *string, GeditSearchBarFlags flags)
{
	g_print ("find: %s (flags: %d)\n", string, flags);
}

static void
grab_focus (GtkWidget *widget)
{
	GeditSearchBarBasePrivate *priv;

	priv = GEDIT_SEARCH_BAR_BASE (widget)->priv;

	gtk_widget_grab_focus (priv->entry);
}

static void
gedit_search_bar_base_class_init (GeditSearchBarBaseClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

  	parent_class = g_type_class_peek_parent (klass);

  	object_class->finalize = gedit_search_bar_base_finalize;

	GTK_OBJECT_CLASS (klass)->destroy = gedit_search_bar_base_destroy;

	GTK_WIDGET_CLASS (klass)->grab_focus = grab_focus;

	signals[FIND] = 
		g_signal_new ("find",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GeditSearchBarBaseClass, find),
			      NULL,
			      NULL,
			      gedit_marshal_VOID__STRING_INT,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_STRING,
			      G_TYPE_INT);

	signals[FIND_ALL] = 
		g_signal_new ("find-all",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GeditSearchBarBaseClass, find_all),
			      NULL,
			      NULL,
			      gedit_marshal_VOID__STRING_INT,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_STRING,
			      G_TYPE_INT);

	klass->find = find_cb;
}

static void 
search_entry_changed_cb (GtkEditable *editable, GeditSearchBarBase *bar)
{
	GeditSearchBarBasePrivate *priv = bar->priv;
	const gchar *search_string;

	search_string = gtk_entry_get_text (GTK_ENTRY (priv->entry));		
	g_return_if_fail (search_string != NULL);

	if (strlen (search_string) <= 0)
	{
		gtk_widget_set_sensitive (priv->next, FALSE);
		gtk_widget_set_sensitive (priv->prev, FALSE);
		gtk_widget_set_sensitive (priv->all, FALSE);
	}
	else
	{
		gtk_widget_set_sensitive (priv->next, TRUE);
		gtk_widget_set_sensitive (priv->prev, TRUE);
		gtk_widget_set_sensitive (priv->all, TRUE);
	}
}

static void 
search_entry_activate_cb (GtkEntry *entry, GeditSearchBarBase *bar)
{
	GeditSearchBarBasePrivate *priv = bar->priv;
	const gchar *search_string;
	gboolean case_sensitive;
	gboolean entire_word;
	GeditSearchBarFlags flags = 0;

	search_string = gtk_entry_get_text (GTK_ENTRY (priv->entry));		

	/* always go forward */
	flags |= GEDIT_SEARCH_BAR_FORWARD;

	case_sensitive = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->match_case));
	entire_word = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->entire_word));

	if (case_sensitive)
		flags |= GEDIT_SEARCH_BAR_CASE_SENSITIVE;
	if (entire_word)
		flags |= GEDIT_SEARCH_BAR_ENTIRE_WORD;

	g_signal_emit (bar, signals[FIND], 0, search_string, flags);
}

static gchar * 
escape_search_text (const gchar* text)
{
	GString *str;
	gint length;
	const gchar *p;
 	const gchar *end;

  	g_return_val_if_fail (text != NULL, NULL);

    	length = strlen (text);

	str = g_string_new ("");

  	p = text;
  	end = text + length;

  	while (p != end)
    	{
      		const gchar *next;
      		next = g_utf8_next_char (p);

		switch (*p)
        	{
       			case '\n':
          			g_string_append (str, "\\n");
          			break;
			case '\r':
          			g_string_append (str, "\\r");
          			break;
			case '\t':
          			g_string_append (str, "\\t");
          			break;
        		default:
          			g_string_append_len (str, p, next - p);
          			break;
        	}

      		p = next;
    	}

	return g_string_free (str, FALSE);
}

static void
insert_text_cb (GtkEditable *editable,
		const gchar *text,
		gint length,
		gint *position,
		gpointer data)
{
	static gboolean insert_text = FALSE;
	gchar *escaped_text;
	gint new_len;

	/* To avoid recursive behavior */
	if (insert_text)
		return;

	escaped_text = escape_search_text (text);

	new_len = strlen (escaped_text);

	if (new_len == length)
	{
		g_free (escaped_text);
		return;
	}

	insert_text = TRUE;

	g_signal_stop_emission_by_name (editable, "insert_text");

	gtk_editable_insert_text (editable, escaped_text, new_len, position);

	insert_text = FALSE;

	g_free (escaped_text);
}

static void
find_clicked_cb (GtkButton *button, GeditSearchBarBase *bar)
{
	GeditSearchBarBasePrivate *priv = bar->priv;
	const gchar *search_string;
	gboolean case_sensitive;
	gboolean entire_word;
	GeditSearchBarFlags flags = 0;

	search_string = gtk_entry_get_text (GTK_ENTRY (priv->entry));		

	if (button == GTK_BUTTON (priv->next))
		flags |= GEDIT_SEARCH_BAR_FORWARD;

	case_sensitive = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->match_case));
	entire_word = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->entire_word));

	if (case_sensitive)
		flags |= GEDIT_SEARCH_BAR_CASE_SENSITIVE;
	if (entire_word)
		flags |= GEDIT_SEARCH_BAR_ENTIRE_WORD;

	g_signal_emit (bar, signals[FIND], 0, search_string, flags);
}

static void
find_all_clicked_cb (GtkButton *button, GeditSearchBarBase *bar)
{
	GeditSearchBarBasePrivate *priv = bar->priv;
	const gchar *search_string;
	gboolean case_sensitive;
	gboolean entire_word;
	GeditSearchBarFlags flags = 0;

	search_string = gtk_entry_get_text (GTK_ENTRY (priv->entry));		

	case_sensitive = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->match_case));
	entire_word = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->entire_word));

	if (case_sensitive)
		flags |= GEDIT_SEARCH_BAR_CASE_SENSITIVE;
	if (entire_word)
		flags |= GEDIT_SEARCH_BAR_ENTIRE_WORD;

	g_signal_emit (bar, signals[FIND_ALL], 0, search_string, flags);
}

static void
gedit_search_bar_base_init (GeditSearchBarBase *bar)
{
	GeditSearchBarBasePrivate *priv;
	GtkWidget *label;
	GtkWidget *image;

	bar->priv = g_new0 (GeditSearchBarBasePrivate, 1);
	priv = bar->priv;

	label = gtk_label_new_with_mnemonic ("S_earch");
	gtk_box_pack_start (GTK_BOX (bar), label, FALSE, FALSE, 6);

	priv->entry = gtk_entry_new ();
	g_signal_connect (priv->entry, "changed",
			  G_CALLBACK (search_entry_changed_cb), bar);
	g_signal_connect (priv->entry, "activate",
			  G_CALLBACK (search_entry_activate_cb), bar);
	g_signal_connect (priv->entry, "insert-text",
			  G_CALLBACK (insert_text_cb), NULL);
	gtk_box_pack_start (GTK_BOX (bar), priv->entry, FALSE, FALSE, 6);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), priv->entry);

	priv->prev = gtk_button_new ();
	image = gtk_image_new_from_stock (GTK_STOCK_GO_BACK, GTK_ICON_SIZE_BUTTON);
	gtk_container_add (GTK_CONTAINER (priv->prev), image);
	g_signal_connect (priv->prev, "clicked",
			  G_CALLBACK (find_clicked_cb), bar);
	gtk_box_pack_start (GTK_BOX (bar), priv->prev, FALSE, FALSE, 6);

	priv->next = gtk_button_new ();
	image = gtk_image_new_from_stock (GTK_STOCK_GO_FORWARD, GTK_ICON_SIZE_BUTTON);
	gtk_container_add (GTK_CONTAINER (priv->next), image);
	g_signal_connect (priv->next, "clicked",
			  G_CALLBACK (find_clicked_cb), bar);
	gtk_box_pack_start (GTK_BOX (bar), priv->next, FALSE, FALSE, 6);

	priv->match_case = gtk_check_button_new_with_mnemonic (_("_Match Case"));
	gtk_box_pack_start (GTK_BOX (bar), priv->match_case, FALSE, FALSE, 6);

	priv->entire_word = gtk_check_button_new_with_mnemonic (_("_Entire Word"));
	gtk_box_pack_start (GTK_BOX (bar), priv->entire_word, FALSE, FALSE, 6);

	priv->all = gtk_button_new_with_mnemonic (_("All Matches"));
	g_signal_connect_swapped (priv->all, "clicked",
				  G_CALLBACK (find_all_clicked_cb), bar);
	gtk_box_pack_start (GTK_BOX (bar), priv->all, FALSE, FALSE, 6);

	priv->close = gtk_button_new ();
	image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_BUTTON);
	gtk_container_add (GTK_CONTAINER (priv->close), image);
	gtk_button_set_relief (GTK_BUTTON (priv->close), GTK_RELIEF_NONE);
	g_signal_connect_swapped (priv->close, "clicked",
				  G_CALLBACK (gtk_widget_hide), bar);
	gtk_box_pack_end (GTK_BOX (bar), priv->close, FALSE, FALSE, 6);
}

GtkWidget *
gedit_search_bar_base_new (void)
{
	return gtk_widget_new (GEDIT_TYPE_SEARCH_BAR_BASE, NULL);
}

void
gedit_search_bar_base_set_search_string (GeditSearchBarBase *bar, const gchar* str)
{
	GeditSearchBarBasePrivate *priv;

  	g_return_if_fail (GEDIT_IS_SEARCH_BAR_BASE (bar));
  	g_return_if_fail (str != NULL);

	priv = bar->priv;

	gtk_entry_set_text (GTK_ENTRY (priv->entry), str);
}

void
gedit_search_bar_base_set_search_flags (GeditSearchBarBase *bar, GeditSearchBarFlags flags)
{
	GeditSearchBarBasePrivate *priv;

  	g_return_if_fail (GEDIT_IS_SEARCH_BAR_BASE (bar));

	priv = bar->priv;

	if (flags & GEDIT_SEARCH_BAR_CASE_SENSITIVE)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->match_case), TRUE);
	if (flags & GEDIT_SEARCH_BAR_ENTIRE_WORD)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->entire_word), TRUE);
}

