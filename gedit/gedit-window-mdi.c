/*
 * gedit-window-mdi.c
 * This file is part of gedit
 *
 * Copyright (C) 2005 - Paolo Maggi 
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
 * Modified by the gedit Team, 2005. See the AUTHORS file for a 
 * list of people on the gedit Team.  
 * See the ChangeLog files for a list of changes. 
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <time.h>
#include <sys/types.h>
#include <string.h>

#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "gedit-ui.h"
#include "gedit-window-mdi.h"
#include "gedit-notebook.h"
#include "gedit-commands.h"
#include "gedit-debug.h"
#include "gedit-utils.h"
#include "gedit-app.h"
#include "gedit-documents-panel.h"

struct _GeditWindowMdiPrivate
{
	GtkActionGroup *action_group;
	GtkActionGroup *quit_action_group;
	GtkActionGroup *documents_list_action_group;
	guint           documents_list_menu_ui_id;
	GtkWidget      *notebook;
	gint            num_tabs;
};

#define GEDIT_WINDOW_MDI_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object),\
					 GEDIT_TYPE_WINDOW_MDI,                    \
					 GeditWindowMdiPrivate))

static GeditTab *gedit_window_mdi_create_tab          (GeditWindow         *window,
						       gboolean             jump_to);
static GeditTab *gedit_window_mdi_create_tab_from_uri (GeditWindow         *window,
					 	       const gchar         *uri,
						       const GeditEncoding *encoding,
						       gint                 line_pos,
						       gboolean             create,
						       gboolean             jump_to);
static void	 gedit_window_mdi_close_tab           (GeditWindow         *window,
						       GeditTab            *tab);
static void	 gedit_window_mdi_set_active_tab      (GeditWindow         *window,
						       GeditTab            *tab);
static void	 gedit_window_mdi_update_sensitivity  (GeditWindow         *window);
						  
G_DEFINE_TYPE(GeditWindowMdi, gedit_window_mdi, GEDIT_TYPE_WINDOW)

static void
gedit_window_mdi_finalize (GObject *object)
{
	//GeditWindowMdi *window = GEDIT_WINDOW_MDI (object); 
	G_OBJECT_CLASS (gedit_window_mdi_parent_class)->finalize (object);
}

static void
gedit_window_mdi_class_init (GeditWindowMdiClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GeditWindowClass *window_class = GEDIT_WINDOW_CLASS (klass);
	
	object_class->finalize = gedit_window_mdi_finalize;

	window_class->create_tab = gedit_window_mdi_create_tab;
	window_class->create_tab_from_uri = gedit_window_mdi_create_tab_from_uri;
	window_class->close_tab = gedit_window_mdi_close_tab;
	window_class->set_active_tab = gedit_window_mdi_set_active_tab;
	window_class->update_sensitivity = gedit_window_mdi_update_sensitivity;

	g_type_class_add_private (object_class, sizeof(GeditWindowMdiPrivate));
}

static void
merge_menu (GeditWindowMdi *window)
{
	GtkActionGroup *action_group;
	GtkUIManager *manager;
	GError *error = NULL;

	manager = gedit_window_get_ui_manager (GEDIT_WINDOW (window));
	

	action_group = gtk_action_group_new ("DocumentsMenu");
	gtk_action_group_set_translation_domain (action_group, NULL);
	gtk_action_group_add_actions (action_group,
				      gedit_documents_menu_entries,
				      G_N_ELEMENTS (gedit_documents_menu_entries),
				      window);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);
	g_object_unref (action_group);
	window->priv->action_group = action_group;

	action_group = gtk_action_group_new ("GeditQuitWindowActions");
	gtk_action_group_set_translation_domain (action_group, NULL);
	gtk_action_group_add_actions (action_group,
				      gedit_quit_menu_entries,
				      G_N_ELEMENTS (gedit_quit_menu_entries),
				      window);

	gtk_ui_manager_insert_action_group (manager, action_group, 0);
	g_object_unref (action_group);
	window->priv->quit_action_group = action_group;

	/* list of open documents menu */
	action_group = gtk_action_group_new ("DocumentsListActions");
	gtk_action_group_set_translation_domain (action_group, NULL);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);
	g_object_unref (action_group);
	window->priv->documents_list_action_group = action_group;

	gtk_ui_manager_add_ui_from_file (manager, GEDIT_UI_DIR "gedit-ui-mdi.xml", &error);

	if (error != NULL)
	{
		g_warning ("Could not merge gedit-ui-mdi.xml: %s", error->message);
		g_error_free (error);
	}
}

