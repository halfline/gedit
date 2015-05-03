/*
 * This file is part of gcode.
 *
 * Copyright 1998, 1999 - Alex Roberts, Evan Lawrence
 * Copyright 2000, 2002 - Chema Celorio
 * Copyright 2000-2005 - Paolo Maggi
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

#include "gcode-utils.h"

#include <string.h>
#include <glib/gi18n.h>

/* For the workspace/viewport stuff */
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#endif

#include "gcode-debug.h"

static void
widget_get_origin (GtkWidget *widget,
		   gint      *x,
		   gint      *y)

{
	GdkWindow *window;

	window = gtk_widget_get_window (widget);
	gdk_window_get_origin (window, x, y);
}

void
gcode_utils_menu_position_under_widget (GtkMenu  *menu,
					gint     *x,
					gint     *y,
					gboolean *push_in,
					gpointer  user_data)
{
	GtkWidget *widget;
	GtkRequisition requisition;
	GtkAllocation allocation;

	widget = GTK_WIDGET (user_data);
	widget_get_origin (widget, x, y);

	gtk_widget_get_preferred_size (GTK_WIDGET (menu), &requisition,
	                               NULL);

	gtk_widget_get_allocation (widget, &allocation);

	if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
	{
		*x += allocation.x + allocation.width - requisition.width;
	}
	else
	{
		*x += allocation.x;
	}

	*y += allocation.y + allocation.height;

	*push_in = TRUE;
}

void
gcode_utils_menu_position_under_tree_view (GtkMenu  *menu,
					   gint     *x,
					   gint     *y,
					   gboolean *push_in,
					   gpointer  user_data)
{
	GtkTreeView *tree = GTK_TREE_VIEW (user_data);
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model (tree);
	g_return_if_fail (model != NULL);

	selection = gtk_tree_view_get_selection (tree);
	g_return_if_fail (selection != NULL);

	if (gtk_tree_selection_get_selected (selection, NULL, &iter))
	{
		GtkTreePath *path;
		GdkRectangle rect;

		widget_get_origin (GTK_WIDGET (tree), x, y);

		path = gtk_tree_model_get_path (model, &iter);
		gtk_tree_view_get_cell_area (tree, path,
					     gtk_tree_view_get_column (tree, 0), /* FIXME 0 for RTL ? */
					     &rect);
		gtk_tree_path_free (path);

		*x += rect.x;
		*y += rect.y + rect.height;

		if (gtk_widget_get_direction (GTK_WIDGET (tree)) == GTK_TEXT_DIR_RTL)
		{
			GtkRequisition requisition;
			gtk_widget_get_preferred_size (GTK_WIDGET (menu),
			                               &requisition, NULL);
			*x += rect.width - requisition.width;
		}
	}
	else
	{
		/* no selection -> regular "under widget" positioning */
		gcode_utils_menu_position_under_widget (menu,
							x, y, push_in,
							tree);
	}
}

/* See gedit_utils_set_atk_name_description(). */
void
gcode_utils_set_atk_name_description (GtkWidget   *widget,
				      const gchar *name,
				      const gchar *description)
{
	AtkObject *aobj;

	aobj = gtk_widget_get_accessible (widget);

	if (!(GTK_IS_ACCESSIBLE (aobj)))
		return;

	if (name)
		atk_object_set_name (aobj, name);

	if (description)
		atk_object_set_description (aobj, description);
}

/* See gedit_utils_set_atk_relation(). */
void
gcode_utils_set_atk_relation (GtkWidget       *obj1,
			      GtkWidget       *obj2,
			      AtkRelationType  rel_type)
{
	AtkObject *atk_obj1, *atk_obj2;
	AtkRelationSet *relation_set;
	AtkObject *targets[1];
	AtkRelation *relation;

	atk_obj1 = gtk_widget_get_accessible (obj1);
	atk_obj2 = gtk_widget_get_accessible (obj2);

	if (!(GTK_IS_ACCESSIBLE (atk_obj1)) || !(GTK_IS_ACCESSIBLE (atk_obj2)))
		return;

	relation_set = atk_object_ref_relation_set (atk_obj1);
	targets[0] = atk_obj2;

	relation = atk_relation_new (targets, 1, rel_type);
	atk_relation_set_add (relation_set, relation);

	g_object_unref (G_OBJECT (relation));
}

