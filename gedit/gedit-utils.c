/*
 * gedit-utils.c
 * This file is part of gedit
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2002 Chema Celorio, Paolo Maggi
 * Copyright (C) 2003-2005 Paolo Maggi
 * Copyright (C) 2015 Sébastien Wilmet <swilmet@gnome.org>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "gedit-utils.h"
#include <glib/gi18n.h>
#include "gcode/gcode-utils.h"

void
gedit_utils_menu_position_under_widget (GtkMenu  *menu,
					gint     *x,
					gint     *y,
					gboolean *push_in,
					gpointer  user_data)
{
	gcode_utils_menu_position_under_widget (menu, x, y, push_in, user_data);
}

void
gedit_utils_menu_position_under_tree_view (GtkMenu  *menu,
					   gint     *x,
					   gint     *y,
					   gboolean *push_in,
					   gpointer  user_data)
{
	gcode_utils_menu_position_under_tree_view (menu, x, y, push_in, user_data);
}

/**
 * gedit_utils_set_atk_name_description:
 * @widget: The Gtk widget for which name/description to be set
 * @name: Atk name string
 * @description: Atk description string
 *
 * This function sets up name and description
 * for a specified gtk widget.
 */
void
gedit_utils_set_atk_name_description (GtkWidget   *widget,
				      const gchar *name,
				      const gchar *description)
{
	gcode_utils_set_atk_name_description (widget, name, description);
}

/**
 * gedit_set_atk_relation:
 * @obj1: specified widget.
 * @obj2: specified widget.
 * @rel_type: the type of relation to set up.
 *
 * This function establishes atk relation
 * between 2 specified widgets.
 */
void
gedit_utils_set_atk_relation (GtkWidget       *obj1,
			      GtkWidget       *obj2,
			      AtkRelationType  rel_type)
{
	gcode_utils_set_atk_relation (obj1, obj2, rel_type);
}

void
gedit_warning (GtkWindow *parent, const gchar *format, ...)
{
	va_list         args;
	gchar          *str;
	GtkWidget      *dialog;
	GtkWindowGroup *wg = NULL;

	g_return_if_fail (format != NULL);

	if (parent != NULL)
		wg = gtk_window_get_group (parent);

	va_start (args, format);
	str = g_strdup_vprintf (format, args);
	va_end (args);

	dialog = gtk_message_dialog_new_with_markup (
			parent,
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		   	GTK_MESSAGE_ERROR,
		   	GTK_BUTTONS_OK,
			"%s", str);

	g_free (str);

	if (wg != NULL)
		gtk_window_group_add_window (wg, GTK_WINDOW (dialog));

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

	g_signal_connect (G_OBJECT (dialog),
			  "response",
			  G_CALLBACK (gtk_widget_destroy),
			  NULL);

	gtk_widget_show (dialog);
}

/*
 * Doubles underscore to avoid spurious menu accels.
 */
gchar *
gedit_utils_escape_underscores (const gchar *text,
				gssize       length)
{
	return gcode_utils_escape_underscores (text, length);
}

gchar *
gedit_utils_str_middle_truncate (const gchar *string,
				 guint        truncate_length)
{
	return gcode_utils_str_middle_truncate (string, truncate_length);
}

gchar *
gedit_utils_str_end_truncate (const gchar *string,
			      guint        truncate_length)
{
	return gcode_utils_str_end_truncate (string, truncate_length);
}

gchar *
gedit_utils_make_valid_utf8 (const char *name)
{
	return gcode_utils_make_valid_utf8 (name);
}

/**
 * gedit_utils_uri_get_dirname:
 * @uri: the URI.
 *
 * Note: this function replace home dir with ~
 *
 * Returns: the directory name.
 */
gchar *
gedit_utils_uri_get_dirname (const gchar *uri)
{
	return gcode_utils_uri_get_dirname (uri);
}

/**
 * gedit_utils_location_get_dirname_for_display:
 * @location: the location
 *
 * Returns a string suitable to be displayed in the UI indicating
 * the name of the directory where the file is located.
 * For remote files it may also contain the hostname etc.
 * For local files it tries to replace the home dir with ~.
 *
 * Returns: (transfer full): a string to display the dirname
 */
gchar *
gedit_utils_location_get_dirname_for_display (GFile *location)
{
	return gcode_utils_location_get_dirname_for_display (location);
}

gchar *
gedit_utils_replace_home_dir_with_tilde (const gchar *uri)
{
	return gcode_utils_replace_home_dir_with_tilde (uri);
}

/**
 * gedit_utils_get_current_workspace:
 * @screen: a #GdkScreen
 *
 * Get the currently visible workspace for the #GdkScreen.
 *
 * If the X11 window property isn't found, 0 (the first workspace)
 * is returned.
 */
