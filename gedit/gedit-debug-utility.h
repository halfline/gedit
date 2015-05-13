/*
 * gedit-debug-utility.h
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

#ifndef __GEDIT_DEBUG_UTILITY_H__
#define __GEDIT_DEBUG_UTILITY_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_DEBUG_UTILITY _gedit_debug_utility_get_type ()
G_DECLARE_FINAL_TYPE (GeditDebugUtility, _gedit_debug_utility, GEDIT, DEBUG_UTILITY, GObject)

GeditDebugUtility *
		_gedit_debug_utility_new		(void);

void 		_gedit_debug_utility_add_section	(GeditDebugUtility *debug,
							 const gchar       *section_name,
							 guint64            section_flag);

void		_gedit_debug_utility_add_all_section	(GeditDebugUtility *debug,
							 const gchar       *all_section_name);

void		_gedit_debug_utility_message		(GeditDebugUtility *debug,
							 guint64            section,
							 const gchar       *file,
							 gint               line,
							 const gchar       *function,
							 const gchar       *format,
							 ...);

G_END_DECLS

#endif /* __GEDIT_DEBUG_UTILITY_H__ */

/* ex:set ts=8 noet: */
