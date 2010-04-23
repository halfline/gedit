/*
 * gedit-settings.c
 * This file is part of gedit
 *
 * Copyright (C) 2002-2005 - Paolo Maggi
 *               2009 - Ignacio Casal Quinteiro
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

#include <string.h>

#include "gedit-settings.h"
#include "gedit-app.h"
#include "gedit-debug.h"
#include "gedit-view.h"
#include "gedit-window.h"
#include "gedit-plugins-engine.h"
#include "gedit-style-scheme-manager.h"
#include "gedit-dirs.h"
#include "gedit-utils.h"
#include "gedit-window-private.h"

#define GS_LOCKDOWN_COMMAND_LINE "disable-command-line"
#define GS_LOCKDOWN_PRINTING "disable-printing"
#define GS_LOCKDOWN_PRINT_SETUP "disable-print-setup"
#define GS_LOCKDOWN_SAVE_TO_DISK "disable-save-to-disk"

#define GS_SYSTEM_FONT "monospace-font-name"

#define GEDIT_STATE_DEFAULT_WINDOW_STATE	0
#define GEDIT_STATE_DEFAULT_WINDOW_WIDTH	650
#define GEDIT_STATE_DEFAULT_WINDOW_HEIGHT	500
#define GEDIT_STATE_DEFAULT_SIDE_PANEL_SIZE	200
#define GEDIT_STATE_DEFAULT_BOTTOM_PANEL_SIZE	140

#define GEDIT_STATE_FILE_LOCATION "gedit-2"

#define GEDIT_STATE_WINDOW_GROUP "window"
#define GEDIT_STATE_WINDOW_STATE "state"
#define GEDIT_STATE_WINDOW_HEIGHT "height"
#define GEDIT_STATE_WINDOW_WIDTH "width"
#define GEDIT_STATE_SIDE_PANEL_SIZE "side_panel_size"
#define GEDIT_STATE_BOTTOM_PANEL_SIZE "bottom_panel_size"
#define GEDIT_STATE_SIDE_PANEL_ACTIVE_PAGE "side_panel_active_page"
#define GEDIT_STATE_BOTTOM_PANEL_ACTIVE_PAGE "bottom_panel_active_page"

#define GEDIT_STATE_FILEFILTER_GROUP "filefilter"
#define GEDIT_STATE_FILEFILTER_ID "id"

#define GEDIT_SETTINGS_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GEDIT_TYPE_SETTINGS, GeditSettingsPrivate))

struct _GeditSettingsPrivate
{
	GSettings *lockdown;
	GSettings *interface;
	GSettings *editor;
	GSettings *ui;
	GSettings *plugins;
	
	gchar *old_scheme;
};

G_DEFINE_TYPE (GeditSettings, gedit_settings, G_TYPE_SETTINGS)

/* GUI state is serialized to a .desktop file, not in gconf */

/* FIXME: use the class to manage this */
static gint window_state = -1;
static gint window_height = -1;
static gint window_width = -1;
static gint side_panel_size = -1;
static gint bottom_panel_size = -1;
static gint side_panel_active_page = -1;
static gint bottom_panel_active_page = -1;
static gint active_file_filter = -1;


static gchar *
get_state_filename (void)
{
	gchar *config_dir;
	gchar *filename = NULL;

	config_dir = gedit_dirs_get_user_config_dir ();

	if (config_dir != NULL)
	{
		filename = g_build_filename (config_dir,
					     GEDIT_STATE_FILE_LOCATION,
					     NULL);
		g_free (config_dir);
	}

	return filename;
}

static GKeyFile *
get_gedit_state_file (void)
{
	static GKeyFile *state_file = NULL;

	if (state_file == NULL)
	{
		gchar *filename;
		GError *err = NULL;

		state_file = g_key_file_new ();

		filename = get_state_filename ();

		if (!g_key_file_load_from_file (state_file,
						filename,
						G_KEY_FILE_NONE,
						&err))
		{
			if (err->domain != G_FILE_ERROR ||
			    err->code != G_FILE_ERROR_NOENT)
			{
				g_warning ("Could not load gedit state file: %s\n",
					   err->message);
			}

			g_error_free (err);
		}

		g_free (filename);
	}

	return state_file;
}

