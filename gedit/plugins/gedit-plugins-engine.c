/*
 * gedit-plugins-engine.c
 * This file is part of gedit
 *
 * Copyright (C) 2002-2005 Paolo Maggi 
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
 * Modified by the gedit Team, 2002-2005. See the AUTHORS file for a 
 * list of people on the gedit Team.  
 * See the ChangeLog files for a list of changes. 
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib/gi18n.h>
#include <gconf/gconf-client.h>

#include "gedit-plugins-engine.h"
#include "gedit-plugin-info-priv.h"
#include "gedit-plugin.h"
#include "gedit-debug.h"
#include "gedit-app.h"

#include "gedit-module.h"
#ifdef ENABLE_PYTHON
#include "gedit-python-module.h"
#endif

#define USER_GEDIT_PLUGINS_LOCATION ".gnome2/gedit/plugins/"

#define GEDIT_PLUGINS_ENGINE_BASE_KEY "/apps/gedit-2/plugins"
#define GEDIT_PLUGINS_ENGINE_KEY GEDIT_PLUGINS_ENGINE_BASE_KEY "/active-plugins"

#define PLUGIN_EXT	".gedit-plugin"

/* Signals */
enum
{
	ACTIVATE_PLUGIN,
	DEACTIVATE_PLUGIN,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE(GeditPluginsEngine, gedit_plugins_engine, G_TYPE_OBJECT)

struct _GeditPluginsEnginePrivate
{
	GList *plugin_list;
	GConfClient *gconf_client;
};

GeditPluginsEngine *default_engine = NULL;

static void	gedit_plugins_engine_active_plugins_changed (GConfClient *client,
							     guint cnxn_id, 
							     GConfEntry *entry, 
							     gpointer user_data);
static void	gedit_plugins_engine_activate_plugin_real (GeditPluginsEngine *engine,
							   GeditPluginInfo    *info);
static void	gedit_plugins_engine_deactivate_plugin_real (GeditPluginsEngine *engine,
							     GeditPluginInfo    *info);

static gint
compare_plugin_info (GeditPluginInfo *info1,
		     GeditPluginInfo *info2)
{
	return strcmp (info1->module_name, info2->module_name);
}

static void
gedit_plugins_engine_load_dir (GeditPluginsEngine *engine,
			       const gchar        *dir,
			       GSList             *active_plugins)
{
	GError *error = NULL;
	GDir *d;
	const gchar *dirent;

	g_return_if_fail (engine->priv->gconf_client != NULL);
	g_return_if_fail (dir != NULL);

	gedit_debug_message (DEBUG_PLUGINS, "DIR: %s", dir);

	d = g_dir_open (dir, 0, &error);
	if (!d)
	{
		g_warning (error->message);
		g_error_free (error);
		return;
	}

	while ((dirent = g_dir_read_name (d)))
	{
		if (g_str_has_suffix (dirent, PLUGIN_EXT))
		{
			gchar *plugin_file;
			GeditPluginInfo *info;
			
			plugin_file = g_build_filename (dir, dirent, NULL);
			info = _gedit_plugin_info_new (plugin_file);
			g_free (plugin_file);

			if (info == NULL)
				continue;

			/* If a plugin with this name has already been loaded
			 * drop this one (user plugins override system plugins) */
			if (g_list_find_custom (engine->priv->plugin_list,
						info,
						(GCompareFunc)compare_plugin_info) != NULL)
			{
				g_warning ("Two or more plugins named '%s'. "
					   "Only the first will be considered.\n",
					   info->module_name);

				gedit_plugin_info_free (info);

				continue;
			}

			/* Actually, the plugin will be activated when reactivate_all
			 * will be called for the first time. */
			info->active = g_slist_find_custom (active_plugins,
							    info->module_name,
							    (GCompareFunc)strcmp) != NULL;

			engine->priv->plugin_list = g_list_prepend (engine->priv->plugin_list, info);

			gedit_debug_message (DEBUG_PLUGINS, "Plugin %s loaded", info->name);
		}
	}

	g_dir_close (d);
}

static void
gedit_plugins_engine_load_all (GeditPluginsEngine *engine)
{
	GSList *active_plugins;
	const gchar *home;
	const gchar *pdirs_env;
	gchar **pdirs;
	int i;

	active_plugins = gconf_client_get_list (engine->priv->gconf_client,
						GEDIT_PLUGINS_ENGINE_KEY,
						GCONF_VALUE_STRING,
						NULL);

	/* load user's plugins */
	home = g_get_home_dir ();
	if (home == NULL)
	{
		g_warning ("Could not get HOME directory\n");
	}
	else
	{
		gchar *pdir;

		pdir = g_build_filename (home,
					 USER_GEDIT_PLUGINS_LOCATION,
					 NULL);

		if (g_file_test (pdir, G_FILE_TEST_IS_DIR))
			gedit_plugins_engine_load_dir (engine, pdir, active_plugins);
		
		g_free (pdir);
	}

	pdirs_env = g_getenv ("GEDIT_PLUGINS_PATH");
	/* What if no env var is set? We use the default location(s)! */
	if (pdirs_env == NULL)
		pdirs_env = GEDIT_PLUGINDIR;

	gedit_debug_message (DEBUG_PLUGINS, "GEDIT_PLUGINS_PATH=%s", pdirs_env);
	pdirs = g_strsplit (pdirs_env, G_SEARCHPATH_SEPARATOR_S, 0);

	for (i = 0; pdirs[i] != NULL; i++)
		gedit_plugins_engine_load_dir (engine, pdirs[i], active_plugins);

	g_strfreev (pdirs);
}

static void
gedit_plugins_engine_init (GeditPluginsEngine *engine)
{
	gedit_debug (DEBUG_PLUGINS);

	if (!g_module_supported ())
	{
		g_warning ("gedit is not able to initialize the plugins engine.");
		return;
	}

	engine->priv = G_TYPE_INSTANCE_GET_PRIVATE (engine,
						    GEDIT_TYPE_PLUGINS_ENGINE,
						    GeditPluginsEnginePrivate);

	engine->priv->gconf_client = gconf_client_get_default ();
	g_return_if_fail (engine->priv->gconf_client != NULL);

	gconf_client_add_dir (engine->priv->gconf_client,
			      GEDIT_PLUGINS_ENGINE_BASE_KEY,
			      GCONF_CLIENT_PRELOAD_ONELEVEL,
			      NULL);

	gconf_client_notify_add (engine->priv->gconf_client,
				 GEDIT_PLUGINS_ENGINE_KEY,
				 gedit_plugins_engine_active_plugins_changed,
				 engine, NULL, NULL);

	gedit_plugins_engine_load_all (engine);
}

void
gedit_plugins_engine_garbage_collect (GeditPluginsEngine *engine)
{
#ifdef ENABLE_PYTHON
	gedit_python_garbage_collect ();
#endif
}

static void
gedit_plugins_engine_finalize (GObject *object)
{
	GeditPluginsEngine *engine = GEDIT_PLUGINS_ENGINE (object);

	gedit_debug (DEBUG_PLUGINS);

#ifdef ENABLE_PYTHON
	/* Note: that this may cause finalization of objects (typically
	 * the GeditWindow) by running the garbage collector. Since some
	 * of the plugin may have installed callbacks upon object
	 * finalization (typically they need to free the WindowData)
	 * it must run before we get rid of the plugins.
	 */
	gedit_python_shutdown ();
#endif

	g_return_if_fail (engine->priv->gconf_client != NULL);

	g_list_foreach (engine->priv->plugin_list,
			(GFunc) gedit_plugin_info_free, NULL);
	g_list_free (engine->priv->plugin_list);

	g_object_unref (engine->priv->gconf_client);
}

static void
gedit_plugins_engine_class_init (GeditPluginsEngineClass *klass)
{
	GType the_type = G_TYPE_FROM_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gedit_plugins_engine_finalize;
	klass->activate_plugin = gedit_plugins_engine_activate_plugin_real;
	klass->deactivate_plugin = gedit_plugins_engine_deactivate_plugin_real;

	signals[ACTIVATE_PLUGIN] =
		g_signal_new ("activate-plugin",
			      the_type,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GeditPluginsEngineClass, activate_plugin),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__BOXED,
			      G_TYPE_NONE,
			      1,
			      GEDIT_TYPE_PLUGIN_INFO | G_SIGNAL_TYPE_STATIC_SCOPE);

