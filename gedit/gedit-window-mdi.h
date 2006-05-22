/*
 * gedit-window-mdi.h
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

#ifndef __GEDIT_WINDOW_MDI_H__
#define __GEDIT_WINDOW_MDI_H__

#include <gtk/gtk.h>

#include <gedit/gedit-window.h>

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define GEDIT_TYPE_WINDOW_MDI              (gedit_window_mdi_get_type())
#define GEDIT_WINDOW_MDI(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GEDIT_TYPE_WINDOW_MDI, GeditWindowMdi))
#define GEDIT_WINDOW_MDI_CONST(obj)        (G_TYPE_CHECK_INSTANCE_CAST((obj), GEDIT_TYPE_WINDOW_MDI, GeditWindowMdi const))
#define GEDIT_WINDOW_MDI_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GEDIT_TYPE_WINDOW_MDI, GeditWindowMdiClass))
#define GEDIT_IS_WINDOW_MDI(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GEDIT_TYPE_WINDOW_MDI))
#define GEDIT_IS_WINDOW_MDI_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_WINDOW_MDI))
#define GEDIT_WINDOW_MDI_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GEDIT_TYPE_WINDOW_MDI, GeditWindowMdiClass))

/* Private structure type */
typedef struct _GeditWindowMdiPrivate GeditWindowMdiPrivate;

/*
 * Main object structure
 */
typedef struct _GeditWindowMdi GeditWindowMdi;

struct _GeditWindowMdi 
{
	GeditWindow window;

	/*< private > */
	GeditWindowMdiPrivate *priv;
};

/*
 * Class definition
 */
typedef struct _GeditWindowMdiClass GeditWindowMdiClass;

struct _GeditWindowMdiClass 
{
	GeditWindowClass parent_class;
};

/*
 * Public methods
 */
GType 		 gedit_window_mdi_get_type 			(void) G_GNUC_CONST;
							 
/*
 * Non exported functions
 */
GtkWidget	*_gedit_window_mdi_get_notebook		  (GeditWindowMdi *window);

GeditWindowMdi	*_gedit_window_mdi_move_tab_to_new_window (GeditWindowMdi *window,
							   GeditTab       *tab);
							 
G_END_DECLS

#endif  /* __GEDIT_WINDOW_MDI_H__  */
