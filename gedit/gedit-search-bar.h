/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Paolo Borelli
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
 * 
 */
 
/*
 * Modified by the gedit Team, 2004. See the AUTHORS file for a 
 * list of people on the gedit Team.  
 * See the ChangeLog files for a list of changes. 
 */

#ifndef __GEDIT_SEARCH_BAR_H__
#define __GEDIT_SEARCH_BAR_H__

#include "gedit-search-bar-base.h"


#define GEDIT_TYPE_SEARCH_BAR            (gedit_search_bar_get_type ())
#define GEDIT_SEARCH_BAR(obj)            (GTK_CHECK_CAST ((obj), GEDIT_TYPE_SEARCH_BAR, GeditSearchBar))
#define GEDIT_SEARCH_BAR_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GEDIT_TYPE_SEARCH_BAR, GeditSearchBarClass))
#define GEDIT_IS_SEARCH_BAR(obj)         (GTK_CHECK_TYPE ((obj), GEDIT_TYPE_SEARCH_BAR))
#define GEDIT_IS_SEARCH_BAR_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_SEARCH_BAR))
#define GEDIT_SEARCH_BAR_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), GEDIT_TYPE_SEARCH_BAR, GeditSearchBarClass))

typedef struct _GeditSearchBar        GeditSearchBar;
typedef struct _GeditSearchBarClass   GeditSearchBarClass;
typedef struct _GeditSearchBarPrivate GeditSearchBarPrivate;

struct _GeditSearchBar
{
	GeditSearchBarBase base;

	GeditSearchBarPrivate *priv;
};

struct _GeditSearchBarClass
{
	GeditSearchBarBaseClass parent_class;
};


GType      gedit_search_bar_get_type (void) G_GNUC_CONST;

GtkWidget *gedit_search_bar_new (void);

void       gedit_search_bar_find (void);

#endif /* __GEDIT_SEARCH_BAR_H__ */

