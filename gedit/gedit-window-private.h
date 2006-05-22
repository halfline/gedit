/*
 * gedit-window-private.h
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
 * MERCHANWINDOWILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
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

#ifndef __GEDIT_WINDOW_PRIVATE_H__
#define __GEDIT_WINDOW_PRIVATE_H__

#include "gedit/gedit-window.h"
#include "gedit-prefs-manager.h"
#include "recent-files/egg-recent-view-uimanager.h"

G_BEGIN_DECLS

/* WindowPrivate is in a separate .h so that we can access it from gedit-commands */

struct _GeditWindowPrivate
{
	GtkWidget      *side_panel;
	GtkWidget      *bottom_panel;

	GtkWidget      *hpaned;
	GtkWidget      *vpaned;	

	/* statusbar and context ids for statusbar messages */
	GtkWidget      *statusbar;	
	guint           generic_message_cid;
	guint           tip_message_cid;

	/* Menus & Toolbars */
	GtkUIManager   *manager;
	GtkActionGroup *action_group;
	GtkActionGroup *always_sensitive_action_group;
	GtkActionGroup *languages_action_group;
	GtkWidget      *toolbar;
	GtkWidget      *toolbar_recent_menu;
	GeditToolbarSetting toolbar_style;

	EggRecentViewUIManager *recent_view_uim;

	GeditTab       *active_tab;
	GList	       *tabs;
	gboolean        is_removing_tabs;

	gint            num_tabs_with_error;

	gint            width;
	gint            height;	
	GdkWindowState  window_state;

	gint		side_panel_size;
	gint		bottom_panel_size;
	
	GeditWindowState state;

	GtkWindowGroup *window_group;
	
	gchar          *default_path;
};

G_END_DECLS

#endif  /* __GEDIT_WINDOW_PRIVATE_H__  */