static void
gedit_state_get_int (const gchar *group,
		     const gchar *key,
		     gint         defval,
		     gint        *result)
{
	GKeyFile *state_file;
	gint res;
	GError *err = NULL;

	state_file = get_gedit_state_file ();
	res = g_key_file_get_integer (state_file,
				      group,
				      key,
				      &err);

	if (err != NULL)
	{
		if ((err->domain != G_KEY_FILE_ERROR) ||
		    ((err->code != G_KEY_FILE_ERROR_GROUP_NOT_FOUND &&
		      err->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND)))
		{
			g_warning ("Could not get state value %s::%s : %s\n",
				   group,
				   key,
				   err->message);
		}

		*result = defval;
		g_error_free (err);
	}
	else
	{
		*result = res;
	}
}

static void
gedit_state_set_int (const gchar *group,
		     const gchar *key,
		     gint         value)
{
	GKeyFile *state_file;

	state_file = get_gedit_state_file ();
	g_key_file_set_integer (state_file,
				group,
				key,
				value);
}

static gboolean
gedit_state_file_sync (void)
{
	GKeyFile *state_file;
	gchar *config_dir;
	gchar *filename = NULL;
	gchar *content = NULL;
	gsize length;
	gint res;
	GError *err = NULL;
	gboolean ret = FALSE;

	state_file = get_gedit_state_file ();
	g_return_val_if_fail (state_file != NULL, FALSE);

	config_dir = gedit_dirs_get_user_config_dir ();
	if (config_dir == NULL)
	{
		g_warning ("Could not get config directory\n");
		return ret;
	}

	res = g_mkdir_with_parents (config_dir, 0755);
	if (res < 0)
	{
		g_warning ("Could not create config directory\n");
		goto out;
	}

	content = g_key_file_to_data (state_file,
				      &length,
				      &err);

	if (err != NULL)
	{
		g_warning ("Could not get data from state file: %s\n",
			   err->message);
		goto out;
	}

	if (content != NULL)
	{
		filename = get_state_filename ();
		if (!g_file_set_contents (filename,
					  content,
					  length,
					  &err))
		{
			g_warning ("Could not write gedit state file: %s\n",
				   err->message);
			goto out;
		}
	}

	ret = TRUE;

 out:
	if (err != NULL)
		g_error_free (err);

	g_free (config_dir);
	g_free (filename);
	g_free (content);

	return ret;
}

/* Window state */
gint
gedit_settings_get_window_state (void)
{
	if (window_state == -1)
	{
		gedit_state_get_int (GEDIT_STATE_WINDOW_GROUP,
				     GEDIT_STATE_WINDOW_STATE,
				     GEDIT_STATE_DEFAULT_WINDOW_STATE,
				     &window_state);
	}

	return window_state;
}
			
void
gedit_settings_set_window_state (gint ws)
{
	g_return_if_fail (ws > -1);
	
	window_state = ws;

	gedit_state_set_int (GEDIT_STATE_WINDOW_GROUP,
			     GEDIT_STATE_WINDOW_STATE,
			     ws);
}

gboolean
gedit_settings_window_state_can_set (void)
{
	return TRUE;
}

/* Window size */
void
gedit_settings_get_window_size (gint *width, gint *height)
{
	g_return_if_fail (width != NULL && height != NULL);

	if (window_width == -1)
	{
		gedit_state_get_int (GEDIT_STATE_WINDOW_GROUP,
				     GEDIT_STATE_WINDOW_WIDTH,
				     GEDIT_STATE_DEFAULT_WINDOW_WIDTH,
				     &window_width);
	}

	if (window_height == -1)
	{
		gedit_state_get_int (GEDIT_STATE_WINDOW_GROUP,
				     GEDIT_STATE_WINDOW_HEIGHT,
				     GEDIT_STATE_DEFAULT_WINDOW_HEIGHT,
				     &window_height);
	}

	*width = window_width;
	*height = window_height;
}

void
gedit_settings_get_default_window_size (gint *width, gint *height)
{
	g_return_if_fail (width != NULL && height != NULL);

	*width = GEDIT_STATE_DEFAULT_WINDOW_WIDTH;
	*height = GEDIT_STATE_DEFAULT_WINDOW_HEIGHT;
}

void
gedit_settings_set_window_size (gint width, gint height)
{
	g_return_if_fail (width > -1 && height > -1);

	window_width = width;
	window_height = height;

	gedit_state_set_int (GEDIT_STATE_WINDOW_GROUP,
			     GEDIT_STATE_WINDOW_WIDTH,
			     width);
	gedit_state_set_int (GEDIT_STATE_WINDOW_GROUP,
			     GEDIT_STATE_WINDOW_HEIGHT,
			     height);
}

gboolean 
gedit_settings_window_size_can_set (void)
{
	return TRUE;
}