/* See gedit_utils_escape_underscores(). */
gchar *
gcode_utils_escape_underscores (const gchar *text,
				gssize       length)
{
	GString *str;
	const gchar *p;
	const gchar *end;

	g_return_val_if_fail (text != NULL, NULL);

	if (length < 0)
		length = strlen (text);

	str = g_string_sized_new (length);

	p = text;
	end = text + length;

	while (p != end)
	{
		const gchar *next;
		next = g_utf8_next_char (p);

		switch (*p)
		{
			case '_':
				g_string_append (str, "__");
				break;
			default:
				g_string_append_len (str, p, next - p);
				break;
		}

		p = next;
	}

	return g_string_free (str, FALSE);
}

/* the following functions are taken from eel */

static gchar *
gcode_utils_str_truncate (const gchar *string,
			  guint        truncate_length,
			  gboolean     middle)
{
	GString     *truncated;
	guint        length;
	guint        n_chars;
	guint        num_left_chars;
	guint        right_offset;
	guint        delimiter_length;
	const gchar *delimiter = "\342\200\246";

	g_return_val_if_fail (string != NULL, NULL);

	length = strlen (string);

	g_return_val_if_fail (g_utf8_validate (string, length, NULL), NULL);

	/* It doesnt make sense to truncate strings to less than
	 * the size of the delimiter plus 2 characters (one on each
	 * side)
	 */
	delimiter_length = g_utf8_strlen (delimiter, -1);
	if (truncate_length < (delimiter_length + 2))
	{
		return g_strdup (string);
	}

	n_chars = g_utf8_strlen (string, length);

	/* Make sure the string is not already small enough. */
	if (n_chars <= truncate_length)
	{
		return g_strdup (string);
	}

	/* Find the 'middle' where the truncation will occur. */
	if (middle)
	{
		num_left_chars = (truncate_length - delimiter_length) / 2;
		right_offset = n_chars - truncate_length + num_left_chars + delimiter_length;

		truncated = g_string_new_len (string,
					      g_utf8_offset_to_pointer (string, num_left_chars) - string);
		g_string_append (truncated, delimiter);
		g_string_append (truncated, g_utf8_offset_to_pointer (string, right_offset));
	}
	else
	{
		num_left_chars = truncate_length - delimiter_length;
		truncated = g_string_new_len (string,
					      g_utf8_offset_to_pointer (string, num_left_chars) - string);
		g_string_append (truncated, delimiter);
	}

	return g_string_free (truncated, FALSE);
}

gchar *
gcode_utils_str_middle_truncate (const gchar *string,
				 guint        truncate_length)
{
	return gcode_utils_str_truncate (string, truncate_length, TRUE);
}

gchar *
gcode_utils_str_end_truncate (const gchar *string,
			      guint        truncate_length)
{
	return gcode_utils_str_truncate (string, truncate_length, FALSE);
}

gchar *
gcode_utils_make_valid_utf8 (const char *name)
{
	GString *string;
	const char *remainder, *invalid;
	int remaining_bytes, valid_bytes;

	g_return_val_if_fail (name != NULL, NULL);

	string = NULL;
	remainder = name;
	remaining_bytes = strlen (name);

	while (remaining_bytes != 0) {
		if (g_utf8_validate (remainder, remaining_bytes, &invalid))
		{
			break;
		}
		valid_bytes = invalid - remainder;

		if (string == NULL)
		{
			string = g_string_sized_new (remaining_bytes);
		}
		g_string_append_len (string, remainder, valid_bytes);
		/* append U+FFFD REPLACEMENT CHARACTER */
		g_string_append (string, "\357\277\275");

		remaining_bytes -= valid_bytes + 1;
		remainder = invalid + 1;
	}

	if (string == NULL)
	{
		return g_strdup (name);
	}

	g_string_append (string, remainder);

	g_assert (g_utf8_validate (string->str, -1, NULL));

	return g_string_free (string, FALSE);
}