	signals[DEACTIVATE_PLUGIN] =
		g_signal_new ("deactivate-plugin",
			      the_type,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GeditPluginsEngineClass, deactivate_plugin),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__BOXED,
			      G_TYPE_NONE,
			      1,
			      GEDIT_TYPE_PLUGIN_INFO | G_SIGNAL_TYPE_STATIC_SCOPE);

	g_type_class_add_private (klass, sizeof (GeditPluginsEnginePrivate));
}

GeditPluginsEngine *
gedit_plugins_engine_get_default (void)
{
	if (default_engine != NULL)
		return default_engine;

	default_engine = GEDIT_PLUGINS_ENGINE (g_object_new (GEDIT_TYPE_PLUGINS_ENGINE, NULL));
	g_object_add_weak_pointer (G_OBJECT (default_engine),
				   (gpointer) &default_engine);
	return default_engine;
}

const GList *
gedit_plugins_engine_get_plugin_list (GeditPluginsEngine *engine)
{
	gedit_debug (DEBUG_PLUGINS);

	return engine->priv->plugin_list;
}

static gint
compare_plugin_module_name (GeditPluginInfo *info, 
			    const gchar     *name)
{
	return strcmp(gedit_plugin_info_get_module_name(info), name);
}

GeditPlugin *
gedit_plugins_engine_get_plugin	(GeditPluginsEngine *engine,
				 const gchar 	    *name)
{
	GList *item;
	GeditPluginInfo *info;
	
	gedit_debug (DEBUG_PLUGINS);
	
	item = g_list_find_custom (engine->priv->plugin_list,
				   name,
				   (GCompareFunc)compare_plugin_module_name);

	if (!item)
		return NULL;
	
	info = GEDIT_PLUGIN_INFO (item->data);
	
	/* CHECK: is this really what we want? */
	if (!gedit_plugin_info_is_active (info))
		return NULL;
		
	return gedit_plugin_info_get_plugin (info);
}

