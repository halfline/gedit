/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gedit-mdi.c
 * This file is part of gedit
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi 
 * Copyright (C) 2002  Paolo Maggi 
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
 * Modified by the gedit Team, 1998-2002. See the AUTHORS file for a 
 * list of people on the gedit Team.  
 * See the ChangeLog files for a list of changes. 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>
#include <libgnomevfs/gnome-vfs.h>

#include <string.h>

#include "gedit-mdi.h"
#include "gedit-mdi-child.h"
#include "gedit2.h"
#include "gedit-menus.h"
#include "gedit-debug.h"
#include "gedit-prefs-manager.h"
#include "gedit-recent.h" 
#include "gedit-file.h"
#include "gedit-view.h"
#include "gedit-utils.h"
#include "gedit-plugins-engine.h"
#include "gnome-recent-view-bonobo.h"

#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-control.h>

#include <gconf/gconf-client.h>

#define RECENT_KEY "GeditRecent"

struct _GeditMDIPrivate
{
	gint dummy;
};

typedef struct _GeditWindowPrefs	GeditWindowPrefs;

struct _GeditWindowPrefs
{
	gboolean toolbar_visible;
	GeditToolbarSetting toolbar_buttons_style;

	gboolean statusbar_visible;
	gboolean statusbar_show_cursor_position;
	gboolean statusbar_show_overwrite_mode;
};

static void gedit_mdi_class_init 	(GeditMDIClass	*klass);
static void gedit_mdi_init 		(GeditMDI 	*mdi);
static void gedit_mdi_finalize 		(GObject 	*object);

static void gedit_mdi_app_created_handler	(BonoboMDI *mdi, BonoboWindow *win);
static void gedit_mdi_drag_data_received_handler (GtkWidget *widget, GdkDragContext *context, 
		                                  gint x, gint y, 
						  GtkSelectionData *selection_data, 
				                  guint info, guint time);
static void gedit_mdi_set_app_toolbar_style 	(BonoboWindow *win);
static void gedit_mdi_set_app_statusbar_style 	(BonoboWindow *win);

static gint gedit_mdi_add_child_handler (BonoboMDI *mdi, BonoboMDIChild *child);
static gint gedit_mdi_add_view_handler (BonoboMDI *mdi, GtkWidget *view);
static gint gedit_mdi_remove_child_handler (BonoboMDI *mdi, BonoboMDIChild *child);
static gint gedit_mdi_remove_view_handler (BonoboMDI *mdi, GtkWidget *view);

static void gedit_mdi_view_changed_handler (BonoboMDI *mdi, GtkWidget *old_view);
static void gedit_mdi_child_changed_handler (BonoboMDI *mdi, BonoboMDIChild *old_child);
static void gedit_mdi_child_state_changed_handler (GeditMDIChild *child);

static void gedit_mdi_set_active_window_undo_redo_verbs_sensitivity (BonoboMDI *mdi);

static void gedit_mdi_view_menu_item_toggled_handler (
			BonoboUIComponent           *ui_component,
			const char                  *path,
			Bonobo_UIComponent_EventType type,
			const char                  *state,
			BonoboWindow                *win);


static GQuark window_prefs_id = 0;

static GeditWindowPrefs *gedit_window_prefs_new 		(void);
static void		 gedit_window_prefs_attach_to_window 	(GeditWindowPrefs *prefs,
								 BonoboWindow 	  *win);
static GeditWindowPrefs	*gedit_window_prefs_get_from_window 	(BonoboWindow     *win);
static void		 gedit_window_prefs_save 		(GeditWindowPrefs *prefs);

static BonoboMDIClass *parent_class = NULL;

GType
gedit_mdi_get_type (void)
{
	static GType mdi_type = 0;

  	if (mdi_type == 0)
    	{
      		static const GTypeInfo our_info =
      		{
        		sizeof (GeditMDIClass),
        		NULL,		/* base_init */
        		NULL,		/* base_finalize */
        		(GClassInitFunc) gedit_mdi_class_init,
        		NULL,           /* class_finalize */
        		NULL,           /* class_data */
        		sizeof (GeditMDI),
        		0,              /* n_preallocs */
        		(GInstanceInitFunc) gedit_mdi_init
      		};

      		mdi_type = g_type_register_static (BONOBO_TYPE_MDI,
                				    "GeditMDI",
                                       	 	    &our_info,
                                       		    0);
    	}

	return mdi_type;
}

static void
gedit_mdi_class_init (GeditMDIClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

  	parent_class = g_type_class_peek_parent (klass);

  	object_class->finalize = gedit_mdi_finalize;
}