/* See gedit_utils_uri_get_dirname(). */
gchar *
gcode_utils_uri_get_dirname (const gchar *uri)
{
	gchar *res;
	gchar *str;

	g_return_val_if_fail (uri != NULL, NULL);

	/* CHECK: does it work with uri chaining? - Paolo */
	str = g_path_get_dirname (uri);
	g_return_val_if_fail (str != NULL, g_strdup ("."));

	if ((strlen (str) == 1) && (*str == '.'))
	{
		g_free (str);

		return NULL;
	}

	res = gcode_utils_replace_home_dir_with_tilde (str);

	g_free (str);

	return res;
}

/* See gedit_utils_location_get_dirname_for_display(). */
gchar *
gcode_utils_location_get_dirname_for_display (GFile *location)
{
	gchar *uri;
	gchar *res;
	GMount *mount;

	g_return_val_if_fail (location != NULL, NULL);

	/* we use the parse name, that is either the local path
	 * or an uri but which is utf8 safe */
	uri = g_file_get_parse_name (location);

	/* FIXME: this is sync... is it a problem? */
	mount = g_file_find_enclosing_mount (location, NULL, NULL);
	if (mount != NULL)
	{
		gchar *mount_name;
		gchar *path = NULL;
		gchar *dirname;

		mount_name = g_mount_get_name (mount);
		g_object_unref (mount);

		/* obtain the "path" part of the uri */
		gcode_utils_decode_uri (uri,
					NULL, NULL,
					NULL, NULL,
					&path);

		if (path == NULL)
		{
			dirname = gcode_utils_uri_get_dirname (uri);
		}
		else
		{
			dirname = gcode_utils_uri_get_dirname (path);
		}

		if (dirname == NULL || strcmp (dirname, ".") == 0)
		{
			res = mount_name;
		}
		else
		{
			res = g_strdup_printf ("%s %s", mount_name, dirname);
			g_free (mount_name);
		}

		g_free (path);
		g_free (dirname);
	}
	else
	{
		/* fallback for local files or uris without mounts */
		res = gcode_utils_uri_get_dirname (uri);
	}

	g_free (uri);

	return res;
}

gchar *
gcode_utils_replace_home_dir_with_tilde (const gchar *uri)
{
	gchar *tmp;
	gchar *home;

	g_return_val_if_fail (uri != NULL, NULL);

	/* Note that g_get_home_dir returns a const string */
	tmp = (gchar *)g_get_home_dir ();

	if (tmp == NULL)
		return g_strdup (uri);

	home = g_filename_to_utf8 (tmp, -1, NULL, NULL, NULL);
	if (home == NULL)
		return g_strdup (uri);

	if (strcmp (uri, home) == 0)
	{
		g_free (home);

		return g_strdup ("~/");
	}

	tmp = home;
	home = g_strdup_printf ("%s/", tmp);
	g_free (tmp);

	if (g_str_has_prefix (uri, home))
	{
		gchar *res;

		res = g_strdup_printf ("~/%s", uri + strlen (home));

		g_free (home);

		return res;
	}

	g_free (home);

	return g_strdup (uri);
}

/* the following two functions are courtesy of galeon */

