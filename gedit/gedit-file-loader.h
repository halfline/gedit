/*
 * gedit-file-loader.h
 * This file is part of gedit
 *
 * Copyright (C) 2015 - Sébastien Wilmet <swilmet@gnome.org>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GEDIT_FILE_LOADER_H__
#define __GEDIT_FILE_LOADER_H__

#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_FILE_LOADER             (_gedit_file_loader_get_type ())
#define GEDIT_FILE_LOADER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_FILE_LOADER, GeditFileLoader))
#define GEDIT_FILE_LOADER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GEDIT_TYPE_FILE_LOADER, GeditFileLoaderClass))
#define GEDIT_IS_FILE_LOADER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEDIT_TYPE_FILE_LOADER))
#define GEDIT_IS_FILE_LOADER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_FILE_LOADER))
#define GEDIT_FILE_LOADER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GEDIT_TYPE_FILE_LOADER, GeditFileLoaderClass))

typedef struct _GeditFileLoader         GeditFileLoader;
typedef struct _GeditFileLoaderClass    GeditFileLoaderClass;
typedef struct _GeditFileLoaderPrivate  GeditFileLoaderPrivate;

struct _GeditFileLoader
{
	GtkSourceFileLoader parent;

	GeditFileLoaderPrivate *priv;
};

struct _GeditFileLoaderClass
{
	GtkSourceFileLoaderClass parent_class;
};

GType			 _gedit_file_loader_get_type			(void) G_GNUC_CONST;

GeditFileLoader		*_gedit_file_loader_new				(void);

G_END_DECLS

#endif /* __GEDIT_FILE_LOADER_H__ */

/* ex:set ts=8 noet: */