static void 
gedit_mdi_init (GeditMDI  *mdi)
{
	gedit_debug (DEBUG_MDI, "START");

	bonobo_mdi_construct (BONOBO_MDI (mdi), "gedit-2", "gedit");
	
	mdi->priv = g_new0 (GeditMDIPrivate, 1);

	bonobo_mdi_set_ui_template_file (BONOBO_MDI (mdi), GEDIT_UI_DIR "gedit-ui.xml", gedit_verbs);
	
	bonobo_mdi_set_child_list_path (BONOBO_MDI (mdi), "/menu/Documents/");

	/* Connect signals */
	g_signal_connect (G_OBJECT (mdi), "top_window_created",
			  G_CALLBACK (gedit_mdi_app_created_handler), NULL);
	
	g_signal_connect (G_OBJECT (mdi), "add_child",
			  G_CALLBACK (gedit_mdi_add_child_handler), NULL);
	g_signal_connect (G_OBJECT (mdi), "add_view",
			  G_CALLBACK (gedit_mdi_add_view_handler), NULL);
	
	g_signal_connect (G_OBJECT (mdi), "remove_child",
			  G_CALLBACK (gedit_mdi_remove_child_handler), NULL);
	g_signal_connect (G_OBJECT (mdi), "remove_view",
			  G_CALLBACK (gedit_mdi_remove_view_handler), NULL);

	g_signal_connect (G_OBJECT (mdi), "child_changed",
			  G_CALLBACK (gedit_mdi_child_changed_handler), NULL);
	g_signal_connect (G_OBJECT (mdi), "view_changed",
			  G_CALLBACK (gedit_mdi_view_changed_handler), NULL);
	
	g_signal_connect (G_OBJECT (mdi), "all_windows_destroyed",
			  G_CALLBACK (gedit_file_exit), NULL);
			  
	gedit_debug (DEBUG_MDI, "END");
}

static void
gedit_mdi_finalize (GObject *object)
{
	GeditMDI *mdi;

	gedit_debug (DEBUG_MDI, "");

	g_return_if_fail (object != NULL);
	
   	mdi = GEDIT_MDI (object);

	g_return_if_fail (GEDIT_IS_MDI (mdi));
	g_return_if_fail (mdi->priv != NULL);

	g_free (mdi->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}


/**
 * gedit_mdi_new:
 * 
 * Creates a new #GeditMDI object.
 *
 * Return value: a new #GeditMDI
 **/
GeditMDI*
gedit_mdi_new (void)
{
	GeditMDI *mdi;

	gedit_debug (DEBUG_MDI, "");

	mdi = GEDIT_MDI (g_object_new (GEDIT_TYPE_MDI, NULL));
  	g_return_val_if_fail (mdi != NULL, NULL);
	
	return mdi;
}

/* Computes a unique ID string for use as the window role */
/* Taken from EOG */

static char *
gen_role (void)
{
        char *ret;
	static char *hostname;
	time_t t;
	static int serial;

	t = time (NULL);

	if (!hostname) {
		static char buffer [512];

		if ((gethostname (buffer, sizeof (buffer) - 1) == 0) &&
		    (buffer [0] != 0))
			hostname = buffer;
		else
			hostname = "localhost";
	}

	ret = g_strdup_printf ("gedit-window-%d-%d-%d-%ld-%d@%s",
			       getpid (),
			       getgid (),
			       getppid (),
			       (long) t,
			       serial++,
			       hostname);

	return ret;
}

static void
gedit_mdi_app_created_handler (BonoboMDI *mdi, BonoboWindow *win)
{
	GtkWidget *widget;
	BonoboControl *control;
	BonoboUIComponent *ui_component;
	GnomeRecentView *view;
	GnomeRecentModel *model;
	gchar *role;
	GeditWindowPrefs *prefs;

	static GtkTargetEntry drag_types[] =
	{
		{ "text/uri-list", 0, 0 },
	};

	static gint n_drag_types = sizeof (drag_types) / sizeof (drag_types [0]);

	gedit_debug (DEBUG_MDI, "");
	
	ui_component = bonobo_mdi_get_ui_component_from_window (win);
	g_return_if_fail (ui_component != NULL);
	
	/* Drag and drop support */
	gtk_drag_dest_set (GTK_WIDGET (win),
			   GTK_DEST_DEFAULT_MOTION |
			   GTK_DEST_DEFAULT_HIGHLIGHT |
			   GTK_DEST_DEFAULT_DROP,
			   drag_types, n_drag_types,
			   GDK_ACTION_COPY);
		
	g_signal_connect (G_OBJECT (win), "drag_data_received",
			  G_CALLBACK (gedit_mdi_drag_data_received_handler), 
			  NULL);
	
	/* Add cursor position status bar */
	widget = gtk_statusbar_new ();
	control = bonobo_control_new (widget);
	
	gtk_widget_set_size_request (widget, 150, 10);	
	gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (widget), FALSE);
	
	bonobo_ui_component_object_set (ui_component,
		       			"/status/CursorPosition",
					BONOBO_OBJREF (control),
					NULL);

	bonobo_object_unref (BONOBO_OBJECT (control));

	g_object_set_data (G_OBJECT (win), "CursorPosition", widget);

	/* Add overwrite mode status bar */
	widget = gtk_statusbar_new ();
	control = bonobo_control_new (widget);
	
	gtk_widget_set_size_request (widget, 80, 10);
	
	bonobo_ui_component_object_set (ui_component,
		       			"/status/OverwriteMode",
					BONOBO_OBJREF (control),
					NULL);

	bonobo_object_unref (BONOBO_OBJECT (control));

	g_object_set_data (G_OBJECT (win), "OverwriteMode", widget);

	prefs = gedit_window_prefs_new ();
	gedit_window_prefs_attach_to_window (prefs, win);

	/* Set the statusbar style according to prefs */
	gedit_mdi_set_app_statusbar_style (win);
	
	/* Set the toolbar style according to prefs */
	gedit_mdi_set_app_toolbar_style (win);
		
	/* Add listener fo the view menu */
	bonobo_ui_component_add_listener (ui_component, "ViewToolbar", 
			(BonoboUIListenerFn)gedit_mdi_view_menu_item_toggled_handler, 
			(gpointer)win);
	bonobo_ui_component_add_listener (ui_component, "ViewStatusbar", 
			(BonoboUIListenerFn)gedit_mdi_view_menu_item_toggled_handler, 
			(gpointer)win);

	bonobo_ui_component_add_listener (ui_component, "ToolbarSystem", 
			(BonoboUIListenerFn)gedit_mdi_view_menu_item_toggled_handler, 
			(gpointer)win);
	bonobo_ui_component_add_listener (ui_component, "ToolbarIcon", 
			(BonoboUIListenerFn)gedit_mdi_view_menu_item_toggled_handler, 
			(gpointer)win);
	bonobo_ui_component_add_listener (ui_component, "ToolbarIconText", 
			(BonoboUIListenerFn)gedit_mdi_view_menu_item_toggled_handler, 
			(gpointer)win);

	bonobo_ui_component_add_listener (ui_component, "StatusBarCursorPosition", 
			(BonoboUIListenerFn)gedit_mdi_view_menu_item_toggled_handler, 
			(gpointer)win);
	bonobo_ui_component_add_listener (ui_component, "StatusBarOverwriteMode", 
			(BonoboUIListenerFn)gedit_mdi_view_menu_item_toggled_handler, 
			(gpointer)win);
	
	/* add a GeditRecentView object */
	model = gedit_recent_get_model ();
	view = GNOME_RECENT_VIEW (gnome_recent_view_bonobo_new (ui_component, "/menu/File/Recents"));
	gnome_recent_view_set_model (view, model);
	
	g_signal_connect (G_OBJECT (view), "activate",
			  G_CALLBACK (gedit_file_open_recent), NULL);
	
	g_object_set_data (G_OBJECT (win), RECENT_KEY, view);

	/* FIXME: Set the window size. */
	gtk_window_set_default_size (GTK_WINDOW (win), 
			600, /*gedit_settings->window_width*/
			400 /*gedit_settings->window_height*/);

	/* FIXME
	g_signal_connect (G_OBJECT (win), "configure_event",
	                  G_CALLBACK (gedit_prefs_configure_event_handler), 
			  NULL);
	*/
	
	/* Add the plugins menus */
	gedit_plugins_engine_update_plugins_ui (win, TRUE);

	/* Set window role */
	role = gen_role ();
	gtk_window_set_role (GTK_WINDOW (win), role);
	g_free (role);
}