/* See gedit_utils_get_current_workspace(). */
guint
gcode_utils_get_current_workspace (GdkScreen *screen)
{
#ifdef GDK_WINDOWING_X11
	GdkWindow *root_win;
	GdkDisplay *display;
	guint ret = 0;

	g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

	root_win = gdk_screen_get_root_window (screen);
	display = gdk_screen_get_display (screen);

	if (GDK_IS_X11_DISPLAY (display))
	{
		Atom type;
		gint format;
		gulong nitems;
		gulong bytes_after;
		guint *current_desktop;
		gint err, result;

		gdk_error_trap_push ();
		result = XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (root_win),
					     gdk_x11_get_xatom_by_name_for_display (display, "_NET_CURRENT_DESKTOP"),
					     0, G_MAXLONG, False, XA_CARDINAL, &type, &format, &nitems,
					     &bytes_after, (gpointer) &current_desktop);
		err = gdk_error_trap_pop ();

		if (err != Success || result != Success)
			return ret;

		if (type == XA_CARDINAL && format == 32 && nitems > 0)
			ret = current_desktop[0];

		XFree (current_desktop);
	}

	return ret;
#else
	/* FIXME: on mac etc proably there are native APIs
	 * to get the current workspace etc */
	return 0;
#endif
}

/* See gedit_utils_get_window_workspace(). */
guint
gcode_utils_get_window_workspace (GtkWindow *gtkwindow)
{
#ifdef GDK_WINDOWING_X11
	GdkWindow *window;
	GdkDisplay *display;
	Atom type;
	gint format;
	gulong nitems;
	gulong bytes_after;
	guint *workspace;
	gint err, result;
	guint ret = GCODE_ALL_WORKSPACES;

	g_return_val_if_fail (GTK_IS_WINDOW (gtkwindow), 0);
	g_return_val_if_fail (gtk_widget_get_realized (GTK_WIDGET (gtkwindow)), 0);

	window = gtk_widget_get_window (GTK_WIDGET (gtkwindow));
	display = gdk_window_get_display (window);

	if (GDK_IS_X11_DISPLAY (display))
	{
		gdk_error_trap_push ();
		result = XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (window),
					     gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_DESKTOP"),
					     0, G_MAXLONG, False, XA_CARDINAL, &type, &format, &nitems,
					     &bytes_after, (gpointer) &workspace);
		err = gdk_error_trap_pop ();

		if (err != Success || result != Success)
			return ret;

		if (type == XA_CARDINAL && format == 32 && nitems > 0)
			ret = workspace[0];

		XFree (workspace);
	}

	return ret;
#else
	/* FIXME: on mac etc proably there are native APIs
	 * to get the current workspace etc */
	return 0;
#endif
}

/* See gedit_utils_get_current_viewport(). */
void
gcode_utils_get_current_viewport (GdkScreen    *screen,
				  gint         *x,
				  gint         *y)
{
#ifdef GDK_WINDOWING_X11
	GdkWindow *root_win;
	GdkDisplay *display;
	Atom type;
	gint format;
	gulong nitems;
	gulong bytes_after;
	gulong *coordinates;
	gint err, result;

	g_return_if_fail (GDK_IS_SCREEN (screen));
	g_return_if_fail (x != NULL && y != NULL);

	/* Default values for the viewport origin */
	*x = 0;
	*y = 0;

	root_win = gdk_screen_get_root_window (screen);
	display = gdk_screen_get_display (screen);

	if (GDK_IS_X11_DISPLAY (display))
	{
		gdk_error_trap_push ();
		result = XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (root_win),
					     gdk_x11_get_xatom_by_name_for_display (display, "_NET_DESKTOP_VIEWPORT"),
					     0, G_MAXLONG, False, XA_CARDINAL, &type, &format, &nitems,
					     &bytes_after, (void*) &coordinates);
		err = gdk_error_trap_pop ();

		if (err != Success || result != Success)
			return;

		if (type != XA_CARDINAL || format != 32 || nitems < 2)
		{
			XFree (coordinates);
			return;
		}

		*x = coordinates[0];
		*y = coordinates[1];
		XFree (coordinates);
	}
#else
	/* FIXME: on mac etc proably there are native APIs
	 * to get the current workspace etc */
	*x = 0;
	*y = 0;
