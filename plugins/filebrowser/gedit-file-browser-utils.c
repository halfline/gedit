/*
 * gedit-file-bookmarks-utils.c - Gedit plugin providing easy file access
 * from the sidepanel
 *
 * Copyright (C) 2006 - Jesse van den Kieboom <jesse@icecrew.nl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include <gedit/gedit-utils.h>

#include "gedit-file-browser-utils.h"

static GdkPixbuf *
process_icon_pixbuf (GdkPixbuf   *pixbuf,
		     gchar const *name,
		     gint         size,
		     GError      *error)
{
	GdkPixbuf *scale;

	if (error != NULL)
	{
		g_warning ("Could not load theme icon %s: %s",
			   name,
			   error->message);
		g_error_free (error);
	}

	if (pixbuf && gdk_pixbuf_get_width (pixbuf) > size)
	{
		scale = gdk_pixbuf_scale_simple (pixbuf,
		                                 size,
		                                 size,
		                                 GDK_INTERP_BILINEAR);
		g_object_unref (pixbuf);
		pixbuf = scale;
	}

	return pixbuf;
}

GdkPixbuf *
gedit_file_browser_utils_pixbuf_from_theme (gchar const *name,
                                            GtkIconSize  size)
{
	gint width;
	GError *error = NULL;
	GdkPixbuf *pixbuf;

	gtk_icon_size_lookup (size, &width, NULL);

	pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
					   name,
					   width,
					   0,
					   &error);

	pixbuf = process_icon_pixbuf (pixbuf, name, width, error);

	return pixbuf;
}

GdkPixbuf *
gedit_file_browser_utils_pixbuf_from_icon (GIcon       *icon,
                                           GtkIconSize  size)
{
	GdkPixbuf *ret = NULL;
	GtkIconTheme *theme;
	GtkIconInfo *info;
	gint width;

	if (!icon)
		return NULL;

	theme = gtk_icon_theme_get_default ();
	gtk_icon_size_lookup (size, &width, NULL);

	info = gtk_icon_theme_lookup_by_gicon (theme,
					       icon,
					       width,
					       GTK_ICON_LOOKUP_USE_BUILTIN);

	if (!info)
		return NULL;

	ret = gtk_icon_info_load_icon (info, NULL);
	g_object_unref (info);

	return ret;
}

GdkPixbuf *
gedit_file_browser_utils_pixbuf_from_file (GFile       *file,
                                           GtkIconSize  size,
                                           gboolean     use_symbolic)
{
	GIcon *icon;
	GFileInfo *info;
	GdkPixbuf *ret = NULL;
	const char *attribute;

	attribute = use_symbolic ? G_FILE_ATTRIBUTE_STANDARD_SYMBOLIC_ICON :
	                           G_FILE_ATTRIBUTE_STANDARD_ICON;

	info = g_file_query_info (file,
				  attribute,
				  G_FILE_QUERY_INFO_NONE,
				  NULL,
				  NULL);

	if (!info)
		return NULL;

	icon = use_symbolic ? g_file_info_get_symbolic_icon (info) :
	                      g_file_info_get_icon (info);
	if (icon != NULL)
		ret = gedit_file_browser_utils_pixbuf_from_icon (icon, size);

	g_object_unref (info);

	return ret;
}

gchar *
gedit_file_browser_utils_file_basename (GFile *file)
{
	return gedit_utils_basename_for_display (file);
}

gboolean
gedit_file_browser_utils_confirmation_dialog (GeditWindow    *window,
                                              GtkMessageType  type,
                                              gchar const    *message,
		                              gchar const    *secondary,
		                              gchar const    *button_stock,
		                              gchar const    *button_label)
{
	GtkWidget *dlg;
	gint ret;
	GtkWidget *button;

	dlg = gtk_message_dialog_new (GTK_WINDOW (window),
				      GTK_DIALOG_MODAL |
				      GTK_DIALOG_DESTROY_WITH_PARENT,
				      type,
				      GTK_BUTTONS_NONE, "%s", message);

	if (secondary)
	{
		gtk_message_dialog_format_secondary_text
		    (GTK_MESSAGE_DIALOG (dlg), "%s", secondary);
	}

	/* Add a cancel button */
	button = gtk_button_new_with_mnemonic (_("_Cancel"));
	gtk_widget_show (button);

	gtk_widget_set_can_default (button, TRUE);
	gtk_dialog_add_action_widget (GTK_DIALOG (dlg),
	                              button,
	                              GTK_RESPONSE_CANCEL);

	/* Add custom button */
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
	button = gtk_button_new_from_stock (button_stock);
	G_GNUC_END_IGNORE_DEPRECATIONS;

	if (button_label)
	{
		G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
		gtk_button_set_use_stock (GTK_BUTTON (button), FALSE);
		G_GNUC_END_IGNORE_DEPRECATIONS;
		gtk_button_set_label (GTK_BUTTON (button), button_label);
	}

	gtk_widget_show (button);
	gtk_widget_set_can_default (button, TRUE);
	gtk_dialog_add_action_widget (GTK_DIALOG (dlg),
                                      button,
                                      GTK_RESPONSE_OK);

	ret = gtk_dialog_run (GTK_DIALOG (dlg));
	gtk_widget_destroy (dlg);

	return (ret == GTK_RESPONSE_OK);
}
/* ex:ts=8:noet: */
