/*
 * This file is part of gcode.
 *
 * Copyright 2003 - Paolo Maggi
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

#ifndef __GCODE_METADATA_MANAGER_H__
#define __GCODE_METADATA_MANAGER_H__

#include <gio/gio.h>

G_BEGIN_DECLS

void		 gcode_metadata_manager_init		(const gchar *metadata_filename);

void		 gcode_metadata_manager_shutdown 	(void);


gchar		*gcode_metadata_manager_get 		(GFile       *location,
					     		 const gchar *key);
void		 gcode_metadata_manager_set		(GFile       *location,
							 const gchar *key,
							 const gchar *value);

G_END_DECLS

#endif /* __GCODE_METADATA_MANAGER_H__ */

/* ex:set ts=8 noet: */
