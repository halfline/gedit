/*
 * gedit-menu-extension.h
 * This file is part of gedit
 *
 * Copyright (C) 2014 - Ignacio Casal Quinteiro
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
 * along with gedit. If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef __GEDIT_MENU_EXTENSION_H__
#define __GEDIT_MENU_EXTENSION_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_MENU				(gedit_menu_extension_get_type ())
#define GEDIT_MENU_EXTENSION(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_MENU, GeditMenuExtension))
#define GEDIT_MENU_EXTENSION_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_MENU, GeditMenuExtension const))
#define GEDIT_MENU_EXTENSION_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GEDIT_TYPE_MENU, GeditMenuExtensionClass))
#define GEDIT_IS_MENU(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEDIT_TYPE_MENU))
#define GEDIT_IS_MENU_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_MENU))
#define GEDIT_MENU_EXTENSION_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GEDIT_TYPE_MENU, GeditMenuExtensionClass))

typedef struct _GeditMenuExtension	 GeditMenuExtension;
typedef struct _GeditMenuExtensionClass	 GeditMenuExtensionClass;

struct _GeditMenuExtension
{
	GObject parent;
};

struct _GeditMenuExtensionClass
{
	GObjectClass parent_class;
};

GType                     gedit_menu_extension_get_type            (void) G_GNUC_CONST;

GeditMenuExtension      *_gedit_menu_extension_new                 (GMenu                *menu);

void                      gedit_menu_extension_append_menu_item    (GeditMenuExtension   *menu,
                                                                    GMenuItem            *item);

void                      gedit_menu_extension_prepend_menu_item   (GeditMenuExtension   *menu,
                                                                    GMenuItem            *item);

void                      gedit_menu_extension_remove_items        (GeditMenuExtension   *menu);

G_END_DECLS

#endif /* __GEDIT_MENU_EXTENSION_H__ */
