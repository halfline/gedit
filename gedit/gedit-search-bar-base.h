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

#ifndef __GEDIT_SEARCH_BAR_BASE_H__
#define __GEDIT_SEARCH_BAR_BASE_H__

#include <gtk/gtkhbox.h>

#define GEDIT_TYPE_SEARCH_BAR_BASE            (gedit_search_bar_base_get_type ())
#define GEDIT_SEARCH_BAR_BASE(obj)            (GTK_CHECK_CAST ((obj), GEDIT_TYPE_SEARCH_BAR_BASE, GeditSearchBarBase))
#define GEDIT_SEARCH_BAR_BASE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GEDIT_TYPE_SEARCH_BAR_BASE, GeditSearchBarBaseClass))
#define GEDIT_IS_SEARCH_BAR_BASE(obj)         (GTK_CHECK_TYPE ((obj), GEDIT_TYPE_SEARCH_BAR_BASE))
#define GEDIT_IS_SEARCH_BASE_BAR_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_SEARCH_BAR_BASE))
#define GEDIT_SEARCH_BAR_BASE_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), GEDIT_TYPE_SEARCH_BAR_BASE, GeditSearchBarBaseClass))

typedef enum
{
	GEDIT_SEARCH_BAR_FORWARD        = 1 << 0,
	GEDIT_SEARCH_BAR_CASE_SENSITIVE = 1 << 2,
	GEDIT_SEARCH_BAR_ENTIRE_WORD    = 1 << 3
} GeditSearchBarFlags;

typedef struct _GeditSearchBarBase        GeditSearchBarBase;
typedef struct _GeditSearchBarBaseClass   GeditSearchBarBaseClass;
typedef struct _GeditSearchBarBasePrivate GeditSearchBarBasePrivate;

struct _GeditSearchBarBase
{
	GtkHBox box;

	GeditSearchBarBasePrivate *priv;
};

struct _GeditSearchBarBaseClass
{
	GtkHBoxClass parent_class;

	void (*find) (GeditSearchBarBase *bar, const gchar *string, GeditSearchBarFlags flags);

	void (*find_all) (GeditSearchBarBase *bar, const gchar *string, GeditSearchBarFlags flags);
};


GType      gedit_search_bar_base_get_type (void) G_GNUC_CONST;

GtkWidget *gedit_search_bar_base_new (void);

void       gedit_search_bar_base_set_search_string (GeditSearchBarBase *bar, const gchar* str);

void       gedit_search_bar_base_set_search_flags (GeditSearchBarBase *bar, GeditSearchBarFlags flags);

#endif /* __GEDIT_SEARCH_BAR_BASE_H__ */