/* Side panel */
gint
gedit_settings_get_side_panel_size (void)
{
	if (side_panel_size == -1)
	{
		gedit_state_get_int (GEDIT_STATE_WINDOW_GROUP,
				     GEDIT_STATE_SIDE_PANEL_SIZE,
				     GEDIT_STATE_DEFAULT_SIDE_PANEL_SIZE,
				     &side_panel_size);
	}

	return side_panel_size;
}

gint 
gedit_settings_get_default_side_panel_size (void)
{
	return GEDIT_STATE_DEFAULT_SIDE_PANEL_SIZE;
}

void 
gedit_settings_set_side_panel_size (gint ps)
{
	g_return_if_fail (ps > -1);
	
	if (side_panel_size == ps)
		return;
		
	side_panel_size = ps;
	gedit_state_set_int (GEDIT_STATE_WINDOW_GROUP,
			     GEDIT_STATE_SIDE_PANEL_SIZE,
			     ps);
}

gboolean 
gedit_settings_side_panel_size_can_set (void)
{
	return TRUE;
}

gint
gedit_settings_get_side_panel_active_page (void)
{
	if (side_panel_active_page == -1)
	{
		gedit_state_get_int (GEDIT_STATE_WINDOW_GROUP,
				     GEDIT_STATE_SIDE_PANEL_ACTIVE_PAGE,
				     0,
				     &side_panel_active_page);
	}

	return side_panel_active_page;
}

void
gedit_settings_set_side_panel_active_page (gint id)
{
	if (side_panel_active_page == id)
		return;

	side_panel_active_page = id;
	gedit_state_set_int (GEDIT_STATE_WINDOW_GROUP,
			     GEDIT_STATE_SIDE_PANEL_ACTIVE_PAGE,
			     id);
}

gboolean 
gedit_settings_side_panel_active_page_can_set (void)
{
	return TRUE;
}

/* Bottom panel */
gint
gedit_settings_get_bottom_panel_size (void)
{
	if (bottom_panel_size == -1)
	{
		gedit_state_get_int (GEDIT_STATE_WINDOW_GROUP,
				     GEDIT_STATE_BOTTOM_PANEL_SIZE,
				     GEDIT_STATE_DEFAULT_BOTTOM_PANEL_SIZE,
				     &bottom_panel_size);
	}

	return bottom_panel_size;
}

gint 
gedit_settings_get_default_bottom_panel_size (void)
{
	return GEDIT_STATE_DEFAULT_BOTTOM_PANEL_SIZE;
}

void 
gedit_settings_set_bottom_panel_size (gint ps)
{
	g_return_if_fail (ps > -1);

	if (bottom_panel_size == ps)
		return;
	
	bottom_panel_size = ps;
	gedit_state_set_int (GEDIT_STATE_WINDOW_GROUP,
			     GEDIT_STATE_BOTTOM_PANEL_SIZE,
			     ps);
}

gboolean 
gedit_settings_bottom_panel_size_can_set (void)
{
	return TRUE;
}

gint
gedit_settings_get_bottom_panel_active_page (void)
{
	if (bottom_panel_active_page == -1)
	{
		gedit_state_get_int (GEDIT_STATE_WINDOW_GROUP,
				     GEDIT_STATE_BOTTOM_PANEL_ACTIVE_PAGE,
				     0,
				     &bottom_panel_active_page);
	}

	return bottom_panel_active_page;
}

void
gedit_settings_set_bottom_panel_active_page (gint id)
{
	if (bottom_panel_active_page == id)
		return;

	bottom_panel_active_page = id;
	gedit_state_set_int (GEDIT_STATE_WINDOW_GROUP,
			     GEDIT_STATE_BOTTOM_PANEL_ACTIVE_PAGE,
			     id);
}

gboolean 
gedit_settings_bottom_panel_active_page_can_set (void)
{
	return TRUE;
}

/* File filter */
gint
gedit_settings_get_active_file_filter (void)
{
	if (active_file_filter == -1)
	{
		gedit_state_get_int (GEDIT_STATE_FILEFILTER_GROUP,
				     GEDIT_STATE_FILEFILTER_ID,
				     0,
				     &active_file_filter);
	}

	return active_file_filter;
}

void
gedit_settings_set_active_file_filter (gint id)
{
	g_return_if_fail (id >= 0);
	
	if (active_file_filter == id)
		return;

	active_file_filter = id;
	gedit_state_set_int (GEDIT_STATE_FILEFILTER_GROUP,
			     GEDIT_STATE_FILEFILTER_ID,
			     id);
}

