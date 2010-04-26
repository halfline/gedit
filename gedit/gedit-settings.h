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

void			 gedit_settings_get_default_window_size		(gint *width, gint *height);

/* key constants */
#define GS_USE_DEFAULT_FONT		"use-default-font"
#define GS_EDITOR_FONT			"editor-font"
#define GS_SCHEME			"scheme"
#define GS_CREATE_BACKUP_COPY		"create-backup-copy"
#define GS_AUTO_SAVE			"auto-save"
#define GS_AUTO_SAVE_INTERVAL		"auto-save-interval"
#define GS_UNDO_ACTIONS_LIMIT		"undo-actions-limit"
#define GS_MAX_UNDO_ACTIONS		"max-undo-actions"
#define GS_WRAP_MODE			"wrap-mode"
#define GS_TABS_SIZE			"tabs-size"
#define GS_INSERT_SPACES		"insert-spaces"
#define GS_AUTO_INDENT			"auto-indent"
#define GS_DISPLAY_LINE_NUMBERS		"display-line-numbers"
#define GS_HIGHLIGHT_CURRENT_LINE	"highlight-current-line"
#define GS_BRACKET_MATCHING		"bracket-matching"
#define GS_DISPLAY_RIGHT_MARGIN		"display-right-margin"
#define GS_RIGHT_MARGIN_POSITION	"right-margin-position"
#define GS_SMART_HOME_END		"smart-home-end"
#define GS_WRITABLE_VFS_SCHEMES		"writable-vfs-schemes"
#define GS_RESTORE_CURSOR_POSITION	"restore-cursor-position"
#define GS_SYNTAX_HIGHLIGHTING		"syntax-highlighting"
#define GS_SEARCH_HIGHLIGHTING		"search-highlighting"
#define GS_TOOLBAR_VISIBLE		"toolbar-visible"
#define GS_TOOLBAR_BUTTONS_STYLE	"toolbar-buttons-style"
#define GS_STATUSBAR_VISIBLE		"statusbar-visible"
#define GS_SIDE_PANE_VISIBLE		"side-pane-visible"
#define GS_BOTTOM_PANE_VISIBLE		"bottom-pane-visible"
#define GS_MAX_RECENTS			"max-recents"
#define GS_PRINT_SYNTAX_HIGHLIGHTING	"print-syntax-highlighting"
#define GS_PRINT_HEADER			"print-header"
#define GS_PRINT_WRAP_MODE		"print-wrap-mode"
#define GS_PRINT_LINE_NUMBERS		"print-line-numbers"
#define GS_PRINT_FONT_BODY_PANGO	"print-font-body-pango"
#define GS_PRINT_FONT_HEADER_PANGO	"print-font-header-pango"
#define GS_PRINT_FONT_NUMBERS_PANGO	"print-font-numbers-pango"
#define GS_ENCODING_AUTO_DETECTED	"auto-detected"
#define GS_ENCODING_SHOW_IN_MENU	"show-in-menu"
#define GS_ACTIVE_PLUGINS		"active-plugins"

/* window state keys */
#define GS_WINDOW_STATE			"state"
#define GS_WINDOW_SIZE			"size"
#define GS_SIDE_PANEL_SIZE		"size"
#define GS_SIDE_PANEL_ACTIVE_PAGE	"active-page"
#define GS_BOTTOM_PANEL_SIZE		"size"
#define GS_BOTTOM_PANEL_ACTIVE_PAGE	"active-page"
#define GS_ACTIVE_FILE_FILTER		"filter-id"

G_END_DECLS

#endif /* __GEDIT_SETTINGS_H__ */