GeditPluginInfo *
gedit_plugins_engine_get_plugin_info (GeditPluginsEngine *engine,
				      const gchar 	 *name)
{
	GList *item;
	GeditPluginInfo *info;
	
	gedit_debug(DEBUG_PLUGINS);
	
	item = g_list_find_custom(engine->priv->plugin_list,
				  name,
				  (GCompareFunc)compare_plugin_module_name);

	if (!item)
		return NULL;
	
	return GEDIT_PLUGIN_INFO (item->data);
}

static gboolean
load_plugin_module (GeditPluginInfo *info)
{
	gchar *path;
	gchar *dirname;

	gedit_debug (DEBUG_PLUGINS);

	g_return_val_if_fail (info != NULL, FALSE);
	g_return_val_if_fail (info->file != NULL, FALSE);
	g_return_val_if_fail (info->module_name != NULL, FALSE);
	g_return_val_if_fail (info->plugin == NULL, FALSE);
	g_return_val_if_fail (info->available, FALSE);
	
	switch (info->loader)
	{
		case GEDIT_PLUGIN_LOADER_C:
			dirname = g_path_get_dirname (info->file);	
			g_return_val_if_fail (dirname != NULL, FALSE);

			path = g_module_build_path (dirname, info->module_name);
			g_free (dirname);
			g_return_val_if_fail (path != NULL, FALSE);
	
			info->module = G_TYPE_MODULE (gedit_module_new (path));
			g_free (path);
			
			break;

#ifdef ENABLE_PYTHON
		case GEDIT_PLUGIN_LOADER_PY:
		{
			gchar *dir;
			
			if (!gedit_python_init ())
			{
				/* Mark plugin as unavailable and fails */
				info->available = FALSE;
				
				g_warning ("Cannot load Python plugin '%s' since gedit "
				           "was not able to initialize the Python interpreter.",
				           info->name);

				return FALSE;
			}

			dir = g_path_get_dirname (info->file);
			
			g_return_val_if_fail ((info->module_name != NULL) &&
			                      (info->module_name[0] != '\0'),
			                      FALSE);

			info->module = G_TYPE_MODULE (
					gedit_python_module_new (dir, info->module_name));
					
			g_free (dir);
			break;
		}
#endif
		default:
			g_return_val_if_reached (FALSE);
	}

	if (!g_type_module_use (info->module))
	{
		switch (info->loader)
		{
			case GEDIT_PLUGIN_LOADER_C:
				g_warning ("Cannot load plugin '%s' since file '%s' cannot be read.",
					   info->name,			           
					   gedit_module_get_path (GEDIT_MODULE (info->module)));
				break;

			case GEDIT_PLUGIN_LOADER_PY:
				g_warning ("Cannot load Python plugin '%s' since file '%s' cannot be read.",
					   info->name,			           
					   info->module_name);
				break;

			default:
				g_return_val_if_reached (FALSE);				
		}
			   
		g_object_unref (G_OBJECT (info->module));
		info->module = NULL;

		/* Mark plugin as unavailable and fails */
		info->available = FALSE;
		
		return FALSE;
	}
	
	switch (info->loader)
	{
		case GEDIT_PLUGIN_LOADER_C:
			info->plugin = 
				GEDIT_PLUGIN (gedit_module_new_object (GEDIT_MODULE (info->module)));
			break;

#ifdef ENABLE_PYTHON
		case GEDIT_PLUGIN_LOADER_PY:
			info->plugin = 
				GEDIT_PLUGIN (gedit_python_module_new_object (GEDIT_PYTHON_MODULE (info->module)));
			break;
#endif

		default:
			g_return_val_if_reached (FALSE);
	}
	
	g_type_module_unuse (info->module);

	gedit_debug_message (DEBUG_PLUGINS, "End");
	
	return TRUE;
}

