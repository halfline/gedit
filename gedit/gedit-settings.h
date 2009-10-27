/*
 * gedit-settings.h
 * This file is part of gedit
 *
 * Copyright (C) 2009 - Ignacio Casal Quinteiro
 *               2002 - Paolo Maggi
 *
 * gedit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * gedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gedit; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */


#ifndef __GEDIT_SETTINGS_H__
#define __GEDIT_SETTINGS_H__

#include <glib-object.h>
#include <glib.h>
#include "gedit-app.h"

G_BEGIN_DECLS

#define GEDIT_TYPE_SETTINGS		(gedit_settings_get_type ())
#define GEDIT_SETTINGS(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_SETTINGS, GeditSettings))
#define GEDIT_SETTINGS_CONST(obj)	(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_SETTINGS, GeditSettings const))
#define GEDIT_SETTINGS_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GEDIT_TYPE_SETTINGS, GeditSettingsClass))
#define GEDIT_IS_SETTINGS(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEDIT_TYPE_SETTINGS))
#define GEDIT_IS_SETTINGS_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_SETTINGS))
#define GEDIT_SETTINGS_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GEDIT_TYPE_SETTINGS, GeditSettingsClass))

typedef struct _GeditSettings		GeditSettings;
typedef struct _GeditSettingsClass	GeditSettingsClass;
typedef struct _GeditSettingsPrivate	GeditSettingsPrivate;

struct _GeditSettings
{
	GSettings parent;
	
	GeditSettingsPrivate *priv;
};

struct _GeditSettingsClass
{
	GSettingsClass parent_class;
};

typedef enum {
	GEDIT_TOOLBAR_SYSTEM = 0,
	GEDIT_TOOLBAR_ICONS,
	GEDIT_TOOLBAR_ICONS_AND_TEXT,
	GEDIT_TOOLBAR_ICONS_BOTH_HORIZ
} GeditToolbarSetting;

GType			 gedit_settings_get_type			(void) G_GNUC_CONST;

GSettings		*gedit_settings_new				(void);

GeditLockdownMask	 gedit_settings_get_lockdown			(GeditSettings *gs);

gchar			*gedit_settings_get_system_font			(GeditSettings *gs);

/* Window state */
gint			 gedit_settings_get_window_state		(void);
void			 gedit_settings_set_window_state		(gint ws);
gboolean		 gedit_settings_window_state_can_set		(void);

/* Window size */
void			 gedit_settings_get_window_size			(gint *width,
									 gint *height);
void			 gedit_settings_get_default_window_size		(gint *width,
									 gint *height);
void 			 gedit_settings_set_window_size			(gint width,
									 gint height);
gboolean		 gedit_settings_window_size_can_set		(void);

/* Side panel */
gint			 gedit_settings_get_side_panel_size		(void);
gint			 gedit_settings_get_default_side_panel_size	(void);
void 			 gedit_settings_set_side_panel_size		(gint ps);
gboolean		 gedit_settings_side_panel_size_can_set		(void);
gint			 gedit_settings_get_side_panel_active_page 	(void);
void 			 gedit_settings_set_side_panel_active_page 	(gint id);
gboolean		 gedit_settings_side_panel_active_page_can_set	(void);

/* Bottom panel */
gint			 gedit_settings_get_bottom_panel_size		(void);
gint			 gedit_settings_get_default_bottom_panel_size	(void);
void 			 gedit_settings_set_bottom_panel_size		(gint ps);
gboolean		 gedit_settings_bottom_panel_size_can_set	(void);
gint			 gedit_settings_get_bottom_panel_active_page	(void);
void 			 gedit_settings_set_bottom_panel_active_page	(gint id);
gboolean		 gedit_settings_bottom_panel_active_page_can_set (void);

/* File filter */
gint			 gedit_settings_get_active_file_filter	(void);
void			 gedit_settings_set_active_file_filter	(gint id);
gboolean		 gedit_settings_active_file_filter_can_set	(void);

G_END_DECLS

/* key constants */
#define GS_USE_DEFAULT_FONT		"use_default_font"
#define GS_EDITOR_FONT			"editor_font"
#define GS_SCHEME			"scheme"
#define GS_CREATE_BACKUP_COPY		"create_backup_copy"
#define GS_AUTO_SAVE			"auto_save"
#define GS_AUTO_SAVE_INTERVAL		"auto_save_interval"
#define GS_UNDO_ACTIONS_LIMIT		"undo_actions_limit"
#define GS_MAX_UNDO_ACTIONS		"max_undo_actions"
#define GS_WRAP_MODE			"wrap_mode"
#define GS_TABS_SIZE			"tabs_size"
#define GS_INSERT_SPACES		"insert_spaces"
#define GS_AUTO_INDENT			"auto_indent"
#define GS_DISPLAY_LINE_NUMBERS		"display_line_numbers"
#define GS_HIGHLIGHT_CURRENT_LINE	"highlight_current_line"
#define GS_BRACKET_MATCHING		"bracket_matching"
#define GS_DISPLAY_RIGHT_MARGIN		"display_right_margin"
#define GS_RIGHT_MARGIN_POSITION	"right_margin_position"
#define GS_SMART_HOME_END		"smart_home_end"
#define GS_WRITABLE_VFS_SCHEMES		"writable_vfs_schemes"
#define GS_RESTORE_CURSOR_POSITION	"restore_cursor_position"
#define GS_SYNTAX_HIGHLIGHTING		"syntax_highlighting"
#define GS_SEARCH_HIGHLIGHTING		"search_highlighting"
#define GS_TOOLBAR_VISIBLE		"toolbar_visible"
#define GS_TOOLBAR_BUTTONS_STYLE	"toolbar_buttons_style"
#define GS_STATUSBAR_VISIBLE		"statusbar_visible"
#define GS_SIDE_PANE_VISIBLE		"side_pane_visible"
#define GS_BOTTOM_PANE_VISIBLE		"bottom_pane_visible"
#define GS_MAX_RECENTS			"max_recents"
#define GS_PRINT_SYNTAX_HIGHLIGHTING	"print_syntax_highlighting"
#define GS_PRINT_HEADER			"print_header"
#define GS_PRINT_WRAP_MODE		"print_wrap_mode"
#define GS_PRINT_LINE_NUMBERS		"print_line_numbers"
#define GS_PRINT_FONT_BODY_PANGO	"print_font_body_pango"
#define GS_PRINT_FONT_HEADER_PANGO	"print_font_header_pango"
#define GS_PRINT_FONT_NUMBERS_PANGO	"print_font_numbers_pango"
#define GS_ENCONDING_AUTO_DETECTED	"auto_detected"
#define GS_ENCONDING_SHOW_IN_MENU	"show_in_menu"
#define GS_ACTIVE_PLUGINS		"active_plugins"

#endif /* __GEDIT_SETTINGS_H__ */
