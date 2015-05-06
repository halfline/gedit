/*
 * This file is part of gcode.
 *
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

#ifndef __GCODE_GEDIT_H__
#define __GCODE_GEDIT_H__

#include <gtksourceview/gtksource.h>

/* This header is a public gedit header (for plugins). It contains GObject
 * classes definition needed for the gedit public classes, so that they can
 * subclass the classes defined here.
 * For example GeditDocument is a subclass of GcodeDocument. Since GeditDocument
 * is a public class, it needs the class definition of GcodeDocument.
 */

G_BEGIN_DECLS

#define GCODE_TYPE_DOCUMENT (gcode_document_get_type())

G_DECLARE_DERIVABLE_TYPE (GcodeDocument, gcode_document, GCODE, DOCUMENT, GtkSourceBuffer)

struct _GcodeDocumentClass
{
	GtkSourceBufferClass parent_class;

	/* Signals */
	void (* cursor_moved)		(GcodeDocument *document);

	void (* load)			(GcodeDocument *document);

	void (* loaded)			(GcodeDocument *document);

	void (* save)			(GcodeDocument *document);

	void (* saved)  		(GcodeDocument *document);
};

G_END_DECLS

#endif /* __GCODE_GEDIT_H__ */

/* ex:set ts=8 noet: */
