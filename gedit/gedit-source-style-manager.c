/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gedit-source-style-manager.c
 *
 * Copyright (C) 2007 - Paolo Borelli
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
 * $Id: gedit-source-style-manager.c 5598 2007-04-15 13:16:24Z pborelli $
 */

#include "gedit-source-style-manager.h"
#include "gedit-prefs-manager.h"

static GtkSourceStyleManager *style_manager = NULL;
static GtkSourceStyleScheme  *def_style = NULL;

GtkSourceStyleManager *
gedit_get_source_style_manager (void)
{
	if (style_manager == NULL)
	{
		style_manager = gtk_source_style_manager_new ();
	}

	return style_manager;
}

GtkSourceStyleScheme *
gedit_source_style_manager_get_default_scheme (GtkSourceStyleManager *manager)
{
	g_return_val_if_fail (GTK_IS_SOURCE_STYLE_MANAGER (manager), def_style);

	if (def_style == NULL)
	{
		gchar *name;

		name = gedit_prefs_manager_get_source_style_scheme ();
		def_style = gtk_source_style_manager_get_scheme (manager,
							         name);
		g_free (name);
	}

	return def_style;
}

// TODO: this function will be used when listening to gconf notifications
void
_gedit_source_style_manager_set_default_scheme (GtkSourceStyleManager *manager,
						const gchar           *name)
{
	g_return_if_fail (GTK_IS_SOURCE_STYLE_MANAGER (manager));

	// FIXME: accept null name as "reset to default"?

	if (def_style != NULL)
		g_object_unref (def_style);

	def_style = gtk_source_style_manager_get_scheme (manager,
						         name);
}