#endif
}

static gboolean
is_valid_scheme_character (gchar c)
{
	return g_ascii_isalnum (c) || c == '+' || c == '-' || c == '.';
}

static gboolean
has_valid_scheme (const gchar *uri)
{
	const gchar *p;

	p = uri;

	if (!is_valid_scheme_character (*p))
	{
		return FALSE;
	}

	do
	{
		p++;
	} while (is_valid_scheme_character (*p));

	return *p == ':';
}

gboolean
gcode_utils_is_valid_location (GFile *location)
{
	const guchar *p;
	gchar *uri;
	gboolean is_valid;

	if (location == NULL)
		return FALSE;

	uri = g_file_get_uri (location);

	if (!has_valid_scheme (uri))
	{
		g_free (uri);
		return FALSE;
	}

	is_valid = TRUE;

	/* We expect to have a fully valid set of characters */
	for (p = (const guchar *)uri; *p; p++) {
		if (*p == '%')
		{
			++p;
			if (!g_ascii_isxdigit (*p))
			{
				is_valid = FALSE;
				break;
			}

			++p;
			if (!g_ascii_isxdigit (*p))
			{
				is_valid = FALSE;
				break;
			}
		}
		else
		{
			if (*p <= 32 || *p >= 128)
			{
				is_valid = FALSE;
				break;
			}
		}
	}

	g_free (uri);

	return is_valid;
}

gchar *
gcode_utils_make_canonical_uri_from_shell_arg (const gchar *str)
{
	GFile *gfile;
	gchar *uri;

	g_return_val_if_fail (str != NULL, NULL);
	g_return_val_if_fail (*str != '\0', NULL);

	/* Note for the future:
	 * FIXME: is still still relevant?
	 *
	 * <federico> paolo: and flame whoever tells
	 * you that file:///gnome/test_files/hëllò
	 * doesn't work --- that's not a valid URI
	 *
	 * <paolo> federico: well, another solution that
	 * does not requires patch to _from_shell_args
	 * is to check that the string returned by it
	 * contains only ASCII chars
	 * <federico> paolo: hmmmm, isn't there
	 * gnome_vfs_is_uri_valid() or something?
	 * <paolo>: I will use gcode_utils_is_valid_uri ()
	 *
	 */

	gfile = g_file_new_for_commandline_arg (str);

	if (gcode_utils_is_valid_location (gfile))
	{
		uri = g_file_get_uri (gfile);
		g_object_unref (gfile);
		return uri;
	}

	g_object_unref (gfile);
	return NULL;
}

/* See gedit_utils_basename_for_display(). */
gchar *
gcode_utils_basename_for_display (GFile *location)
{
	gchar *name;
	gchar *hn;
	gchar *uri;

	g_return_val_if_fail (G_IS_FILE (location), NULL);

	uri = g_file_get_uri (location);

	/* First, try to query the display name, but only on local files */
	if (g_file_has_uri_scheme (location, "file"))
	{
		GFileInfo *info;

		info = g_file_query_info (location,
					  G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
					  G_FILE_QUERY_INFO_NONE,
					  NULL,
					  NULL);

		if (info)
		{
			/* Simply get the display name to use as the basename */
			name = g_strdup (g_file_info_get_display_name (info));
			g_object_unref (info);
		}
		else
		{
			/* This is a local file, and therefore we will use
			 * g_filename_display_basename on the local path */
			gchar *local_path;

			local_path = g_file_get_path (location);
			name = g_filename_display_basename (local_path);
			g_free (local_path);
		}
	}
	else if (g_file_has_parent (location, NULL) ||
	          !gcode_utils_decode_uri (uri, NULL, NULL, &hn, NULL, NULL))
	{
		/* For remote files with a parent (so not just http://foo.com)
		   or remote file for which the decoding of the host name fails,
		   use the _parse_name and take basename of that */
		gchar *parse_name;
		gchar *base;

		parse_name = g_file_get_parse_name (location);
		base = g_filename_display_basename (parse_name);
		name = g_uri_unescape_string (base, NULL);

		g_free (base);
		g_free (parse_name);
	}
	else
	{
		/* display '/ on <host>' using the decoded host */
		gchar *hn_utf8;

		if  (hn != NULL)
		{
			hn_utf8 = gcode_utils_make_valid_utf8 (hn);
		}
		else
		{
			/* we should never get here */
			hn_utf8 = g_strdup ("?");
		}

		/* Translators: '/ on <remote-share>' */
		name = g_strdup_printf (_("/ on %s"), hn_utf8);

		g_free (hn_utf8);
		g_free (hn);
	}

	g_free (uri);

	return name;
}