static void
save_active_plugin_list (GeditPluginsEngine *engine)
{
	GSList *active_plugins = NULL;
	GList *l;
	gboolean res;

	for (l = engine->priv->plugin_list; l != NULL; l = l->next)
	{
		const GeditPluginInfo *info = (const GeditPluginInfo *) l->data;
		if (info->active)
		{
			active_plugins = g_slist_prepend (active_plugins,
							  info->module_name);
		}
	}

	res = gconf_client_set_list (engine->priv->gconf_client,
				     GEDIT_PLUGINS_ENGINE_KEY,
				     GCONF_VALUE_STRING,
				     active_plugins,
				     NULL);

	if (!res)
		g_warning ("Error saving the list of active plugins.");

	g_slist_free (active_plugins);
}

static void
gedit_plugins_engine_activate_plugin_real (GeditPluginsEngine *engine,
					   GeditPluginInfo *info)
{
	gboolean res = TRUE;

	if (info->active || !info->available)
		return;

	if (info->plugin == NULL)
		res = load_plugin_module (info);

	if (res)
	{
		const GList *wins = gedit_app_get_windows (gedit_app_get_default ());
		for (; wins != NULL; wins = wins->next)
			gedit_plugin_activate (info->plugin, GEDIT_WINDOW (wins->data));

		info->active = TRUE;
	}
	else
		g_warning ("Error activating plugin '%s'", info->name);
}

gboolean
gedit_plugins_engine_activate_plugin (GeditPluginsEngine *engine,
				      GeditPluginInfo    *info)
{
	gedit_debug (DEBUG_PLUGINS);

	g_return_val_if_fail (info != NULL, FALSE);

	if (!info->available)
		return FALSE;
		
	if (info->active)
		return TRUE;

	g_signal_emit (engine, signals[ACTIVATE_PLUGIN], 0, info);
	if (info->active)
		save_active_plugin_list (engine);

	return info->active;
}

static void
gedit_plugins_engine_deactivate_plugin_real (GeditPluginsEngine *engine,
					     GeditPluginInfo *info)
{
	const GList *wins;

	if (!info->active || !info->available)
		return;

	wins = gedit_app_get_windows (gedit_app_get_default ());
	for (; wins != NULL; wins = wins->next)
		gedit_plugin_deactivate (info->plugin, GEDIT_WINDOW (wins->data));

	info->active = FALSE;
}

gboolean
gedit_plugins_engine_deactivate_plugin (GeditPluginsEngine *engine,
					GeditPluginInfo    *info)
{
	gedit_debug (DEBUG_PLUGINS);

	g_return_val_if_fail (info != NULL, FALSE);

	if (!info->active || !info->available)
		return TRUE;

	g_signal_emit (engine, signals[DEACTIVATE_PLUGIN], 0, info);
	if (!info->active)
		save_active_plugin_list (engine);

	return !info->active;
}

