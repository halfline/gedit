/*
 * This file is part of gcode.
 *
 * Copyright 1998, 1999 - Alex Roberts, Evan Lawrence
 * Copyright 2000, 2001 - Chema Celorio
 * Copyright 2000-2005 - Paolo Maggi
 * Copyright 2015 - Sébastien Wilmet <swilmet@gnome.org>
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

#ifndef __GCODE_UTILS_H__
#define __GCODE_UTILS_H__

#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

/* useful macro */
#define GBOOLEAN_TO_POINTER(i) (GINT_TO_POINTER ((i) ? 2 : 1))
#define GPOINTER_TO_BOOLEAN(i) ((gboolean) ((GPOINTER_TO_INT(i) == 2) ? TRUE : FALSE))

#define IS_VALID_BOOLEAN(v) (((v == TRUE) || (v == FALSE)) ? TRUE : FALSE)

enum { GCODE_ALL_WORKSPACES = 0xffffffff };

void		 gcode_utils_menu_position_under_widget (GtkMenu          *menu,
							 gint             *x,
							 gint             *y,
							 gboolean         *push_in,
							 gpointer          user_data);

void		 gcode_utils_menu_position_under_tree_view
							(GtkMenu          *menu,
							 gint             *x,
							 gint             *y,
							 gboolean         *push_in,
							 gpointer          user_data);

gchar		*gcode_utils_escape_underscores		(const gchar      *text,
							 gssize            length);

gchar		*gcode_utils_str_middle_truncate	(const gchar      *string,
							 guint             truncate_length);

gchar		*gcode_utils_str_end_truncate		(const gchar      *string,
							 guint             truncate_length);

void		 gcode_utils_set_atk_name_description	(GtkWidget        *widget,
							 const gchar      *name,
							 const gchar      *description);

void		 gcode_utils_set_atk_relation		(GtkWidget        *obj1,
							 GtkWidget        *obj2,
							 AtkRelationType   rel_type);

gchar		*gcode_utils_make_valid_utf8		(const char       *name);

gchar		*gcode_utils_uri_get_dirname		(const char       *uri);

gchar		*gcode_utils_location_get_dirname_for_display
							(GFile            *location);

gchar		*gcode_utils_replace_home_dir_with_tilde(const gchar      *uri);

guint		 gcode_utils_get_current_workspace	(GdkScreen        *screen);

guint		 gcode_utils_get_window_workspace	(GtkWindow        *gtkwindow);

void		 gcode_utils_get_current_viewport	(GdkScreen        *screen,
							 gint             *x,
							 gint             *y);

gboolean	 gcode_utils_is_valid_location		(GFile            *location);

gchar		*gcode_utils_make_canonical_uri_from_shell_arg
							(const gchar      *str);

gchar		*gcode_utils_basename_for_display	(GFile            *location);
gboolean	 gcode_utils_decode_uri 		(const gchar      *uri,
							 gchar           **scheme,
							 gchar           **user,
							 gchar           **port,
							 gchar           **host,
							 gchar           **path);

gchar		**gcode_utils_drop_get_uris		(GtkSelectionData *selection_data);

GtkSourceCompressionType
		 gcode_utils_get_compression_type_from_content_type
		 					(const gchar      *content_type);

gchar           *gcode_utils_set_direct_save_filename	(GdkDragContext *context);

const gchar     *gcode_utils_newline_type_to_string	(GtkSourceNewlineType newline_type);

G_END_DECLS

#endif /* __GCODE_UTILS_H__ */

/* ex:set ts=8 noet: */