/* See gedit_utils_drop_get_uris(). */
gchar **
gcode_utils_drop_get_uris (GtkSelectionData *selection_data)
{
	gchar **uris;
	gint i;
	gint p = 0;
	gchar **uri_list;

	uris = g_uri_list_extract_uris ((gchar *) gtk_selection_data_get_data (selection_data));
	uri_list = g_new0(gchar *, g_strv_length (uris) + 1);

	for (i = 0; uris[i] != NULL; i++)
	{
		gchar *uri;

		uri = gcode_utils_make_canonical_uri_from_shell_arg (uris[i]);

		/* Silently ignore malformed URI/filename */
		if (uri != NULL)
			uri_list[p++] = uri;
	}

	if (*uri_list == NULL)
	{
		g_free(uri_list);
		g_strfreev (uris);
		return NULL;
	}

	g_strfreev (uris);
	return uri_list;
}

static void
null_ptr (gchar **ptr)
{
	if (ptr)
		*ptr = NULL;
}

/* See gedit_utils_decode_uri(). */
gboolean
gcode_utils_decode_uri (const gchar  *uri,
			gchar       **scheme,
			gchar       **user,
			gchar       **host,
			gchar       **port,
			gchar       **path)
{
	/* Largely copied from glib/gio/gdummyfile.c:_g_decode_uri. This
	 * functionality should be in glib/gio, but for now we implement it
	 * ourselves (see bug #546182) */

	const char *p, *in, *hier_part_start, *hier_part_end;
	char *out;
	char c;

	/* From RFC 3986 Decodes:
	 * URI = scheme ":" hier-part [ "?" query ] [ "#" fragment ]
	 */

	p = uri;

	null_ptr (scheme);
	null_ptr (user);
	null_ptr (port);
	null_ptr (host);
	null_ptr (path);

	/* Decode scheme:
	 * scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
	 */

	if (!g_ascii_isalpha (*p))
		return FALSE;

	while (1)
	{
		c = *p++;

		if (c == ':')
			break;

		if (!(g_ascii_isalnum(c) ||
		      c == '+' ||
		      c == '-' ||
		      c == '.'))
		{
			return FALSE;
		}
	}

	if (scheme)
	{
		*scheme = g_malloc (p - uri);
		out = *scheme;

		for (in = uri; in < p - 1; in++)
		{
			*out++ = g_ascii_tolower (*in);
		}

		*out = '\0';
	}

	hier_part_start = p;
	hier_part_end = p + strlen (p);

	if (hier_part_start[0] == '/' && hier_part_start[1] == '/')
	{
		const char *authority_start, *authority_end;
		const char *userinfo_start, *userinfo_end;
		const char *host_start, *host_end;
		const char *port_start;

		authority_start = hier_part_start + 2;
		/* authority is always followed by / or nothing */
		authority_end = memchr (authority_start, '/', hier_part_end - authority_start);

		if (authority_end == NULL)
			authority_end = hier_part_end;

		/* 3.2:
		 * authority = [ userinfo "@" ] host [ ":" port ]
		 */

		userinfo_end = memchr (authority_start, '@', authority_end - authority_start);

		if (userinfo_end)
		{
			userinfo_start = authority_start;

			if (user)
				*user = g_uri_unescape_segment (userinfo_start, userinfo_end, NULL);

			if (user && *user == NULL)
			{
				if (scheme)
					g_free (*scheme);

				return FALSE;
			}

			host_start = userinfo_end + 1;
		}
		else
		{
			host_start = authority_start;
		}

		port_start = memchr (host_start, ':', authority_end - host_start);

		if (port_start)
		{
			host_end = port_start++;

			if (port)
				*port = g_strndup (port_start, authority_end - port_start);
		}
		else
		{
			host_end = authority_end;
		}

		if (host)
			*host = g_strndup (host_start, host_end - host_start);

		hier_part_start = authority_end;
	}

	if (path)
		*path = g_uri_unescape_segment (hier_part_start, hier_part_end, "/");

	return TRUE;
}