gboolean 
gedit_settings_active_file_filter_can_set (void)
{
	return TRUE;
}

static void
gedit_settings_finalize (GObject *object)
{
	GeditSettings *gs = GEDIT_SETTINGS (object);
	
	g_free (gs->priv->old_scheme);
	
	gedit_state_file_sync ();

	G_OBJECT_CLASS (gedit_settings_parent_class)->finalize (object);
}

static void
gedit_settings_dispose (GObject *object)
{
	GeditSettings *gs = GEDIT_SETTINGS (object);
	
	if (gs->priv->lockdown != NULL)
	{
		g_object_unref (gs->priv->lockdown);
		gs->priv->lockdown = NULL;
	}
	
	if (gs->priv->interface != NULL)
	{
		g_object_unref (gs->priv->interface);
		gs->priv->interface = NULL;
	}
	
	if (gs->priv->editor != NULL)
	{
		g_object_unref (gs->priv->editor);
		gs->priv->editor = NULL;
	}
	
	if (gs->priv->ui != NULL)
	{
		g_object_unref (gs->priv->ui);
		gs->priv->ui = NULL;
	}
	
	if (gs->priv->plugins != NULL)
	{
		g_object_unref (gs->priv->plugins);
		gs->priv->plugins = NULL;
	}

	G_OBJECT_CLASS (gedit_settings_parent_class)->dispose (object);
}

static void
gedit_settings_class_init (GeditSettingsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	object_class->finalize = gedit_settings_finalize;
	object_class->dispose = gedit_settings_dispose;

	g_type_class_add_private (object_class, sizeof (GeditSettingsPrivate));
}

static void
on_lockdown_changed (GSettings   *settings,
		     const gchar *key,
		     gpointer     useless)
{
	gboolean locked;
	GeditApp *app;
	
	locked = g_settings_get_boolean (settings, key);
	app = gedit_app_get_default ();
	
	if (strcmp (key, GS_LOCKDOWN_COMMAND_LINE) == 0)
		_gedit_app_set_lockdown_bit (app, 
					     GEDIT_LOCKDOWN_COMMAND_LINE,
					     locked);
	else if (strcmp (key, GS_LOCKDOWN_PRINTING) == 0)
		_gedit_app_set_lockdown_bit (app, 
					     GEDIT_LOCKDOWN_PRINTING,
					     locked);
	else if (strcmp (key, GS_LOCKDOWN_PRINT_SETUP) == 0)
		_gedit_app_set_lockdown_bit (app, 
					     GEDIT_LOCKDOWN_PRINT_SETUP,
					     locked);
	else if (strcmp (key, GS_LOCKDOWN_SAVE_TO_DISK) == 0)
		_gedit_app_set_lockdown_bit (app, 
					     GEDIT_LOCKDOWN_SAVE_TO_DISK,
					     locked);
}

static void
set_font (GeditSettings *gs,
	  const gchar *font)
{
	GList *views, *l;
	guint ts;
	
	g_settings_get (gs->priv->editor, GS_TABS_SIZE, "u", &ts);
	
	views = gedit_app_get_views (gedit_app_get_default ());
	
	for (l = views; l != NULL; l = g_list_next (l))
	{
		/* Note: we use def=FALSE to avoid GeditView to query gconf */
		gedit_view_set_font (GEDIT_VIEW (l->data), FALSE, font);

		gtk_source_view_set_tab_width (GTK_SOURCE_VIEW (l->data), ts);
	}
	
	g_list_free (views);
}

static void
on_system_font_changed (GSettings     *settings,
			const gchar   *key,
			GeditSettings *gs)
{
	
	gboolean use_default_font;
	gchar *font;
	
	use_default_font = g_settings_get_boolean (gs->priv->editor,
						   GS_USE_DEFAULT_FONT);
	if (!use_default_font)
		return;
	
	font = g_settings_get_string (settings, key);
	
	set_font (gs, font);
	
	g_free (font);
}

static void
on_use_default_font_changed (GSettings     *settings,
			     const gchar   *key,
			     GeditSettings *gs)
{
	gboolean def;
	gchar *font;

	def = g_settings_get_boolean (settings, key);
	
	if (def)
	{
		font = g_settings_get_string (gs->priv->interface, GS_SYSTEM_FONT);
	}
	else
	{
		font = g_settings_get_string (gs->priv->editor, GS_EDITOR_FONT);
	}
	
	set_font (gs, font);
	
	g_free (font);
}