static void
gedit_mdi_view_menu_item_toggled_handler (
			BonoboUIComponent           *ui_component,
			const char                  *path,
			Bonobo_UIComponent_EventType type,
			const char                  *state,
			BonoboWindow                *win)
{
	gboolean s;
	GeditWindowPrefs *prefs;

	gedit_debug (DEBUG_MDI, "%s toggled to '%s'", path, state);

	prefs = gedit_window_prefs_get_from_window (win);
	g_return_if_fail (prefs != NULL);

	s = (strcmp (state, "1") == 0);

	if (strcmp (path, "ViewToolbar") == 0)
	{
		if (s != prefs->toolbar_visible)
		{
			prefs->toolbar_visible = s;
			gedit_mdi_set_app_toolbar_style (win);
		}

		goto save_prefs;
	}

	if (strcmp (path, "ViewStatusbar") == 0)
	{
		if (s != prefs->statusbar_visible)
		{
			prefs->statusbar_visible = s;
			gedit_mdi_set_app_statusbar_style (win);
		}

		goto save_prefs;
	}

	if (s && (strcmp (path, "ToolbarSystem") == 0))
	{
		if (prefs->toolbar_buttons_style  != GEDIT_TOOLBAR_SYSTEM)
		{
			prefs->toolbar_buttons_style  = GEDIT_TOOLBAR_SYSTEM;
			gedit_mdi_set_app_toolbar_style (win);
		}

		goto save_prefs;
	}

	if (s && (strcmp (path, "ToolbarIcon") == 0))
	{
		if (prefs->toolbar_buttons_style != GEDIT_TOOLBAR_ICONS)
		{
			prefs->toolbar_buttons_style = GEDIT_TOOLBAR_ICONS;
			gedit_mdi_set_app_toolbar_style (win);
		}
		
		goto save_prefs;
	}

	if (s && (strcmp (path, "ToolbarIconText") == 0))
	{
		if (prefs->toolbar_buttons_style != GEDIT_TOOLBAR_ICONS_AND_TEXT)
		{
			prefs->toolbar_buttons_style = GEDIT_TOOLBAR_ICONS_AND_TEXT;
			gedit_mdi_set_app_toolbar_style (win);
		}

		goto save_prefs;
	}

	if (strcmp (path, "StatusBarCursorPosition") == 0)
	{
		if (s != prefs->statusbar_show_cursor_position)
		{
			prefs->statusbar_show_cursor_position = s;
			gedit_mdi_set_app_statusbar_style (win);
		}

		goto save_prefs;
	}

	if (strcmp (path, "StatusBarOverwriteMode") == 0)
	{
		if (s != prefs->statusbar_show_overwrite_mode)
		{
			prefs->statusbar_show_overwrite_mode = s;
			gedit_mdi_set_app_statusbar_style (win);
		}

		goto save_prefs;
	}

save_prefs:
	gedit_window_prefs_save (prefs);
}

