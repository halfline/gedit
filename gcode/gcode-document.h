/*
 * This file is part of gcode.
 *
 * Copyright 1998, 1999 - Alex Roberts, Evan Lawrence
 * Copyright 2000, 2001 - Chema Celorio
 * Copyright 2000-2005 - Paolo Maggi
 * Copyright 2014-2015 - Sébastien Wilmet
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

#ifndef __GCODE_DOCUMENT_H__
#define __GCODE_DOCUMENT_H__

#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define GCODE_TYPE_DOCUMENT (gcode_document_get_type())

G_DECLARE_DERIVABLE_TYPE (GcodeDocument, gcode_document, GCODE, DOCUMENT, GtkSourceBuffer)

#ifdef G_OS_WIN32
#define GCODE_METADATA_ATTRIBUTE_POSITION "position"
#define GCODE_METADATA_ATTRIBUTE_ENCODING "encoding"
#define GCODE_METADATA_ATTRIBUTE_LANGUAGE "language"
#else
#define GCODE_METADATA_ATTRIBUTE_POSITION "metadata::gcode-position"
#define GCODE_METADATA_ATTRIBUTE_ENCODING "metadata::gcode-encoding"
#define GCODE_METADATA_ATTRIBUTE_LANGUAGE "metadata::gcode-language"
#endif

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

GcodeDocument   *gcode_document_new				(void);

GtkSourceFile	*gcode_document_get_file			(GcodeDocument       *doc);

G_DEPRECATED_FOR (gtk_source_file_get_location)
GFile		*gcode_document_get_location			(GcodeDocument       *doc);

G_DEPRECATED_FOR (gtk_source_file_set_location)
void		 gcode_document_set_location			(GcodeDocument       *doc,
								 GFile               *location);

gchar		*gcode_document_get_uri_for_display		(GcodeDocument       *doc);

gchar		*gcode_document_get_short_name_for_display	(GcodeDocument       *doc);

void		 gcode_document_set_short_name_for_display	(GcodeDocument       *doc,
								 const gchar         *short_name);

gchar		*gcode_document_get_content_type		(GcodeDocument       *doc);

void		 gcode_document_set_content_type		(GcodeDocument       *doc,
								 const gchar         *content_type);

gchar		*gcode_document_get_mime_type			(GcodeDocument       *doc);

gboolean	 gcode_document_get_readonly			(GcodeDocument       *doc);

gboolean	 gcode_document_is_untouched			(GcodeDocument       *doc);

gboolean	 gcode_document_is_untitled			(GcodeDocument       *doc);

gboolean	 gcode_document_is_local			(GcodeDocument       *doc);

gboolean	 gcode_document_get_deleted			(GcodeDocument       *doc);

gboolean	 gcode_document_goto_line			(GcodeDocument       *doc,
								gint                 line);

gboolean	 gcode_document_goto_line_offset		(GcodeDocument       *doc,
								 gint                 line,
								 gint                 line_offset);

void 		 gcode_document_set_language			(GcodeDocument       *doc,
								 GtkSourceLanguage   *lang);
GtkSourceLanguage
		*gcode_document_get_language			(GcodeDocument       *doc);

G_DEPRECATED_FOR (gtk_source_file_get_encoding)
const GtkSourceEncoding
		*gcode_document_get_encoding			(GcodeDocument       *doc);

G_DEPRECATED_FOR (gtk_source_file_get_newline_type)
GtkSourceNewlineType
		 gcode_document_get_newline_type		(GcodeDocument       *doc);

G_DEPRECATED_FOR (gtk_source_file_get_compression_type)
GtkSourceCompressionType
		 gcode_document_get_compression_type		(GcodeDocument       *doc);

gchar		*gcode_document_get_metadata			(GcodeDocument       *doc,
								 const gchar         *key);

void		 gcode_document_set_metadata			(GcodeDocument       *doc,
								 const gchar         *first_key,
								 ...);

glong		 _gcode_document_get_seconds_since_last_save_or_load	(GcodeDocument       *doc);

/* Note: this is a sync stat: use only on local files */
gboolean	 _gcode_document_check_externally_modified		(GcodeDocument       *doc);

gboolean	 _gcode_document_needs_saving				(GcodeDocument       *doc);

void		 _gcode_document_set_create				(GcodeDocument       *doc,
									 gboolean             create);

gboolean	 _gcode_document_get_create				(GcodeDocument       *doc);

G_END_DECLS

#endif /* __GCODE_DOCUMENT_H__ */

/* ex:set ts=8 noet: */