static void
on_editor_font_changed (GSettings     *settings,
			const gchar   *key,
			GeditSettings *gs)
{
	gboolean use_default_font;
	gchar *font;
	
	use_default_font = g_settings_get_boolean (gs->priv->editor,
						   GS_USE_DEFAULT_FONT);
	if (use_default_font)
		return;
	
	font = g_settings_get_string (settings, key);
	
	set_font (gs, font);
	
	g_free (font);
}

static void
on_scheme_changed (GSettings     *settings,
		   const gchar   *key,
		   GeditSettings *gs)
{
	GtkSourceStyleScheme *style;
	gchar *scheme;
	GList *docs;
	GList *l;

	scheme = g_settings_get_string (settings, key);

	if (gs->priv->old_scheme != NULL && (strcmp (scheme, gs->priv->old_scheme) == 0))
		return;

	g_free (gs->priv->old_scheme);
	gs->priv->old_scheme = scheme;

	style = gtk_source_style_scheme_manager_get_scheme (
			gedit_get_style_scheme_manager (),
			scheme);

	if (style == NULL)
	{
		g_warning ("Default style scheme '%s' not found, falling back to 'classic'", scheme);
		
		style = gtk_source_style_scheme_manager_get_scheme (
			gedit_get_style_scheme_manager (),
			"classic");

		if (style == NULL) 
		{
			g_warning ("Style scheme 'classic' cannot be found, check your GtkSourceView installation.");
			return;
		}
	}

	docs = gedit_app_get_documents (gedit_app_get_default ());
	for (l = docs; l != NULL; l = g_list_next (l))
	{
		g_return_if_fail (GTK_IS_SOURCE_BUFFER (l->data));

		gtk_source_buffer_set_style_scheme (GTK_SOURCE_BUFFER (l->data),
						    style);
	}

	g_list_free (docs);
}

static void
on_auto_save_changed (GSettings     *settings,
		      const gchar   *key,
		      GeditSettings *gs)
{
	GList *docs, *l;
	gboolean auto_save;
	
	auto_save = g_settings_get_boolean (settings, key);
	
	docs = gedit_app_get_documents (gedit_app_get_default ());
	
	for (l = docs; l != NULL; l = g_list_next (l))
	{
		GeditTab *tab = gedit_tab_get_from_document (GEDIT_DOCUMENT (l->data));
		
		gedit_tab_set_auto_save_enabled (tab, auto_save);
	}
	
	g_list_free (docs);
}

static void
on_auto_save_interval_changed (GSettings     *settings,
			       const gchar   *key,
			       GeditSettings *gs)
{
	GList *docs, *l;
	gint auto_save_interval;
	
	g_settings_get (settings, key, "u", &auto_save_interval);
	
	docs = gedit_app_get_documents (gedit_app_get_default ());
	
	for (l = docs; l != NULL; l = g_list_next (l))
	{
		GeditTab *tab = gedit_tab_get_from_document (GEDIT_DOCUMENT (l->data));
		
		gedit_tab_set_auto_save_interval (tab, auto_save_interval);
	}
	
	g_list_free (docs);
}

static void
on_undo_actions_limit_changed (GSettings     *settings,
			       const gchar   *key,
			       GeditSettings *gs)
{
	GList *docs, *l;
	gint ul;
	
	g_settings_get (settings, key, "u", &ul);
	
	ul = CLAMP (ul, -1, 250);
	
	docs = gedit_app_get_documents (gedit_app_get_default ());
	
	for (l = docs; l != NULL; l = g_list_next (l))
	{
		gtk_source_buffer_set_max_undo_levels (GTK_SOURCE_BUFFER (l->data),
						       ul);
	}
	
	g_list_free (docs);
}

static void
on_wrap_mode_changed (GSettings     *settings,
		      const gchar   *key,
		      GeditSettings *gs)
{
	GtkWrapMode wrap_mode;
	GList *views, *l;
	gchar *wrap_str;
	
	wrap_str = g_settings_get_string (settings, key);
	
	wrap_mode = gedit_utils_get_wrap_mode_from_string (wrap_str);
	g_free (wrap_str);
	
	views = gedit_app_get_views (gedit_app_get_default ());
	
	for (l = views; l != NULL; l = g_list_next (l))
	{
		gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (l->data),
					     wrap_mode);
	}
	
	g_list_free (views);
}

static void
on_tabs_size_changed (GSettings     *settings,
		      const gchar   *key,
		      GeditSettings *gs)
{
	GList *views, *l;
	guint ts;
	
	g_settings_get (settings, key, "u", &ts);
	
	ts = CLAMP (ts, 1, 24);
	
	views = gedit_app_get_views (gedit_app_get_default ());
	
	for (l = views; l != NULL; l = g_list_next (l))
	{
		gtk_source_view_set_tab_width (GTK_SOURCE_VIEW (l->data),
					       ts);
	}
	
	g_list_free (views);
}