static void 
gedit_mdi_drag_data_received_handler (GtkWidget *widget, GdkDragContext *context, 
		                      gint x, gint y, GtkSelectionData *selection_data, 
				      guint info, guint time)
{
	GList *list = NULL;
	GList *file_list = NULL;
	GList *p = NULL;
	
	gedit_debug (DEBUG_MDI, "");

	list = gnome_vfs_uri_list_parse (selection_data->data);
	p = list;

	while (p != NULL)
	{
		file_list = g_list_append (file_list, 
				gnome_vfs_uri_to_string ((const GnomeVFSURI*)(p->data), 
				GNOME_VFS_URI_HIDE_NONE));
		p = p->next;
	}
	
	gnome_vfs_uri_list_free (list);

	gedit_file_open_uri_list (file_list, 0);	
	
	if (file_list == NULL)
		return;

	for (p = file_list; p != NULL; p = p->next) {
		g_free (p->data);
	}
	
	g_list_free (file_list);
}

static void
gedit_mdi_set_app_toolbar_style (BonoboWindow *win)
{
	BonoboUIComponent *ui_component;
	GeditWindowPrefs *prefs = NULL;
	
	gedit_debug (DEBUG_MDI, "");
	
	g_return_if_fail (BONOBO_IS_WINDOW (win));
	
	prefs = gedit_window_prefs_get_from_window (win);
	g_return_if_fail (prefs != NULL);
	
	ui_component = bonobo_mdi_get_ui_component_from_window (win);
	g_return_if_fail (ui_component != NULL);
			
	bonobo_ui_component_freeze (ui_component, NULL);

	/* Updated view menu */
	gedit_menus_set_verb_state (ui_component, 
				    "/commands/ViewToolbar",
				    prefs->toolbar_visible);

	gedit_menus_set_verb_sensitive (ui_component, 
				        "/commands/ToolbarSystem",
				        prefs->toolbar_visible);
	gedit_menus_set_verb_sensitive (ui_component, 
				        "/commands/ToolbarIcon",
				        prefs->toolbar_visible);
	gedit_menus_set_verb_sensitive (ui_component, 
				        "/commands/ToolbarIconText",
				        prefs->toolbar_visible);
	gedit_menus_set_verb_sensitive (ui_component, 
				        "/commands/ToolbarTooltips",
				        prefs->toolbar_visible);

	gedit_menus_set_verb_state (ui_component, 
				    "/commands/ToolbarSystem",
				    prefs->toolbar_buttons_style == GEDIT_TOOLBAR_SYSTEM);

	gedit_menus_set_verb_state (ui_component, 
				    "/commands/ToolbarIcon",
				    prefs->toolbar_buttons_style == GEDIT_TOOLBAR_ICONS);

	gedit_menus_set_verb_state (ui_component, 
				    "/commands/ToolbarIconText",
				    prefs->toolbar_buttons_style == GEDIT_TOOLBAR_ICONS_AND_TEXT);

	switch (prefs->toolbar_buttons_style)
	{
		case GEDIT_TOOLBAR_SYSTEM:
			gedit_debug (DEBUG_MDI, "GEDIT: SYSTEM");
			bonobo_ui_component_set_prop (
				ui_component, "/Toolbar", "look", "system", NULL);

			break;
			
		case GEDIT_TOOLBAR_ICONS:
			gedit_debug (DEBUG_MDI, "GEDIT: ICONS");
			bonobo_ui_component_set_prop (
				ui_component, "/Toolbar", "look", "icon", NULL);
			
			break;
			
		case GEDIT_TOOLBAR_ICONS_AND_TEXT:
			gedit_debug (DEBUG_MDI, "GEDIT: ICONS_AND_TEXT");
			bonobo_ui_component_set_prop (
				ui_component, "/Toolbar", "look", "both", NULL);
			
			break;
		default:
			goto error;
			break;
	}
	
	bonobo_ui_component_set_prop (
			ui_component, "/Toolbar",
			"hidden", prefs->toolbar_visible ? "0":"1", NULL);

 error:
	bonobo_ui_component_thaw (ui_component, NULL);
}

