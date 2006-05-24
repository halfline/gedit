/*
 * gedit-window-sdi.c
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
#include "gedit-window-sdi.h"
#include "gedit-commands.h"
#include "gedit-debug.h"
#include "gedit-utils.h"
#include "gedit-app.h"

struct _GeditWindowSdiPrivate
{
	GtkWidget *tab;
};

#define GEDIT_WINDOW_SDI_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object),\
					 GEDIT_TYPE_WINDOW_SDI,                    \
					 GeditWindowSdiPrivate))

static GeditTab *gedit_window_sdi_create_tab          (GeditWindow         *window,
						       gboolean             jump_to);
static GeditTab *gedit_window_sdi_create_tab_from_uri (GeditWindow         *window,
					 	       const gchar         *uri,
						       const GeditEncoding *encoding,
						       gint                 line_pos,
						       gboolean             create,
						       gboolean             jump_to);
static void	 gedit_window_sdi_close_tab           (GeditWindow         *window,
						       GeditTab            *tab);
static void	 gedit_window_sdi_set_active_tab      (GeditWindow         *window,
						       GeditTab            *tab);
						  
G_DEFINE_TYPE(GeditWindowSdi, gedit_window_sdi, GEDIT_TYPE_WINDOW)

static void
gedit_window_sdi_finalize (GObject *object)
{
	//GeditWindowSdi *window = GEDIT_WINDOW_SDI (object); 
	G_OBJECT_CLASS (gedit_window_sdi_parent_class)->finalize (object);
}

static void
gedit_window_sdi_class_init (GeditWindowSdiClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GeditWindowClass *window_class = GEDIT_WINDOW_CLASS (klass);
	
	object_class->finalize = gedit_window_sdi_finalize;

	window_class->create_tab = gedit_window_sdi_create_tab;
	window_class->create_tab_from_uri = gedit_window_sdi_create_tab_from_uri;
	window_class->close_tab = gedit_window_sdi_close_tab;
	window_class->set_active_tab = gedit_window_sdi_set_active_tab;

	g_type_class_add_private (object_class, sizeof(GeditWindowSdiPrivate));
}

static void
add_tab (GeditWindowSdi *window,
         GeditTab       *tab)
{
	window->priv->tab = GTK_WIDGET (tab);
	g_signal_emit_by_name (window, "tab_added", tab);

	_gedit_window_set_widget (GEDIT_WINDOW (window), GTK_WIDGET (tab));
	gedit_window_set_active_tab (GEDIT_WINDOW (window), 
	                             tab);
}

static void
create_tab (GeditWindowSdi *window)
{
	add_tab (window, GEDIT_TAB (_gedit_tab_new ()));
}

static void
create_tab_from_uri (GeditWindowSdi      *window, 
		     const gchar         *uri,
		     const GeditEncoding *encoding,
		     gint                 line_pos,
		     gboolean             create)
{
	add_tab (window, GEDIT_TAB (_gedit_tab_new_from_uri (uri,
	                                                     encoding,
	                                                     line_pos,
  	                                                     create)));
}

static void
gedit_window_sdi_init (GeditWindowSdi *window)
{
	gedit_debug (DEBUG_WINDOW);

	window->priv = GEDIT_WINDOW_SDI_GET_PRIVATE (window);
	gedit_debug_message (DEBUG_WINDOW, "END");
}

static GeditTab *
gedit_window_sdi_create_tab (GeditWindow *window,
			     gboolean     jump_to)
{
	GeditWindow    *new_window;
	GeditWindowSdi *window_sdi;
	
	g_return_val_if_fail (GEDIT_IS_WINDOW_SDI (window), NULL);
		
	window_sdi = GEDIT_WINDOW_SDI (window);
	
	if (window_sdi->priv->tab == NULL) {
		create_tab(window_sdi);
		
		return GEDIT_TAB (window_sdi->priv->tab);
	} else {
		new_window = gedit_app_create_window_simple (gedit_app_get_default (), 
	                                gtk_window_get_screen (GTK_WINDOW (window)));

		/* Clone this window */
		_gedit_window_clone (new_window, window);
		gedit_window_create_tab (new_window, TRUE);
		
		gtk_widget_show (GTK_WIDGET (new_window));
	}

	return NULL;
}

static GeditTab *
gedit_window_sdi_create_tab_from_uri (GeditWindow         *window,
				      const gchar         *uri,
				      const GeditEncoding *encoding,
				      gint                 line_pos,
				      gboolean             create,
				      gboolean             jump_to)
{
	GeditWindowSdi *window_sdi;
	GeditDocument  *document = NULL;
	GeditWindow    *new_window;
	GeditTab       *tab;

	g_return_val_if_fail (GEDIT_IS_WINDOW_SDI (window), NULL);

	window_sdi = GEDIT_WINDOW_SDI (window);
	
	if (window_sdi->priv->tab != NULL)
		document = gedit_tab_get_document (GEDIT_TAB (window_sdi->priv->tab));
	
	/* Check if the document is untitled and untouched, open the new
	   uri in the current doc if that's the case. Otherwise open a new
	   sdi window */
	if (document == NULL || (gedit_document_is_untitled (document) && 
	    gedit_document_is_untouched (document))) {
	    	if (document != NULL) {
			_gedit_tab_load (GEDIT_TAB (window_sdi->priv->tab), 
		 	                 uri, 
	        		         encoding, 
	                		 line_pos,
		                	 create);
		} else {
			create_tab_from_uri (window_sdi,
			                     uri, 
			                     encoding, 
			                     line_pos, 
			                     create);
		}
		
		tab = GEDIT_TAB (window_sdi->priv->tab);
	} else {
		new_window = gedit_app_create_window_simple (
				gedit_app_get_default (),
				gtk_window_get_screen (GTK_WINDOW (window)));

		/* Clone this window */
		_gedit_window_clone (new_window, window);
		tab = gedit_window_create_tab_from_uri (new_window, 
		                                        uri, 
		                                        encoding, 
		                                        line_pos, 
		                                        create, 
		                                        jump_to);

		gtk_widget_show (GTK_WIDGET (new_window));
	}
	
	return tab;
}				  

static void
gedit_window_sdi_close_tab (GeditWindow *window,
		 	    GeditTab    *tab)
{
	GeditWindowSdi *window_sdi;
	
	g_return_if_fail (GEDIT_IS_WINDOW_SDI (window));
	g_return_if_fail (GEDIT_IS_TAB (tab));
	g_return_if_fail ((gedit_tab_get_state (tab) != GEDIT_TAB_STATE_SAVING) &&
			  (gedit_tab_get_state (tab) != GEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW));
	
	window_sdi = GEDIT_WINDOW_SDI (window);
	
	g_return_if_fail (tab == GEDIT_TAB (window_sdi->priv->tab));
	
	g_signal_emit_by_name (window, "tab_removed", tab);
	gtk_widget_destroy (GTK_WIDGET (tab));

	gedit_cmd_file_quit (NULL, window);
}

static void
gedit_window_sdi_set_active_tab (GeditWindow *window,
			         GeditTab    *tab)
{
	GeditWindowSdi *window_sdi;

	g_return_if_fail (GEDIT_IS_WINDOW_SDI (window));
	g_return_if_fail (GEDIT_IS_TAB (tab));
	
	window_sdi = GEDIT_WINDOW_SDI (window);
	
	if (window_sdi->priv->tab == NULL)
		add_tab (window_sdi, tab);
	
	g_return_if_fail (tab == GEDIT_TAB (window_sdi->priv->tab));
}