guint
gedit_utils_get_current_workspace (GdkScreen *screen)
{
	return gcode_utils_get_current_workspace (screen);
}

/**
 * gedit_utils_get_window_workspace:
 * @gtkwindow: a #GtkWindow.
 *
 * Get the workspace the window is on.
 *
 * This function gets the workspace that the #GtkWindow is visible on,
 * it returns GEDIT_ALL_WORKSPACES if the window is sticky, or if
 * the window manager doesn't support this function.
 *
 * Returns: the workspace the window is on.
 */
guint
gedit_utils_get_window_workspace (GtkWindow *gtkwindow)
{
	return gcode_utils_get_window_workspace (gtkwindow);
}

/**
 * gedit_utils_get_current_viewport:
 * @screen: a #GdkScreen
 * @x: (out): x-axis point.
 * @y: (out): y-axis point.
 *
 * Get the currently visible viewport origin for the #GdkScreen.
 *
 * If the X11 window property isn't found, (0, 0) is returned.
 */
void
gedit_utils_get_current_viewport (GdkScreen    *screen,
				  gint         *x,
				  gint         *y)
{
	return gcode_utils_get_current_viewport (screen, x, y);
}

gboolean
gedit_utils_is_valid_location (GFile *location)
{
	return gcode_utils_is_valid_location (location);
}

static GtkWidget *handle_builder_error (const gchar *message, ...) G_GNUC_PRINTF (1, 2);

static GtkWidget *
handle_builder_error (const gchar *message, ...)
{
	GtkWidget *label;
	gchar *msg;
	gchar *msg_plain;
	va_list args;

	va_start (args, message);
	msg_plain = g_strdup_vprintf (message, args);
	va_end (args);

	label = gtk_label_new (NULL);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

	msg = g_strconcat ("<span size=\"large\" weight=\"bold\">",
			   msg_plain, "</span>\n\n",
			   _("Please check your installation."),
			   NULL);

	gtk_label_set_markup (GTK_LABEL (label), msg);

	g_free (msg_plain);
	g_free (msg);

	gtk_widget_set_margin_start (label, 6);
	gtk_widget_set_margin_end (label, 6);
	gtk_widget_set_margin_top (label, 6);
	gtk_widget_set_margin_bottom (label, 6);

	return label;
}

/* TODO: just add a translation_doamin arg to get_ui_objects method */
static gboolean
get_ui_objects_with_translation_domain (const gchar  *filename,
                                        const gchar  *translation_domain,
                                        gchar       **root_objects,
                                        GtkWidget   **error_widget,
                                        const gchar  *object_name,
                                        va_list       args)
{
	GtkBuilder *builder;
	const gchar *name;
	GError *error = NULL;
	gchar *filename_markup;
	gboolean ret = TRUE;

	g_return_val_if_fail (filename != NULL, FALSE);
	g_return_val_if_fail (error_widget != NULL, FALSE);
	g_return_val_if_fail (object_name != NULL, FALSE);

	filename_markup = g_markup_printf_escaped ("<i>%s</i>", filename);
	*error_widget = NULL;

	builder = gtk_builder_new ();

	if (translation_domain != NULL)
	{
		gtk_builder_set_translation_domain (builder, translation_domain);
	}

	if (root_objects != NULL)
	{
		gtk_builder_add_objects_from_file (builder,
						   filename,
						   root_objects,
						   &error);
	}
	else
	{
		gtk_builder_add_from_file (builder,
					   filename,
					   &error);
	}

	if (error != NULL)
	{
		*error_widget = handle_builder_error (_("Unable to open UI file %s. Error: %s"),
						      filename_markup,
						      error->message);
		g_error_free (error);
		g_free (filename_markup);
		g_object_unref (builder);

		return FALSE;
	}

	for (name = object_name; name; name = va_arg (args, const gchar *))
	{
		GObject **gobj;

		gobj = va_arg (args, GObject **);
		*gobj = gtk_builder_get_object (builder, name);

		if (!*gobj)
		{
			*error_widget = handle_builder_error (_("Unable to find the object '%s' inside file %s."),
							      name,
							      filename_markup),
			ret = FALSE;
			break;
		}

		/* we return a new ref for the root objects,
		 * the others are already reffed by their parent root object */
		if (root_objects != NULL)
		{
			gint i;

			for (i = 0; root_objects[i] != NULL; ++i)
			{
				if ((strcmp (name, root_objects[i]) == 0))
				{
					g_object_ref (*gobj);
				}
			}
		}
	}

	g_free (filename_markup);
	g_object_unref (builder);

	return ret;
}