static void
reactivate_all (GeditPluginsEngine *engine,
		GeditWindow        *window)
{
	GList *pl;

	gedit_debug (DEBUG_PLUGINS);

	for (pl = engine->priv->plugin_list; pl; pl = pl->next)
	{
		gboolean res = TRUE;
		
		GeditPluginInfo *info = (GeditPluginInfo*)pl->data;

		/* If plugin is not available, don't try to activate/load it */
		if (info->available && info->active)
		{		
			if (info->plugin == NULL)
				res = load_plugin_module (info);
				
			if (res)
				gedit_plugin_activate (info->plugin,
						       window);			
		}
	}
	
	gedit_debug_message (DEBUG_PLUGINS, "End");
}

void
gedit_plugins_engine_update_plugins_ui (GeditPluginsEngine *engine,
					GeditWindow        *window,
					gboolean            new_window)
{
	GList *pl;

	gedit_debug (DEBUG_PLUGINS);

	g_return_if_fail (GEDIT_IS_WINDOW (window));

	if (new_window)
		reactivate_all (engine, window);

	/* updated ui of all the plugins that implement update_ui */
	for (pl = engine->priv->plugin_list; pl; pl = pl->next)
	{
		GeditPluginInfo *info = (GeditPluginInfo*)pl->data;

		if (!info->available || !info->active)
			continue;
			
	       	gedit_debug_message (DEBUG_PLUGINS, "Updating UI of %s", info->name);
		
		gedit_plugin_update_ui (info->plugin, window);
	}
}

void 	 
gedit_plugins_engine_configure_plugin (GeditPluginsEngine *engine,
				       GeditPluginInfo    *info,
				       GtkWindow          *parent)
{
	GtkWidget *conf_dlg;
	
	GtkWindowGroup *wg;
	
	gedit_debug (DEBUG_PLUGINS);

	g_return_if_fail (info != NULL);

	conf_dlg = gedit_plugin_create_configure_dialog (info->plugin);
	g_return_if_fail (conf_dlg != NULL);
	gtk_window_set_transient_for (GTK_WINDOW (conf_dlg),
				      parent);

	wg = parent->group;		      
	if (wg == NULL)
	{
		wg = gtk_window_group_new ();
		gtk_window_group_add_window (wg, parent);
	}
			
	gtk_window_group_add_window (wg,
				     GTK_WINDOW (conf_dlg));
		
	gtk_window_set_modal (GTK_WINDOW (conf_dlg), TRUE);		     
	gtk_widget_show (conf_dlg);
}

static void 
gedit_plugins_engine_active_plugins_changed (GConfClient *client,
					     guint cnxn_id,
					     GConfEntry *entry,
					     gpointer user_data)
{
	GeditPluginsEngine *engine;
	GList *pl;
	gboolean to_activate;
	GSList *active_plugins;

	gedit_debug (DEBUG_PLUGINS);

	g_return_if_fail (entry->key != NULL);
	g_return_if_fail (entry->value != NULL);

	engine = GEDIT_PLUGINS_ENGINE (user_data);
	
	if (!((entry->value->type == GCONF_VALUE_LIST) && 
	      (gconf_value_get_list_type (entry->value) == GCONF_VALUE_STRING)))
	{
		g_warning ("The gconf key '%s' may be corrupted.", GEDIT_PLUGINS_ENGINE_KEY);
		return;
	}
	
	active_plugins = gconf_client_get_list (engine->priv->gconf_client,
						GEDIT_PLUGINS_ENGINE_KEY,
						GCONF_VALUE_STRING,
						NULL);

	for (pl = engine->priv->plugin_list; pl; pl = pl->next)
	{
		GeditPluginInfo *info = (GeditPluginInfo*)pl->data;

		if (!info->available)
			continue;

		to_activate = (g_slist_find_custom (active_plugins,
						    info->module_name,
						    (GCompareFunc)strcmp) != NULL);

		if (!info->active && to_activate)
			g_signal_emit (engine, signals[ACTIVATE_PLUGIN], 0, info);
		else if (info->active && !to_activate)
			g_signal_emit (engine, signals[DEACTIVATE_PLUGIN], 0, info);
	}

	g_slist_foreach (active_plugins, (GFunc) g_free, NULL);
	g_slist_free (active_plugins);
}