static void
documents_list_menu_activate (GtkToggleAction *action,
			      GeditWindowMdi     *window)
{
	gint n;

	if (gtk_toggle_action_get_active (action) == FALSE)
		return;

	n = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (action));
	gtk_notebook_set_current_page (GTK_NOTEBOOK (window->priv->notebook), n);
}

static void
update_documents_list_menu (GeditWindowMdi *window)
{
	GeditWindowMdiPrivate *p = window->priv;
	GList *actions, *l;
	gint n, i;
	guint id;
	GSList *group = NULL;
	GtkUIManager *manager;
	
	gedit_debug (DEBUG_WINDOW);

	g_return_if_fail (p->documents_list_action_group != NULL);
	
	manager = gedit_window_get_ui_manager (GEDIT_WINDOW (window));

	if (p->documents_list_menu_ui_id != 0)
		gtk_ui_manager_remove_ui (manager,
					  p->documents_list_menu_ui_id);

	actions = gtk_action_group_list_actions (p->documents_list_action_group);
	for (l = actions; l != NULL; l = l->next)
	{
		g_signal_handlers_disconnect_by_func (GTK_ACTION (l->data),
						      G_CALLBACK (documents_list_menu_activate),
						      window);
 		gtk_action_group_remove_action (p->documents_list_action_group,
						GTK_ACTION (l->data));
	}
	g_list_free (actions);

	n = gtk_notebook_get_n_pages (GTK_NOTEBOOK (p->notebook));

	id = (n > 0) ? gtk_ui_manager_new_merge_id (manager) : 0;

	for (i = 0; i < n; i++)
	{
		GtkWidget *tab;
		GtkRadioAction *action;
		gchar *action_name;
		gchar *tab_name;
		gchar *name;
		gchar *tip;
		gchar *accel;

		tab = gtk_notebook_get_nth_page (GTK_NOTEBOOK (p->notebook), i);

		/* NOTE: the action is associated to the position of the tab in
		 * the notebook not to the tab itself! This is needed to work
		 * around the gtk+ bug #170727: gtk leaves around the accels
		 * of the action. Since the accel depends on the tab position
		 * the problem is worked around, action with the same name always
		 * get the same accel.
		 */
		action_name = g_strdup_printf ("Tab_%d", i);
		tab_name = _gedit_tab_get_name (GEDIT_TAB (tab));
		name = gedit_utils_escape_underscores (tab_name, -1);
		tip =  g_strdup_printf (_("Activate %s"), tab_name);

		/* alt + 1, 2, 3... 0 to switch to the first ten tabs */
		accel = (i < 10) ? g_strdup_printf ("<alt>%d", (i + 1) % 10) : NULL;

		action = gtk_radio_action_new (action_name,
					       name,
					       tip,
					       NULL,
					       i);

		if (group != NULL)
			gtk_radio_action_set_group (action, group);

		/* note that group changes each time we add an action, so it must be updated */
		group = gtk_radio_action_get_group (action);

		gtk_action_group_add_action_with_accel (p->documents_list_action_group,
							GTK_ACTION (action),
							accel);

		g_signal_connect (action,
				  "activate",
				  G_CALLBACK (documents_list_menu_activate),
				  window);

		gtk_ui_manager_add_ui (manager,
				       id,
				       "/MenuBar/DocumentsPlaceholder/DocumentsMenu/DocumentsListPlaceholder",
				       action_name, action_name,
				       GTK_UI_MANAGER_MENUITEM,
				       FALSE);

		if (GEDIT_TAB (tab) == gedit_window_get_active_tab (GEDIT_WINDOW (window)))
			gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

		g_object_unref (action);

		g_free (action_name);
		g_free (tab_name);
		g_free (name);
		g_free (tip);
		g_free (accel);
	}

	p->documents_list_menu_ui_id = id;
}