/**
 * gedit_utils_get_ui_objects:
 * @filename: the path to the gtk builder file
 * @root_objects: a %NULL terminated list of root objects to load or NULL to
 *                load all objects
 * @error_widget: a pointer were a #GtkLabel
 * @object_name: the name of the first object
 * @...: a pointer were the first object is returned, followed by more
 *       name / object pairs and terminated by %NULL.
 *
 * This function gets the requested objects from a GtkBuilder ui file. In case
 * of error it returns %FALSE and sets error_widget to a GtkLabel containing
 * the error message to display.
 *
 * Returns: %FALSE if an error occurs, %TRUE on success.
 */
gboolean
gedit_utils_get_ui_objects (const gchar  *filename,
			    gchar       **root_objects,
			    GtkWidget   **error_widget,
			    const gchar  *object_name,
			    ...)
{
	gboolean ret;
	va_list args;

	va_start (args, object_name);
	ret = get_ui_objects_with_translation_domain (filename,
	                                              NULL,
	                                              root_objects,
	                                              error_widget,
	                                              object_name,
	                                              args);
	va_end (args);

	return ret;
}

/**
 * gedit_utils_get_ui_objects_with_translation_domain:
 * @filename: the path to the gtk builder file
 * @translation_domain: the specific translation domain
 * @root_objects: a %NULL terminated list of root objects to load or NULL to
 *                load all objects
 * @error_widget: a pointer were a #GtkLabel
 * @object_name: the name of the first object
 * @...: a pointer were the first object is returned, followed by more
 *       name / object pairs and terminated by %NULL.
 *
 * This function gets the requested objects from a GtkBuilder ui file. In case
 * of error it returns %FALSE and sets error_widget to a GtkLabel containing
 * the error message to display.
 *
 * Returns: %FALSE if an error occurs, %TRUE on success.
 */
gboolean
gedit_utils_get_ui_objects_with_translation_domain (const gchar  *filename,
                                                    const gchar  *translation_domain,
                                                    gchar       **root_objects,
                                                    GtkWidget   **error_widget,
                                                    const gchar  *object_name,
                                                    ...)
{
	gboolean ret;
	va_list args;

	va_start (args, object_name);
	ret = get_ui_objects_with_translation_domain (filename,
	                                              translation_domain,
	                                              root_objects,
	                                              error_widget,
	                                              object_name,
	                                              args);
	va_end (args);

	return ret;
}

gchar *
gedit_utils_make_canonical_uri_from_shell_arg (const gchar *str)
{
	return gcode_utils_make_canonical_uri_from_shell_arg (str);
}

/**
 * gedit_utils_basename_for_display:
 * @location: location for which the basename should be displayed
 *
 * Returns: (transfer full): the basename of a file suitable for display to users.
 */
gchar *
gedit_utils_basename_for_display (GFile *location)
{
	return gcode_utils_basename_for_display (location);
}

/**
 * gedit_utils_drop_get_uris:
 * @selection_data: the #GtkSelectionData from drag_data_received
 *
 * Create a list of valid uri's from a uri-list drop.
 *
 * Returns: (transfer full): a string array which will hold the uris or
 *           %NULL if there were no valid uris. g_strfreev should be used when
 *           the string array is no longer used
 */
gchar **
gedit_utils_drop_get_uris (GtkSelectionData *selection_data)
{
	return gcode_utils_drop_get_uris (selection_data);
}

/**
 * gedit_utils_decode_uri:
 * @uri: the uri to decode
 * @scheme: (out) (allow-none): return value pointer for the uri's
 * scheme (e.g. http, sftp, ...), or %NULL
 * @user: (out) (allow-none): return value pointer for the uri user info, or %NULL
 * @port: (out) (allow-none): return value pointer for the uri port, or %NULL
 * @host: (out) (allow-none): return value pointer for the uri host, or %NULL
 * @path: (out) (allow-none): return value pointer for the uri path, or %NULL
 *
 * Parse and break an uri apart in its individual components like the uri
 * scheme, user info, port, host and path. The return value pointer can be
 * %NULL to ignore certain parts of the uri. If the function returns %TRUE, then
 * all return value pointers should be freed using g_free
 *
 * Return value: %TRUE if the uri could be properly decoded, %FALSE otherwise.
 */
gboolean
gedit_utils_decode_uri (const gchar  *uri,
			gchar       **scheme,
			gchar       **user,
			gchar       **host,
			gchar       **port,
			gchar       **path)
{
	return gcode_utils_decode_uri (uri, scheme, user, host, port, path);
}

GtkSourceCompressionType
gedit_utils_get_compression_type_from_content_type (const gchar *content_type)
{
	return gcode_utils_get_compression_type_from_content_type (content_type);
}

gchar *
gedit_utils_set_direct_save_filename (GdkDragContext *context)
{
	return gcode_utils_set_direct_save_filename (context);
}

const gchar *
gedit_utils_newline_type_to_string (GtkSourceNewlineType newline_type)
{
	return gcode_utils_newline_type_to_string (newline_type);
}

/* ex:set ts=8 noet: */
