/*
 * This file is part of gcode.
 *
 * Copyright 1998, 1999 - Alex Roberts, Evan Lawrence
 * Copyright 2000, 2001 - Chema Celorio
 * Copyright 2000-2005 - Paolo Maggi
 * Copyright 2015 - Sébastien Wilmet <swilmet@gnome.org>
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

#ifndef __GCODE_DEBUG_H__
#define __GCODE_DEBUG_H__

#include <glib.h>

/**
 * GcodeDebugSection:
 *
 * See GeditDebugSection. Replace "GEDIT" by "GCODE".
 */
typedef enum {
	GCODE_NO_DEBUG       = 0,
	GCODE_DEBUG_UTILS    = 1 << 0,
	GCODE_DEBUG_METADATA = 1 << 1,
	GCODE_DEBUG_DOCUMENT = 1 << 2,
} GcodeDebugSection;

#define	DEBUG_UTILS	GCODE_DEBUG_UTILS,   __FILE__, __LINE__, G_STRFUNC
#define	DEBUG_METADATA	GCODE_DEBUG_METADATA,__FILE__, __LINE__, G_STRFUNC
#define	DEBUG_DOCUMENT	GCODE_DEBUG_DOCUMENT,__FILE__, __LINE__, G_STRFUNC

void gcode_debug_init (void);

void gcode_debug (GcodeDebugSection  section,
		  const gchar       *file,
		  gint               line,
		  const gchar       *function);

void gcode_debug_message (GcodeDebugSection  section,
			  const gchar       *file,
			  gint               line,
			  const gchar       *function,
			  const gchar       *format, ...) G_GNUC_PRINTF(5, 6);

#endif /* __GCODE_DEBUG_H__ */

/* ex:set ts=8 noet: */