static GeditWindowMdi *
clone_window (GeditWindowMdi *origin)
{
/*	GtkWindow *window;
	GdkScreen *screen;
	GeditApp  *app;

	gedit_debug (DEBUG_WINDOW);	

	app = gedit_app_get_default ();

	screen = gtk_window_get_screen (GTK_WINDOW (origin));
	window = GTK_WINDOW (gedit_app_create_window (app, screen));

	gtk_window_set_default_size (window, 
				     origin->priv->width,
				     origin->priv->height);
				     
	if ((origin->priv->window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0)
	{
		gtk_window_set_default_size (window, 
					     gedit_prefs_manager_get_default_window_width (),
					     gedit_prefs_manager_get_default_window_height ());
					     
		gtk_window_maximize (window);
	}
	else
	{
		gtk_window_set_default_size (window, 
				     origin->priv->width,
				     origin->priv->height);

		gtk_window_unmaximize (window);
	}		

	if ((origin->priv->window_state & GDK_WINDOW_STATE_STICKY ) != 0)
		gtk_window_stick (window);
	else
		gtk_window_unstick (window);

	gtk_paned_set_position (GTK_PANED (GEDIT_WINDOW_MDI (window)->priv->hpaned),
				gtk_paned_get_position (GTK_PANED (origin->priv->hpaned)));

	gtk_paned_set_position (GTK_PANED (GEDIT_WINDOW_MDI (window)->priv->vpaned),
				gtk_paned_get_position (GTK_PANED (origin->priv->vpaned)));
				
	if (GTK_WIDGET_VISIBLE (origin->priv->side_panel))
		gtk_widget_show (GEDIT_WINDOW_MDI (window)->priv->side_panel);
	else
		gtk_widget_hide (GEDIT_WINDOW_MDI (window)->priv->side_panel);

	if (GTK_WIDGET_VISIBLE (origin->priv->bottom_panel))
		gtk_widget_show (GEDIT_WINDOW_MDI (window)->priv->bottom_panel);
	else
		gtk_widget_hide (GEDIT_WINDOW_MDI (window)->priv->bottom_panel);
		
	set_statusbar_style (GEDIT_WINDOW_MDI (window), origin);
	set_toolbar_style (GEDIT_WINDOW_MDI (window), origin);

	return GEDIT_WINDOW_MDI (window);*/
	
	return NULL;
}

static void 
notebook_switch_page (GtkNotebook     *book, 
		      GtkNotebookPage *pg,
		      gint             page_num, 
		      GeditWindowMdi     *window)
{
	GeditTab *tab;
	GtkAction *action;
	gchar *action_name;

	/* CHECK: I don't know why but it seems notebook_switch_page is called
	two times every time the user change the active tab */

	tab = GEDIT_TAB (gtk_notebook_get_nth_page (book, page_num));
	
	if (tab == gedit_window_get_active_tab (GEDIT_WINDOW (window)))
		return;

	/* set the active tab */
	gedit_window_set_active_tab (GEDIT_WINDOW (window), tab);

	/* activate the right item in the documents menu */
	action_name = g_strdup_printf ("Tab_%d", page_num);
	action = gtk_action_group_get_action (window->priv->documents_list_action_group,
					      action_name);

	/* sometimes the action doesn't exist yet, and the proper action
	 * is set active during the documents list menu creation
	 * CHECK: would it be nicer if active_tab was a property and we monitored the notify signal?
	 */
	if (action != NULL)
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

	g_free (action_name);
}

static void
gedit_window_mdi_update_sensitivity (GeditWindow *window)
{
	gint            state;
	GtkAction      *action;
	GeditWindowMdi *window_mdi;
	
	g_message("Update sensitivity!");
	
	window_mdi = GEDIT_WINDOW_MDI (window);
	
	state = gedit_window_get_state (window);
	
	gtk_action_group_set_sensitive (window_mdi->priv->quit_action_group,
				  !(state & GEDIT_WINDOW_STATE_SAVING) &&
				  !(state & GEDIT_WINDOW_STATE_PRINTING));

	action = gtk_action_group_get_action (window_mdi->priv->action_group,
				              "FileSaveAll");
	gtk_action_set_sensitive (action, 
				  !(state & GEDIT_WINDOW_STATE_PRINTING));

	action = gtk_action_group_get_action (window_mdi->priv->action_group,
				              "FileCloseAll");
	gtk_action_set_sensitive (action, 
				  !(state & GEDIT_WINDOW_STATE_SAVING) &&
				  !(state & GEDIT_WINDOW_STATE_PRINTING));

	if ((state & GEDIT_WINDOW_STATE_SAVING_SESSION) != 0)
		if (gtk_action_group_get_sensitive (window_mdi->priv->quit_action_group))
			gtk_action_group_set_sensitive (window_mdi->priv->quit_action_group,
							FALSE);
	else
		if (!gtk_action_group_get_sensitive (window_mdi->priv->quit_action_group))
			gtk_action_group_set_sensitive (window_mdi->priv->quit_action_group,
							window_mdi->priv->num_tabs != 0);

	gedit_notebook_set_close_buttons_sensitive (GEDIT_NOTEBOOK (window_mdi->priv->notebook),
						    !(state & GEDIT_WINDOW_STATE_SAVING_SESSION));
						    
	gedit_notebook_set_tab_drag_and_drop_enabled (GEDIT_NOTEBOOK (window_mdi->priv->notebook),
						      !(state & GEDIT_WINDOW_STATE_SAVING_SESSION));
}