GtkSourceCompressionType
gcode_utils_get_compression_type_from_content_type (const gchar *content_type)
{
	if (content_type == NULL)
	{
		return GTK_SOURCE_COMPRESSION_TYPE_NONE;
	}

	if (g_content_type_is_a (content_type, "application/x-gzip"))
	{
		return GTK_SOURCE_COMPRESSION_TYPE_GZIP;
	}

	return GTK_SOURCE_COMPRESSION_TYPE_NONE;
}

/* Copied from nautilus */
static gchar *
get_direct_save_filename (GdkDragContext *context)
{
	guchar *prop_text;
	gint prop_len;

	if (!gdk_property_get (gdk_drag_context_get_source_window  (context), gdk_atom_intern ("XdndDirectSave0", FALSE),
			       gdk_atom_intern ("text/plain", FALSE), 0, 1024, FALSE, NULL, NULL,
			       &prop_len, &prop_text) && prop_text != NULL) {
		return NULL;
	}

	/* Zero-terminate the string */
	prop_text = g_realloc (prop_text, prop_len + 1);
	prop_text[prop_len] = '\0';

	/* Verify that the file name provided by the source is valid */
	if (*prop_text == '\0' ||
	    strchr ((const gchar *) prop_text, G_DIR_SEPARATOR) != NULL) {
		gcode_debug_message (DEBUG_UTILS, "Invalid filename provided by XDS drag site");
		g_free (prop_text);
		return NULL;
	}

	return (gchar *)prop_text;
}

gchar *
gcode_utils_set_direct_save_filename (GdkDragContext *context)
{
	gchar *uri;
	gchar *filename;

	uri = NULL;
	filename = get_direct_save_filename (context);

	if (filename != NULL)
	{
		gchar *tempdir;
		gchar *path;

		tempdir = g_dir_make_tmp ("gcode-drop-XXXXXX", NULL);
		if (tempdir == NULL)
		{
			tempdir = g_strdup (g_get_tmp_dir ());
		}

		path = g_build_filename (tempdir,
					filename,
					NULL);

		uri = g_filename_to_uri (path, NULL, NULL);

		/* Change the property */
		gdk_property_change (gdk_drag_context_get_source_window (context),
				     gdk_atom_intern ("XdndDirectSave0", FALSE),
				     gdk_atom_intern ("text/plain", FALSE), 8,
				     GDK_PROP_MODE_REPLACE, (const guchar *) uri,
				     strlen (uri));

		g_free (tempdir);
		g_free (path);
		g_free (filename);
	}

	return uri;
}

const gchar *
gcode_utils_newline_type_to_string (GtkSourceNewlineType newline_type)
{
	switch (newline_type)
	{
	case GTK_SOURCE_NEWLINE_TYPE_LF:
		return _("Unix/Linux");
	case GTK_SOURCE_NEWLINE_TYPE_CR:
		return _("Mac OS Classic");
	case GTK_SOURCE_NEWLINE_TYPE_CR_LF:
		return _("Windows");
	}

	return NULL;
}

/* ex:set ts=8 noet: */