/* FIXME: insert_spaces and line_numbers are mostly the same it just changes
 the func called, maybe typedef the func and refactorize? */
static void
on_insert_spaces_changed (GSettings     *settings,
			  const gchar   *key,
			  GeditSettings *gs)
{
	GList *views, *l;
	gboolean spaces;
	
	spaces = g_settings_get_boolean (settings, key);
	
	views = gedit_app_get_views (gedit_app_get_default ());
	
	for (l = views; l != NULL; l = g_list_next (l))
	{
		gtk_source_view_set_insert_spaces_instead_of_tabs (
					GTK_SOURCE_VIEW (l->data),
					spaces);
	}
	
	g_list_free (views);
}

static void
on_auto_indent_changed (GSettings     *settings,
			const gchar   *key,
			GeditSettings *gs)
{
	GList *views, *l;
	gboolean enable;
	
	enable = g_settings_get_boolean (settings, key);
	
	views = gedit_app_get_views (gedit_app_get_default ());
	
	for (l = views; l != NULL; l = g_list_next (l))
	{
		gtk_source_view_set_auto_indent (GTK_SOURCE_VIEW (l->data),
						 enable);
	}
	
	g_list_free (views);
}

static void
on_display_line_numbers_changed (GSettings     *settings,
				 const gchar   *key,
				 GeditSettings *gs)
{
	GList *views, *l;
	gboolean line_numbers;
	
	line_numbers = g_settings_get_boolean (settings, key);
	
	views = gedit_app_get_views (gedit_app_get_default ());
	
	for (l = views; l != NULL; l = g_list_next (l))
	{
		gtk_source_view_set_show_line_numbers (GTK_SOURCE_VIEW (l->data),
						       line_numbers);
	}
	
	g_list_free (views);
}

static void
on_hl_current_line_changed (GSettings     *settings,
			    const gchar   *key,
			    GeditSettings *gs)
{
	GList *views, *l;
	gboolean hl;
	
	hl = g_settings_get_boolean (settings, key);
	
	views = gedit_app_get_views (gedit_app_get_default ());
	
	for (l = views; l != NULL; l = g_list_next (l))
	{
		gtk_source_view_set_highlight_current_line (GTK_SOURCE_VIEW (l->data),
							    hl);
	}
	
	g_list_free (views);
}

static void
on_bracket_matching_changed (GSettings     *settings,
			     const gchar   *key,
			     GeditSettings *gs)
{
	GList *docs, *l;
	gboolean enable;
	
	enable = g_settings_get_boolean (settings, key);
	
	docs = gedit_app_get_documents (gedit_app_get_default ());
	
	for (l = docs; l != NULL; l = g_list_next (l))
	{
		gtk_source_buffer_set_highlight_matching_brackets (GTK_SOURCE_BUFFER (l->data),
								   enable);
	}
	
	g_list_free (docs);
}

static void
on_display_right_margin_changed (GSettings     *settings,
				 const gchar   *key,
				 GeditSettings *gs)
{
	GList *views, *l;
	gboolean display;
	
	display = g_settings_get_boolean (settings, key);
	
	views = gedit_app_get_views (gedit_app_get_default ());
	
	for (l = views; l != NULL; l = g_list_next (l))
	{
		gtk_source_view_set_show_right_margin (GTK_SOURCE_VIEW (l->data),
						       display);
	}
	
	g_list_free (views);
}

static void
on_right_margin_position_changed (GSettings     *settings,
				  const gchar   *key,
				  GeditSettings *gs)
{
	GList *views, *l;
	gint pos;
	
	g_settings_get (settings, key, "u", &pos);
	
	pos = CLAMP (pos, 1, 160);
	
	views = gedit_app_get_views (gedit_app_get_default ());
	
	for (l = views; l != NULL; l = g_list_next (l))
	{
		gtk_source_view_set_right_margin_position (GTK_SOURCE_VIEW (l->data),
							   pos);
	}
	
	g_list_free (views);
}

static void
on_smart_home_end_changed (GSettings     *settings,
			   const gchar   *key,
			   GeditSettings *gs)
{
	GtkSourceSmartHomeEndType smart_he;
	GList *views, *l;
	gchar *smart_str;
	
	smart_str = g_settings_get_string (settings, key);
	
	smart_he = gedit_utils_get_smart_home_end_from_string (smart_str);
	g_free (smart_str);
	
	views = gedit_app_get_views (gedit_app_get_default ());
	
	for (l = views; l != NULL; l = g_list_next (l))
	{
		gtk_source_view_set_smart_home_end (GTK_SOURCE_VIEW (l->data),
						    smart_he);
	}
	
	g_list_free (views);
}