static void
notebook_tab_added (GeditNotebook *notebook,
		    GeditTab      *tab,
		    GeditWindowMdi   *window)
{
	GtkAction *action;
	
	gedit_debug (DEBUG_WINDOW);

	g_return_if_fail ((gedit_window_get_state (GEDIT_WINDOW (window)) & GEDIT_WINDOW_STATE_SAVING_SESSION) == 0);

	/* emit the signal, the GeditWindow will add the tab in the default
	   handler */
	g_signal_emit_by_name (window, "tab_added", tab);
	++window->priv->num_tabs;

	/* Set sensitivity */
	if (!gtk_action_group_get_sensitive (window->priv->action_group))
		gtk_action_group_set_sensitive (window->priv->action_group,
						TRUE);

	action = gtk_action_group_get_action (window->priv->action_group,
					     "DocumentsMoveToNewWindow");
	gtk_action_set_sensitive (action,
				  window->priv->num_tabs > 1);

	update_documents_list_menu (window);
}

static void 
notebook_tab_removed (GeditNotebook *notebook,
		      GeditTab      *tab,
		      GeditWindowMdi   *window)
{
	GtkAction     *action;
	
	gedit_debug (DEBUG_WINDOW);
	
	g_return_if_fail ((gedit_window_get_state (GEDIT_WINDOW (window)) & GEDIT_WINDOW_STATE_SAVING_SESSION) == 0);

	/* emit the signal, the GeditWindow will remove the tab in the default
	   handler */
	g_signal_emit_by_name (window, "tab_removed", tab);
	--window->priv->num_tabs;

	g_return_if_fail (window->priv->num_tabs >= 0);

	if (!_gedit_window_is_removing_tabs (GEDIT_WINDOW (window)))
	{
		update_documents_list_menu (window);
	}
	else
	{
		if (window->priv->num_tabs == 0)
			update_documents_list_menu (window);
	}

	/* Set sensitivity */
	if (window->priv->num_tabs == 0)
		if (gtk_action_group_get_sensitive (window->priv->action_group))
			gtk_action_group_set_sensitive (window->priv->action_group,
							FALSE);

	if (window->priv->num_tabs <= 1) {
		action = gtk_action_group_get_action (window->priv->action_group,
						     "DocumentsMoveToNewWindow");

		gtk_action_set_sensitive (action,
					  FALSE);
	}	
}

static void
notebook_tabs_reordered (GeditNotebook *notebook,
			 GeditWindowMdi   *window)
{
	update_documents_list_menu (window);

	g_signal_emit_by_name (window, "tabs_reordered");
}

static void
notebook_tab_detached (GeditNotebook *notebook,
		       GeditTab      *tab,
		       GeditWindowMdi   *window)
{
	/* CHECK: we must be smart here
	GeditWindowMdi *new_window;
	
	new_window = clone_window (window);

	gedit_notebook_move_tab (notebook,
				 GEDIT_NOTEBOOK (_gedit_window_mdi_get_notebook (new_window)),
				 tab, 0);
				 
	gtk_window_set_position (GTK_WINDOW (new_window), 
				 GTK_WIN_POS_MOUSE);
					 
	gtk_widget_show (GTK_WIDGET (new_window)); */
}		      

static void 
notebook_tab_close_request (GeditNotebook *notebook,
			    GeditTab      *tab,
			    GtkWindow     *window)
{
	/* CHECK: check the code path please */
	/* Note: we are destroying the tab before the default handler
	 * seems to be ok, but we need to keep an eye on this. */
	_gedit_cmd_file_close_tab (tab, GEDIT_WINDOW (window));
}