static void
gedit_mdi_set_app_statusbar_style (BonoboWindow *win)
{
	GeditWindowPrefs *prefs = NULL;
	BonoboUIComponent *ui_component;
	GtkWidget *cp, *om;
	
	gedit_debug (DEBUG_MDI, "");
	
	g_return_if_fail (BONOBO_IS_WINDOW (win));

	prefs = gedit_window_prefs_get_from_window (win);
	g_return_if_fail (prefs != NULL);

	ui_component = bonobo_mdi_get_ui_component_from_window (win);
	g_return_if_fail (ui_component != NULL);

	bonobo_ui_component_freeze (ui_component, NULL);
	
	/* Update menu */
	gedit_menus_set_verb_state (ui_component, 
				    "/commands/ViewStatusbar",
				    prefs->statusbar_visible);

	gedit_menus_set_verb_sensitive (ui_component, 
				    "/commands/StatusBarCursorPosition",
				    prefs->statusbar_visible);

	gedit_menus_set_verb_sensitive (ui_component, 
				    "/commands/StatusBarOverwriteMode",
				    prefs->statusbar_visible);

	gedit_menus_set_verb_state (ui_component, 
				    "/commands/StatusBarCursorPosition",
				    prefs->statusbar_show_cursor_position);

	gedit_menus_set_verb_state (ui_component, 
				    "/commands/StatusBarOverwriteMode",
				    prefs->statusbar_show_overwrite_mode);
	
	/* Actually update status bar style */
	bonobo_ui_component_set_prop (
		ui_component, "/status",
		"hidden", prefs->statusbar_visible ? "0" : "1",
		NULL);

	cp = GTK_WIDGET (g_object_get_data (G_OBJECT (win), "CursorPosition"));
	if (cp == NULL)
		goto error;

	if (prefs->statusbar_show_cursor_position)
	{
		bonobo_ui_component_set_prop (
			ui_component, "/status/CursorPosition", "hidden", "0", NULL);

		gtk_widget_show (cp);
	}
	else
	{
		bonobo_ui_component_set_prop (
			ui_component, "/status/CursorPosition", "hidden", "1", NULL);

		gtk_widget_hide (cp);
	}
	
	om = GTK_WIDGET (g_object_get_data (G_OBJECT (win), "OverwriteMode"));
	if (om == NULL)
		goto error;

	if (prefs->statusbar_show_overwrite_mode)
	{
		gtk_widget_show (om);
		
		bonobo_ui_component_set_prop (
			ui_component, "/status/OverwriteMode", "hidden", "0", NULL);
	}
	else
	{
		bonobo_ui_component_set_prop (
			ui_component, "/status/OverwriteMode", "hidden", "1", NULL);
		
		gtk_widget_hide (om);
	}

	if (!prefs->statusbar_show_overwrite_mode)
		gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (cp), TRUE);
	else
		gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (cp), FALSE);

	if (!prefs->statusbar_show_cursor_position &&
	    !prefs->statusbar_show_overwrite_mode)
		bonobo_ui_component_set_prop (
			ui_component, "/status", "resize_grip", "1", NULL);
	else
		bonobo_ui_component_set_prop (
			ui_component, "/status", "resize_grip", "0", NULL);

error:
	bonobo_ui_component_thaw (ui_component, NULL);
	
}

static void 
gedit_mdi_child_state_changed_handler (GeditMDIChild *child)
{
	gedit_debug (DEBUG_MDI, "");

	if (bonobo_mdi_get_active_child (BONOBO_MDI (gedit_mdi)) != BONOBO_MDI_CHILD (child))
		return;
	
	gedit_mdi_set_active_window_title (BONOBO_MDI (gedit_mdi));
	gedit_mdi_set_active_window_verbs_sensitivity (BONOBO_MDI (gedit_mdi));
}

static void 
gedit_mdi_child_undo_redo_state_changed_handler (GeditMDIChild *child)
{
	gedit_debug (DEBUG_MDI, "");

	if (bonobo_mdi_get_active_child (BONOBO_MDI (gedit_mdi)) != BONOBO_MDI_CHILD (child))
		return;
	
	gedit_mdi_set_active_window_undo_redo_verbs_sensitivity (BONOBO_MDI (gedit_mdi));
}

static gint 
gedit_mdi_add_child_handler (BonoboMDI *mdi, BonoboMDIChild *child)
{
	gedit_debug (DEBUG_MDI, "");

	g_signal_connect (G_OBJECT (child), "state_changed",
			  G_CALLBACK (gedit_mdi_child_state_changed_handler), 
			  NULL);
	g_signal_connect (G_OBJECT (child), "undo_redo_state_changed",
			  G_CALLBACK (gedit_mdi_child_undo_redo_state_changed_handler), 
			  NULL);

	return TRUE;
}