static void
on_syntax_highlighting_changed (GSettings     *settings,
				const gchar   *key,
				GeditSettings *gs)
{
	const GList *windows;
	GList *docs, *l;
	gboolean enable;
	
	enable = g_settings_get_boolean (settings, key);
	
	docs = gedit_app_get_documents (gedit_app_get_default ());
	
	for (l = docs; l != NULL; l = g_list_next (l))
	{
		gtk_source_buffer_set_highlight_syntax (GTK_SOURCE_BUFFER (l->data),
							enable);
	}
	
	g_list_free (docs);

	/* update the sensitivity of the Higlight Mode menu item */
	windows = gedit_app_get_windows (gedit_app_get_default ());
	while (windows != NULL)
	{
		GtkUIManager *ui;
		GtkAction *a;

		ui = gedit_window_get_ui_manager (GEDIT_WINDOW (windows->data));

		a = gtk_ui_manager_get_action (ui,
					       "/MenuBar/ViewMenu/ViewHighlightModeMenu");

		gtk_action_set_sensitive (a, enable);

		windows = g_list_next (windows);
	}
}

static void
on_search_highlighting_changed (GSettings     *settings,
				const gchar   *key,
				GeditSettings *gs)
{
	GList *docs, *l;
	gboolean enable;
	
	enable = g_settings_get_boolean (settings, key);
	
	docs = gedit_app_get_documents (gedit_app_get_default ());
	
	for (l = docs; l != NULL; l = g_list_next (l))
	{
		gedit_document_set_enable_search_highlighting  (GEDIT_DOCUMENT (l->data),
								enable);
	}
	
	g_list_free (docs);
}

static void
on_max_recents_changed (GSettings     *settings,
			const gchar   *key,
			GeditSettings *gs)
{
	const GList *windows;
	gint max;
	
	g_settings_get (settings, key, "u", &max);
	
	windows = gedit_app_get_windows (gedit_app_get_default ());
	while (windows != NULL)
	{
		GeditWindow *w = GEDIT_WINDOW (windows->data);

		gtk_recent_chooser_set_limit (GTK_RECENT_CHOOSER (w->priv->toolbar_recent_menu),
					      max);

		windows = g_list_next (windows);
	}
	
	/* FIXME: we have no way at the moment to trigger the
	 * update of the inline recents in the File menu */
}

static void
on_active_plugins_changed (GSettings     *settings,
			   const gchar   *key,
			   GeditSettings *gs)
{
	GeditPluginsEngine *engine;

	engine = gedit_plugins_engine_get_default ();

	gedit_plugins_engine_active_plugins_changed (engine);
}

static void
gedit_settings_init (GeditSettings *gs)
{
	gs->priv = GEDIT_SETTINGS_GET_PRIVATE (gs);
	
	gs->priv->old_scheme = NULL;
}