static gboolean
show_notebook_popup_menu (GtkNotebook    *notebook,
			  GeditWindowMdi    *window,
			  GdkEventButton *event)
{
	GtkWidget    *menu;
	GtkUIManager *manager;
//	GtkAction *action;

	manager = gedit_window_get_ui_manager (GEDIT_WINDOW (window));
	menu = gtk_ui_manager_get_widget (manager, "/NotebookPopup");
	g_return_val_if_fail (menu != NULL, FALSE);

// CHECK do we need this?
#if 0
	/* allow extensions to sync when showing the popup */
	action = gtk_action_group_get_action (window->priv->action_group,
					      "NotebookPopupAction");
	g_return_val_if_fail (action != NULL, FALSE);
	gtk_action_activate (action);
#endif
	if (event != NULL)
	{
		gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
				NULL, NULL,
				event->button, event->time);
	}
	else
	{
		GtkWidget *tab;
		GtkWidget *tab_label;

		tab = GTK_WIDGET (gedit_window_get_active_tab (GEDIT_WINDOW (window)));
		g_return_val_if_fail (tab != NULL, FALSE);

		tab_label = gtk_notebook_get_tab_label (notebook, tab);

		gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
				gedit_utils_menu_position_under_widget, tab_label,
				0, gtk_get_current_event_time ());

		gtk_menu_shell_select_first (GTK_MENU_SHELL (menu), FALSE);
	}

	return TRUE;
}

static gboolean
notebook_button_press_event (GtkNotebook    *notebook,
			     GdkEventButton *event,
			     GeditWindowMdi    *window)
{
	if (GDK_BUTTON_PRESS == event->type && 3 == event->button)
	{
		return show_notebook_popup_menu (notebook, window, event);
	}

	return FALSE;
}

static gboolean
notebook_popup_menu (GtkNotebook *notebook,
		     GeditWindowMdi *window)
{
	/* Only respond if the notebook is the actual focus */
	if (GEDIT_IS_NOTEBOOK (gtk_window_get_focus (GTK_WINDOW (window))))
	{
		return show_notebook_popup_menu (notebook, window, NULL);
	}

	return FALSE;
}

static void
gedit_window_mdi_init (GeditWindowMdi *window)
{
	GtkWidget *documents_panel;

	gedit_debug (DEBUG_WINDOW);

	window->priv = GEDIT_WINDOW_MDI_GET_PRIVATE (window);
	window->priv->num_tabs = 0;

	/* Add menu */
	merge_menu (window);
  	
	gedit_debug_message (DEBUG_WINDOW, "Create gedit notebook");
	window->priv->notebook = gedit_notebook_new ();

	_gedit_window_set_widget (GEDIT_WINDOW (window), window->priv->notebook);

	/* Create the documents panel */
	documents_panel = gedit_documents_panel_new (GEDIT_WINDOW (window));
        gedit_panel_add_item_with_stock_icon (gedit_window_get_side_panel (GEDIT_WINDOW (window)),
                                              documents_panel,
                                              "Documents",
                                              GTK_STOCK_FILE);

	/* Connect signals */
	g_signal_connect (window->priv->notebook,
			  "switch_page",
			  G_CALLBACK (notebook_switch_page),
			  window);
	g_signal_connect (window->priv->notebook,
			  "tab_added",
			  G_CALLBACK (notebook_tab_added),
			  window);
	g_signal_connect (window->priv->notebook,
			  "tab_removed",
			  G_CALLBACK (notebook_tab_removed),
			  window);
	g_signal_connect (window->priv->notebook,
			  "tabs_reordered",
			  G_CALLBACK (notebook_tabs_reordered),
			  window);			  
	g_signal_connect (window->priv->notebook,
			  "tab_detached",
			  G_CALLBACK (notebook_tab_detached),
			  window);
	g_signal_connect (window->priv->notebook,
			  "tab_close_request",
			  G_CALLBACK (notebook_tab_close_request),
			  window);
	g_signal_connect (window->priv->notebook,
			  "button-press-event",
			  G_CALLBACK (notebook_button_press_event),
			  window);
	g_signal_connect (window->priv->notebook,
			  "popup-menu",
			  G_CALLBACK (notebook_popup_menu),
			  window);

	gedit_debug_message (DEBUG_WINDOW, "END");
}