static gint 
gedit_mdi_add_view_handler (BonoboMDI *mdi, GtkWidget *view)
{
	static GtkTargetEntry drag_types[] =
	{
		{ "text/uri-list", 0, 0 },
	};

	static gint n_drag_types = sizeof (drag_types) / sizeof (drag_types [0]);

	gedit_debug (DEBUG_MDI, "");
	
	/* FIXME */
	/* Drag and drop support */
	gtk_drag_dest_set (view,
			   GTK_DEST_DEFAULT_MOTION |
			   GTK_DEST_DEFAULT_HIGHLIGHT |
			   GTK_DEST_DEFAULT_DROP,
			   drag_types, n_drag_types,
			   GDK_ACTION_COPY);
		
	g_signal_connect (G_OBJECT (view), "drag_data_received",
			  G_CALLBACK (gedit_mdi_drag_data_received_handler), 
			  NULL);

	return TRUE;
}

static gint 
gedit_mdi_remove_child_handler (BonoboMDI *mdi, BonoboMDIChild *child)
{
	GeditDocument* doc;
	gboolean close = TRUE;
	
	gedit_debug (DEBUG_MDI, "");

	g_return_val_if_fail (child != NULL, FALSE);
	g_return_val_if_fail (GEDIT_MDI_CHILD (child)->document != NULL, FALSE);

	doc = GEDIT_MDI_CHILD (child)->document;

	if (gedit_document_get_modified (doc))
	{
		GtkWidget *msgbox, *w;
		gchar *fname = NULL, *msg = NULL;
		gint ret;
		gboolean exiting;

		w = GTK_WIDGET (g_list_nth_data (bonobo_mdi_child_get_views (child), 0));
			
		if(w != NULL)
			bonobo_mdi_set_active_view (mdi, w);

		fname = gedit_document_get_short_name (doc);

		msgbox = gtk_message_dialog_new (GTK_WINDOW (bonobo_mdi_get_active_window (mdi)),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_QUESTION,
				GTK_BUTTONS_NONE,
				_("Do you want to save the changes you made to the document \"%s\"? \n\n"
				  "Your changes will be lost if you don't save them."),
				fname);

		gedit_dialog_add_button (GTK_DIALOG (msgbox),
				_("Do_n't save"), GTK_STOCK_NO,
				GTK_RESPONSE_NO);

		if (gedit_close_x_button_pressed)
			exiting = FALSE;
		else if (gedit_exit_button_pressed)
			exiting = TRUE;
		else
		{
			/* Delete event generated */
			if (g_list_length (bonobo_mdi_get_windows (BONOBO_MDI (gedit_mdi))) == 1)
				exiting = TRUE;
			else
				exiting = FALSE;
		}

#if 0		
		if (exiting)
			gedit_dialog_add_button (GTK_DIALOG (msgbox),
					_("_Don't quit"), GTK_STOCK_CANCEL,
                	             	GTK_RESPONSE_CANCEL);
		else
			gedit_dialog_add_button (GTK_DIALOG (msgbox),
					_("_Don't close"), GTK_STOCK_CANCEL,
                	             	GTK_RESPONSE_CANCEL);
#endif
		
		gtk_dialog_add_button (GTK_DIALOG (msgbox), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

		gtk_dialog_add_button (GTK_DIALOG (msgbox),
			       	GTK_STOCK_SAVE,
				GTK_RESPONSE_YES);

		gtk_dialog_set_default_response	(GTK_DIALOG (msgbox), GTK_RESPONSE_YES);

		gtk_window_set_resizable (GTK_WINDOW (msgbox), FALSE);

		gtk_widget_show (msgbox);
		gtk_window_present (GTK_WINDOW (msgbox));

		ret = gtk_dialog_run (GTK_DIALOG (msgbox));
		
		gtk_widget_destroy (msgbox);

		g_free (fname);
		g_free (msg);
		
		switch (ret)
		{
			case GTK_RESPONSE_YES:
				close = gedit_file_save (GEDIT_MDI_CHILD (child));
				break;
			case GTK_RESPONSE_NO:
				close = TRUE;
				break;
			default:
				close = FALSE;
		}

		gedit_debug (DEBUG_MDI, "CLOSE: %s", close ? "TRUE" : "FALSE");
	}
	
	/* FIXME: there is a bug if you "close all" >1 docs, don't save the document
	 * and then don't close the last one.
	 */
	/* Disable to avoid the bug */
	/*
	if (close)
	{
		g_signal_handlers_disconnect_by_func (child, 
						      G_CALLBACK (gedit_mdi_child_state_changed_handler),
						      NULL);
		g_signal_handlers_disconnect_by_func (GTK_OBJECT (child), 
						      G_CALLBACK (gedit_mdi_child_undo_redo_state_changed_handler),
						      NULL);
	}
	*/
	
	return close;
}

static gint 
gedit_mdi_remove_view_handler (BonoboMDI *mdi,  GtkWidget *view)
{
	gedit_debug (DEBUG_MDI, "");

	return TRUE;
}

void 
gedit_mdi_set_active_window_title (BonoboMDI *mdi)
{
	BonoboMDIChild* active_child = NULL;
	GeditDocument* doc = NULL;
	gchar* docname = NULL;
	gchar* title = NULL;
	
	gedit_debug (DEBUG_MDI, "");

	
	active_child = bonobo_mdi_get_active_child (mdi);
	if (active_child == NULL)
		return;

	doc = GEDIT_MDI_CHILD (active_child)->document;
	g_return_if_fail (doc != NULL);
	
	/* Set active window title */
	docname = gedit_document_get_uri (doc);
	g_return_if_fail (docname != NULL);

	if (gedit_document_get_modified (doc))
	{
		title = g_strdup_printf ("%s %s - gedit", docname, _("(modified)"));
	} 
	else 
	{
		if (gedit_document_is_readonly (doc)) 
		{
			title = g_strdup_printf ("%s %s - gedit", docname, _("(readonly)"));
		} 
		else 
		{
			title = g_strdup_printf ("%s - gedit", docname);
		}

	}

	gtk_window_set_title (GTK_WINDOW (bonobo_mdi_get_active_window (mdi)), title);
	
	g_free (docname);
	g_free (title);
}

static 
void gedit_mdi_child_changed_handler (BonoboMDI *mdi, BonoboMDIChild *old_child)
{
	gedit_debug (DEBUG_MDI, "");

	gedit_mdi_set_active_window_title (mdi);	
}

static 
void gedit_mdi_view_changed_handler (BonoboMDI *mdi, GtkWidget *old_view)
{
	BonoboWindow *win;
	GtkWidget *status;
	GtkWidget *active_view;
	
	gedit_debug (DEBUG_MDI, "");

	gedit_mdi_set_active_window_verbs_sensitivity (mdi);

	active_view = bonobo_mdi_get_active_view (mdi);
		
	win = bonobo_mdi_get_active_window (mdi);
	g_return_if_fail (win != NULL);

	if (old_view != NULL)
	{
		gedit_view_set_cursor_position_statusbar (GEDIT_VIEW (old_view), NULL);
		gedit_view_set_overwrite_mode_statusbar (GEDIT_VIEW (old_view), NULL);
	}

	if (active_view == NULL)
		return;

	/*
	gtk_widget_grab_focus (active_view);
	*/

	status = g_object_get_data (G_OBJECT (win), "CursorPosition");	
	gedit_view_set_cursor_position_statusbar (GEDIT_VIEW (active_view), status);

	status = g_object_get_data (G_OBJECT (win), "OverwriteMode");	
	gedit_view_set_overwrite_mode_statusbar (GEDIT_VIEW (active_view), status);
}

void 
gedit_mdi_clear_active_window_statusbar (GeditMDI *mdi)
{
	gpointer status;
	BonoboWindow *win;

	win = bonobo_mdi_get_active_window (BONOBO_MDI (mdi));
	if (win == NULL)
		return;

	status = g_object_get_data (G_OBJECT (win), "CursorPosition");	
	g_return_if_fail (status != NULL);
	g_return_if_fail (GTK_IS_STATUSBAR (status));

	/* clear any previous message, underflow is allowed */
	gtk_statusbar_pop (GTK_STATUSBAR (status), 0); 

	status = g_object_get_data (G_OBJECT (win), "OverwriteMode");	
	g_return_if_fail (status != NULL);
	g_return_if_fail (GTK_IS_STATUSBAR (status));

	/* clear any previous message, underflow is allowed */
	gtk_statusbar_pop (GTK_STATUSBAR (status), 0); 
}

void 
gedit_mdi_set_active_window_verbs_sensitivity (BonoboMDI *mdi)
{
	/* FIXME: it is too slooooooow! - Paolo */

	BonoboWindow* active_window = NULL;
	BonoboMDIChild* active_child = NULL;
	GeditDocument* doc = NULL;
	BonoboUIComponent *ui_component;
	
	gedit_debug (DEBUG_MDI, "");
	
	active_window = bonobo_mdi_get_active_window (mdi);

	if (active_window == NULL)
		return;
	
	ui_component = bonobo_mdi_get_ui_component_from_window (active_window);
	g_return_if_fail (ui_component != NULL);
	
	active_child = bonobo_mdi_get_active_child (mdi);
	
	bonobo_ui_component_freeze (ui_component, NULL);
	
	gedit_plugins_engine_update_plugins_ui (active_window, FALSE);

	if (active_child == NULL)
	{
		gedit_menus_set_verb_list_sensitive (ui_component, 
				gedit_menus_no_docs_sensible_verbs, FALSE);
		goto end;
	}
	else
	{
		gedit_menus_set_verb_list_sensitive (ui_component, 
				gedit_menus_all_sensible_verbs, TRUE);
	}

	doc = GEDIT_MDI_CHILD (active_child)->document;
	g_return_if_fail (doc != NULL);
	
	if (gedit_document_is_readonly (doc))
	{
		gedit_menus_set_verb_list_sensitive (ui_component, 
				gedit_menus_ro_sensible_verbs, FALSE);
		goto end;
	}

	if (!gedit_document_can_undo (doc))
		gedit_menus_set_verb_sensitive (ui_component, "/commands/EditUndo", FALSE);	

	if (!gedit_document_can_redo (doc))
		gedit_menus_set_verb_sensitive (ui_component, "/commands/EditRedo", FALSE);		

	if (!gedit_document_get_modified (doc))
	{
		gedit_menus_set_verb_list_sensitive (ui_component, 
				gedit_menus_not_modified_doc_sensible_verbs, FALSE);
		goto end;
	}

	if (gedit_document_is_untitled (doc))
	{
		gedit_menus_set_verb_list_sensitive (ui_component, 
				gedit_menus_untitled_doc_sensible_verbs, FALSE);
	}

end:
	bonobo_ui_component_thaw (ui_component, NULL);
}


static void 
gedit_mdi_set_active_window_undo_redo_verbs_sensitivity (BonoboMDI *mdi)
{
	BonoboWindow* active_window = NULL;
	BonoboMDIChild* active_child = NULL;
	GeditDocument* doc = NULL;
	BonoboUIComponent *ui_component;
	
	gedit_debug (DEBUG_MDI, "");
	
	active_window = bonobo_mdi_get_active_window (mdi);
	g_return_if_fail (active_window != NULL);
	
	ui_component = bonobo_mdi_get_ui_component_from_window (active_window);
	g_return_if_fail (ui_component != NULL);
	
	active_child = bonobo_mdi_get_active_child (mdi);
	doc = GEDIT_MDI_CHILD (active_child)->document;
	g_return_if_fail (doc != NULL);

	bonobo_ui_component_freeze (ui_component, NULL);

	gedit_menus_set_verb_sensitive (ui_component, "/commands/EditUndo", 
			gedit_document_can_undo (doc));	

	gedit_menus_set_verb_sensitive (ui_component, "/commands/EditRedo", 
			gedit_document_can_redo (doc));	

	bonobo_ui_component_thaw (ui_component, NULL);
}

GnomeRecentView *
gedit_mdi_get_recent_view_from_window (BonoboWindow* win)
{
	gpointer r;
	gedit_debug (DEBUG_MDI, "");

	r = g_object_get_data (G_OBJECT (win), RECENT_KEY);
	
	return (r != NULL) ? GNOME_RECENT_VIEW (r) : NULL;
}


static GeditWindowPrefs *
gedit_window_prefs_new (void)
{
	GeditWindowPrefs *prefs;

	gedit_debug (DEBUG_MDI, "");

	prefs = g_new0 (GeditWindowPrefs, 1);

	prefs->toolbar_visible = gedit_prefs_manager_get_toolbar_visible ();
	prefs->toolbar_buttons_style = gedit_prefs_manager_get_toolbar_buttons_style ();

	prefs->statusbar_visible = gedit_prefs_manager_get_statusbar_visible ();
	prefs->statusbar_show_cursor_position = gedit_prefs_manager_get_statusbar_show_cursor_position ();
	prefs->statusbar_show_overwrite_mode = gedit_prefs_manager_get_statusbar_show_overwrite_mode ();

	return prefs;
}

static void
gedit_window_prefs_attach_to_window (GeditWindowPrefs *prefs, BonoboWindow *win)
{
	gedit_debug (DEBUG_MDI, "");

	g_return_if_fail (prefs != NULL);
	g_return_if_fail (win != NULL);
	g_return_if_fail (BONOBO_IS_WINDOW (win));

	if (!window_prefs_id)
		window_prefs_id = g_quark_from_static_string ("GeditWindowPrefsData");

	g_object_set_qdata_full (G_OBJECT (win), window_prefs_id, prefs, g_free);
}

static GeditWindowPrefs	*
gedit_window_prefs_get_from_window (BonoboWindow *win)
{
	GeditWindowPrefs *prefs;

	gedit_debug (DEBUG_MDI, "");

	g_return_val_if_fail (win != NULL, NULL);
	g_return_val_if_fail (BONOBO_IS_WINDOW (win), NULL);

	prefs = g_object_get_qdata (G_OBJECT (win), window_prefs_id);

	return (prefs != NULL) ? (GeditWindowPrefs*)prefs : NULL;
}

static void
gedit_window_prefs_save (GeditWindowPrefs *prefs)
{
	gedit_debug (DEBUG_MDI, "");

	g_return_if_fail (prefs != NULL);

	if (prefs->toolbar_visible != gedit_prefs_manager_get_toolbar_visible ())
		gedit_prefs_manager_set_toolbar_visible (prefs->toolbar_visible);

	if (prefs->toolbar_buttons_style != gedit_prefs_manager_get_toolbar_buttons_style ())
		gedit_prefs_manager_set_toolbar_buttons_style (prefs->toolbar_buttons_style);

	if (prefs->statusbar_visible != gedit_prefs_manager_get_statusbar_visible ())
		gedit_prefs_manager_set_statusbar_visible (prefs->statusbar_visible);

	if (prefs->statusbar_show_cursor_position != 
			gedit_prefs_manager_get_statusbar_show_cursor_position ())
		gedit_prefs_manager_set_statusbar_show_cursor_position (
				prefs->statusbar_show_cursor_position);

	if (prefs->statusbar_show_overwrite_mode != 
			gedit_prefs_manager_get_statusbar_show_overwrite_mode ())
		gedit_prefs_manager_set_statusbar_show_overwrite_mode (
				prefs->statusbar_show_overwrite_mode);
}