static void
initialize (GeditSettings *gs)
{
	GSettings *prefs;
	
	prefs = g_settings_get_child (G_SETTINGS (gs), "preferences");
	gs->priv->editor = g_settings_get_child (prefs, "editor");
	gs->priv->ui = g_settings_get_child (prefs, "ui");
	g_object_unref (prefs);
	gs->priv->plugins = g_settings_get_child (G_SETTINGS (gs), "plugins");
	
	/* Load settings */
	gs->priv->lockdown = g_settings_new ("org.gnome.Desktop.Lockdown");
	
	g_signal_connect (gs->priv->lockdown,
			  "changed",
			  G_CALLBACK (on_lockdown_changed),
			  NULL);

	gs->priv->interface = g_settings_new ("org.gnome.Desktop.Interface");
	
	g_signal_connect (gs->priv->interface,
			  "changed",
			  G_CALLBACK (on_system_font_changed),
			  gs);

	/* editor changes */
	g_signal_connect (gs->priv->editor,
			  "changed::use-default-font",
			  G_CALLBACK (on_use_default_font_changed),
			  gs);
	g_signal_connect (gs->priv->editor,
			  "changed::editor-font",
			  G_CALLBACK (on_editor_font_changed),
			  gs);
	g_signal_connect (gs->priv->editor,
			  "changed::scheme",
			  G_CALLBACK (on_scheme_changed),
			  gs);
	g_signal_connect (gs->priv->editor,
			  "changed::auto-save",
			  G_CALLBACK (on_auto_save_changed),
			  gs);
	g_signal_connect (gs->priv->editor,
			  "changed::auto-save-interval",
			  G_CALLBACK (on_auto_save_interval_changed),
			  gs);
	g_signal_connect (gs->priv->editor,
			  "changed::undo-actions-limit",
			  G_CALLBACK (on_undo_actions_limit_changed),
			  gs);
	g_signal_connect (gs->priv->editor,
			  "changed::wrap-mode",
			  G_CALLBACK (on_wrap_mode_changed),
			  gs);
	g_signal_connect (gs->priv->editor,
			  "changed::tabs-size",
			  G_CALLBACK (on_tabs_size_changed),
			  gs);
	g_signal_connect (gs->priv->editor,
			  "changed::insert-spaces",
			  G_CALLBACK (on_insert_spaces_changed),
			  gs);
	g_signal_connect (gs->priv->editor,
			  "changed::auto-indent",
			  G_CALLBACK (on_auto_indent_changed),
			  gs);
	g_signal_connect (gs->priv->editor,
			  "changed::display-line-numbers",
			  G_CALLBACK (on_display_line_numbers_changed),
			  gs);
	g_signal_connect (gs->priv->editor,
			  "changed::highlight-current-line",
			  G_CALLBACK (on_hl_current_line_changed),
			  gs);
	g_signal_connect (gs->priv->editor,
			  "changed::bracket-matching",
			  G_CALLBACK (on_bracket_matching_changed),
			  gs);
	g_signal_connect (gs->priv->editor,
			  "changed::display-right-margin",
			  G_CALLBACK (on_display_right_margin_changed),
			  gs);
	g_signal_connect (gs->priv->editor,
			  "changed::right-margin-position",
			  G_CALLBACK (on_right_margin_position_changed),
			  gs);
	g_signal_connect (gs->priv->editor,
			  "changed::smart-home-end",
			  G_CALLBACK (on_smart_home_end_changed),
			  gs);
	g_signal_connect (gs->priv->editor,
			  "changed::syntax-highlighting",
			  G_CALLBACK (on_syntax_highlighting_changed),
			  gs);
	g_signal_connect (gs->priv->editor,
			  "changed::search-highlighting",
			  G_CALLBACK (on_search_highlighting_changed),
			  gs);

	/* ui changes */
	g_signal_connect (gs->priv->ui,
			  "changed::max-recents",
			  G_CALLBACK (on_max_recents_changed),
			  gs);

	/* plugins changes */
	g_signal_connect (gs->priv->plugins,
			  "changed::active-plugins",
			  G_CALLBACK (on_active_plugins_changed),
			  gs);
}

GSettings *
gedit_settings_new ()
{
	GeditSettings *settings;
	
	settings = g_object_new (GEDIT_TYPE_SETTINGS,
				 "schema", "org.gnome.gedit",
				 NULL);
	
	initialize (settings);
	
	return G_SETTINGS (settings);
}

GeditLockdownMask
gedit_settings_get_lockdown (GeditSettings *gs)
{
	guint lockdown = 0;
	gboolean command_line, printing, print_setup, save_to_disk;
	
	command_line = g_settings_get_boolean (gs->priv->lockdown,
					       GS_LOCKDOWN_COMMAND_LINE);
	printing = g_settings_get_boolean (gs->priv->lockdown,
					   GS_LOCKDOWN_PRINTING);
	print_setup = g_settings_get_boolean (gs->priv->lockdown,
					      GS_LOCKDOWN_PRINT_SETUP);
	save_to_disk = g_settings_get_boolean (gs->priv->lockdown,
					       GS_LOCKDOWN_SAVE_TO_DISK);

	if (command_line)
		lockdown |= GEDIT_LOCKDOWN_COMMAND_LINE;

	if (printing)
		lockdown |= GEDIT_LOCKDOWN_PRINTING;

	if (print_setup)
		lockdown |= GEDIT_LOCKDOWN_PRINT_SETUP;

	if (save_to_disk)
		lockdown |= GEDIT_LOCKDOWN_SAVE_TO_DISK;

	return lockdown;
}

gchar *
gedit_settings_get_system_font (GeditSettings *gs)
{
	gchar *system_font;

	g_return_val_if_fail (GEDIT_IS_SETTINGS (gs), NULL);
	
	system_font = g_settings_get_string (gs->priv->interface,
					     "monospace-font-name");
	
	return system_font;
}