GtkWidget *
_gedit_window_mdi_get_notebook (GeditWindowMdi *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW_MDI (window), NULL);

	return window->priv->notebook;
}

static GeditTab *
gedit_window_mdi_create_tab (GeditWindow *window,
			     gboolean     jump_to)
{
	GeditTab       *tab;
	GeditWindowMdi *window_mdi;
	
	g_return_val_if_fail (GEDIT_IS_WINDOW_MDI (window), NULL);

	window_mdi = GEDIT_WINDOW_MDI (window);

	tab = GEDIT_TAB (_gedit_tab_new ());	
	gtk_widget_show (GTK_WIDGET (tab));	

	gedit_notebook_add_tab (GEDIT_NOTEBOOK (window_mdi->priv->notebook),
				tab,
				-1,
				jump_to);

	return tab;
}

static GeditTab *
gedit_window_mdi_create_tab_from_uri (GeditWindow         *window,
				      const gchar         *uri,
				      const GeditEncoding *encoding,
				      gint                 line_pos,
				      gboolean             create,
				      gboolean             jump_to)
{
	GtkWidget      *tab;
	GeditWindowMdi *window_mdi;

	g_return_val_if_fail (GEDIT_IS_WINDOW_MDI (window), NULL);
	g_return_val_if_fail (uri != NULL, NULL);

	window_mdi = GEDIT_WINDOW_MDI (window);
	tab = _gedit_tab_new_from_uri (uri,
				       encoding,
				       line_pos,
				       create);	
	if (tab == NULL)
		return NULL;

	gtk_widget_show (tab);	
	
	gedit_notebook_add_tab (GEDIT_NOTEBOOK (window_mdi->priv->notebook),
				GEDIT_TAB (tab),
				-1,
				jump_to);
				
	return GEDIT_TAB (tab);
}				  

static void
gedit_window_mdi_close_tab (GeditWindow *window,
		 	    GeditTab    *tab)
{
	g_return_if_fail (GEDIT_IS_WINDOW_MDI (window));
	g_return_if_fail (GEDIT_IS_TAB (tab));
	g_return_if_fail ((gedit_tab_get_state (tab) != GEDIT_TAB_STATE_SAVING) &&
			  (gedit_tab_get_state (tab) != GEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW));
	
	gedit_notebook_remove_tab (GEDIT_NOTEBOOK (GEDIT_WINDOW_MDI(window)->priv->notebook),
				   tab);
}

static void
gedit_window_mdi_set_active_tab (GeditWindow *window,
			         GeditTab    *tab)
{
	gint            page_num;
	GeditWindowMdi *window_mdi;
	
	g_return_if_fail (GEDIT_IS_WINDOW_MDI (window));
	g_return_if_fail (GEDIT_IS_TAB (tab));
	
	window_mdi = GEDIT_WINDOW_MDI (window);
	
	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (window_mdi->priv->notebook),
					  GTK_WIDGET (tab));

	g_return_if_fail (page_num != -1);

	if (gtk_notebook_get_current_page (GTK_NOTEBOOK (
					       window_mdi->priv->notebook)) == page_num)
		return;
	
	gtk_notebook_set_current_page (GTK_NOTEBOOK (window_mdi->priv->notebook),
				       page_num);
}

GeditWindowMdi *
_gedit_window_mdi_move_tab_to_new_window (GeditWindowMdi *window,
				      GeditTab    *tab)
{
	/* CHECK: handle this properly
	GeditWindowMdi *new_window;

	g_return_val_if_fail (GEDIT_IS_WINDOW_MDI (window), NULL);
	g_return_val_if_fail (GEDIT_IS_TAB (tab), NULL);
	g_return_val_if_fail (gtk_notebook_get_n_pages (
				GTK_NOTEBOOK (window->priv->notebook)) > 1, 
			      NULL);
			      
	new_window = clone_window (window);

	gedit_notebook_move_tab (GEDIT_NOTEBOOK (window->priv->notebook),
				 GEDIT_NOTEBOOK (new_window->priv->notebook),
				 tab,
				 -1);
				 
	gtk_widget_show (GTK_WIDGET (new_window));
	
	return new_window;*/
	
	return NULL;
}
