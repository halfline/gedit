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

#include "gcode-document.h"

#include <string.h>
#include <glib/gi18n.h>

#include "gcode-metadata-manager.h"
#include "gcode-utils.h"
#include "gcode-debug.h"

#define METADATA_QUERY "metadata::*"

#define NO_LANGUAGE_NAME "_NORMAL_"

static void	gcode_document_loaded_real	(GcodeDocument *doc);

static void	gcode_document_saved_real	(GcodeDocument *doc);

typedef struct
{
	GtkSourceFile *file;

	gint 	     untitled_number;
	gchar       *short_name;

	GFileInfo   *metadata_info;

	gchar	    *content_type;

	GTimeVal     mtime;
	GTimeVal     time_of_last_save_or_load;

	guint user_action;

	guint readonly : 1;
	guint externally_modified : 1;
	guint deleted : 1;
	guint language_set_by_user : 1;
	guint mtime_set : 1;
	guint use_gvfs_metadata : 1;

	/* Create file if location points to a non existing file (for example
	 * when opened from the command line).
	 */
	guint create : 1;
} GcodeDocumentPrivate;

enum
{
	PROP_0,
	PROP_SHORTNAME,
	PROP_CONTENT_TYPE,
	PROP_MIME_TYPE,
	PROP_READ_ONLY,
	PROP_USE_GVFS_METADATA
};

enum
{
	CURSOR_MOVED,
	LOAD,
	LOADED,
	SAVE,
	SAVED,
	LAST_SIGNAL
};

static guint document_signals[LAST_SIGNAL] = { 0 };

static GHashTable *allocated_untitled_numbers = NULL;

G_DEFINE_TYPE_WITH_PRIVATE (GcodeDocument, gcode_document, GTK_SOURCE_TYPE_BUFFER)

static gint
get_untitled_number (void)
{
	gint i = 1;

	if (allocated_untitled_numbers == NULL)
		allocated_untitled_numbers = g_hash_table_new (NULL, NULL);

	g_return_val_if_fail (allocated_untitled_numbers != NULL, -1);

	while (TRUE)
	{
		if (g_hash_table_lookup (allocated_untitled_numbers, GINT_TO_POINTER (i)) == NULL)
		{
			g_hash_table_insert (allocated_untitled_numbers,
					     GINT_TO_POINTER (i),
					     GINT_TO_POINTER (i));

			return i;
		}

		++i;
	}
}

static void
release_untitled_number (gint n)
{
	g_return_if_fail (allocated_untitled_numbers != NULL);

	g_hash_table_remove (allocated_untitled_numbers, GINT_TO_POINTER (n));
}

static const gchar *
get_language_string (GcodeDocument *doc)
{
	GtkSourceLanguage *lang = gcode_document_get_language (doc);

	return lang != NULL ? gtk_source_language_get_id (lang) : NO_LANGUAGE_NAME;
}

static void
save_metadata (GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;
	const gchar *language = NULL;
	GtkTextIter iter;
	gchar *position;

	priv = gcode_document_get_instance_private (doc);
	if (priv->language_set_by_user)
	{
		language = get_language_string (doc);
	}

	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (doc),
					  &iter,
					  gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (doc)));

	position = g_strdup_printf ("%d", gtk_text_iter_get_offset (&iter));

	if (language == NULL)
	{
		gcode_document_set_metadata (doc,
					     GCODE_METADATA_ATTRIBUTE_POSITION, position,
					     NULL);
	}
	else
	{
		gcode_document_set_metadata (doc,
					     GCODE_METADATA_ATTRIBUTE_POSITION, position,
					     GCODE_METADATA_ATTRIBUTE_LANGUAGE, language,
					     NULL);
	}

	g_free (position);
}

static void
gcode_document_dispose (GObject *object)
{
	GcodeDocumentPrivate *priv;

	gcode_debug (DEBUG_DOCUMENT);

	priv = gcode_document_get_instance_private (GCODE_DOCUMENT (object));

	/* Metadata must be saved here and not in finalize because the language
	 * is gone by the time finalize runs.
	 */
	if (priv->file != NULL)
	{
		save_metadata (GCODE_DOCUMENT (object));

		g_object_unref (priv->file);
		priv->file = NULL;
	}

	g_clear_object (&priv->metadata_info);

	G_OBJECT_CLASS (gcode_document_parent_class)->dispose (object);
}

static void
gcode_document_finalize (GObject *object)
{
	GcodeDocumentPrivate *priv;

	gcode_debug (DEBUG_DOCUMENT);

	priv = gcode_document_get_instance_private (GCODE_DOCUMENT (object));

	if (priv->untitled_number > 0)
	{
		release_untitled_number (priv->untitled_number);
	}

	g_free (priv->content_type);
	g_free (priv->short_name);

	G_OBJECT_CLASS (gcode_document_parent_class)->finalize (object);
}

static void
gcode_document_get_property (GObject    *object,
			     guint       prop_id,
			     GValue     *value,
			     GParamSpec *pspec)
{
	GcodeDocument *doc = GCODE_DOCUMENT (object);
	GcodeDocumentPrivate *priv;

	priv = gcode_document_get_instance_private (doc);

	switch (prop_id)
	{
		case PROP_SHORTNAME:
			g_value_take_string (value, gcode_document_get_short_name_for_display (doc));
			break;

		case PROP_CONTENT_TYPE:
			g_value_take_string (value, gcode_document_get_content_type (doc));
			break;

		case PROP_MIME_TYPE:
			g_value_take_string (value, gcode_document_get_mime_type (doc));
			break;

		case PROP_READ_ONLY:
			g_value_set_boolean (value, priv->readonly);
			break;

		case PROP_USE_GVFS_METADATA:
			g_value_set_boolean (value, priv->use_gvfs_metadata);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gcode_document_set_property (GObject      *object,
			     guint         prop_id,
			     const GValue *value,
			     GParamSpec   *pspec)
{
	GcodeDocument *doc = GCODE_DOCUMENT (object);
	GcodeDocumentPrivate *priv = gcode_document_get_instance_private (doc);

	switch (prop_id)
	{
		case PROP_SHORTNAME:
			gcode_document_set_short_name_for_display (doc, g_value_get_string (value));
			break;

		case PROP_CONTENT_TYPE:
			gcode_document_set_content_type (doc, g_value_get_string (value));
			break;

		case PROP_USE_GVFS_METADATA:
			priv->use_gvfs_metadata = g_value_get_boolean (value);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gcode_document_begin_user_action (GtkTextBuffer *buffer)
{
	GcodeDocumentPrivate *priv;

	priv = gcode_document_get_instance_private (GCODE_DOCUMENT (buffer));

	++priv->user_action;

	if (GTK_TEXT_BUFFER_CLASS (gcode_document_parent_class)->begin_user_action != NULL)
	{
		GTK_TEXT_BUFFER_CLASS (gcode_document_parent_class)->begin_user_action (buffer);
	}
}

static void
gcode_document_end_user_action (GtkTextBuffer *buffer)
{
	GcodeDocumentPrivate *priv;

	priv = gcode_document_get_instance_private (GCODE_DOCUMENT (buffer));

	--priv->user_action;

	if (GTK_TEXT_BUFFER_CLASS (gcode_document_parent_class)->end_user_action != NULL)
	{
		GTK_TEXT_BUFFER_CLASS (gcode_document_parent_class)->end_user_action (buffer);
	}
}

static void
gcode_document_mark_set (GtkTextBuffer     *buffer,
                         const GtkTextIter *iter,
                         GtkTextMark       *mark)
{
	GcodeDocument *doc = GCODE_DOCUMENT (buffer);
	GcodeDocumentPrivate *priv;

	priv = gcode_document_get_instance_private (doc);

	if (GTK_TEXT_BUFFER_CLASS (gcode_document_parent_class)->mark_set != NULL)
	{
		GTK_TEXT_BUFFER_CLASS (gcode_document_parent_class)->mark_set (buffer, iter, mark);
	}

	if (mark == gtk_text_buffer_get_insert (buffer) && (priv->user_action == 0))
	{
		g_signal_emit (doc, document_signals[CURSOR_MOVED], 0);
	}
}

static void
gcode_document_changed (GtkTextBuffer *buffer)
{
	g_signal_emit (GCODE_DOCUMENT (buffer), document_signals[CURSOR_MOVED], 0);

	GTK_TEXT_BUFFER_CLASS (gcode_document_parent_class)->changed (buffer);
}

static void
gcode_document_class_init (GcodeDocumentClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkTextBufferClass *buf_class = GTK_TEXT_BUFFER_CLASS (klass);

	object_class->dispose = gcode_document_dispose;
	object_class->finalize = gcode_document_finalize;
	object_class->get_property = gcode_document_get_property;
	object_class->set_property = gcode_document_set_property;

	buf_class->begin_user_action = gcode_document_begin_user_action;
	buf_class->end_user_action = gcode_document_end_user_action;
	buf_class->mark_set = gcode_document_mark_set;
	buf_class->changed = gcode_document_changed;

	klass->loaded = gcode_document_loaded_real;
	klass->saved = gcode_document_saved_real;

	g_object_class_install_property (object_class, PROP_SHORTNAME,
					 g_param_spec_string ("shortname",
							      "Short Name",
							      "The document's short name",
							      NULL,
							      G_PARAM_READWRITE |
							      G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class, PROP_CONTENT_TYPE,
					 g_param_spec_string ("content-type",
							      "Content Type",
							      "The document's Content Type",
							      NULL,
							      G_PARAM_READWRITE |
							      G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class, PROP_MIME_TYPE,
					 g_param_spec_string ("mime-type",
							      "MIME Type",
							      "The document's MIME Type",
							      "text/plain",
							      G_PARAM_READABLE |
							      G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class, PROP_READ_ONLY,
					 g_param_spec_boolean ("read-only",
							       "Read Only",
							       "Whether the document is read only or not",
							       FALSE,
							       G_PARAM_READABLE |
							       G_PARAM_STATIC_STRINGS));

	/**
	 * GcodeDocument:use-gvfs-metadata:
	 *
	 * Whether to use GVFS metadata. If %FALSE, use the gcode metadata
	 * manager that stores the metadata in an XML file in the user cache
	 * directory.
	 *
	 * <warning>
	 * The property is used internally by gcode. It must not be used in a
	 * gcode plugin. The property can be modified or removed at any time.
	 * </warning>
	 */
	g_object_class_install_property (object_class, PROP_USE_GVFS_METADATA,
					 g_param_spec_boolean ("use-gvfs-metadata",
							       "Use GVFS metadata",
							       "",
							       TRUE,
							       G_PARAM_READWRITE |
							       G_PARAM_CONSTRUCT_ONLY |
							       G_PARAM_STATIC_STRINGS));

	/* This signal is used to update the cursor position in the statusbar,
	 * it's emitted either when the insert mark is moved explicitely or
	 * when the buffer changes (insert/delete).
	 * FIXME When the replace_all was implemented in gcode, this signal was
	 * not emitted during the replace_all to improve performance. Now the
	 * replace_all is implemented in GtkSourceView, so the signal is
	 * emitted.
	 */
	document_signals[CURSOR_MOVED] =
		g_signal_new ("cursor-moved",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcodeDocumentClass, cursor_moved),
			      NULL, NULL, NULL,
			      G_TYPE_NONE,
			      0);

	/**
	 * GcodeDocument::load:
	 * @document: the #GcodeDocument.
	 *
	 * The "load" signal is emitted at the beginning of a file loading.
	 *
	 * Before gcode 3.14 this signal contained parameters to configure the
	 * file loading (the location, encoding, etc). Plugins should not need
	 * those parameters.
	 *
	 * Since: 2.22
	 */
	document_signals[LOAD] =
		g_signal_new ("load",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcodeDocumentClass, load),
			      NULL, NULL, NULL,
			      G_TYPE_NONE, 0);

	/**
	 * GcodeDocument::loaded:
	 * @document: the #GcodeDocument.
	 *
	 * The "loaded" signal is emitted at the end of a successful file
	 * loading.
	 *
	 * Before gcode 3.14 this signal contained a #GError parameter, and the
	 * signal was also emitted if an error occurred. Plugins should not need
	 * the error parameter.
	 */
	document_signals[LOADED] =
		g_signal_new ("loaded",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GcodeDocumentClass, loaded),
			      NULL, NULL, NULL,
			      G_TYPE_NONE, 0);

	/**
	 * GcodeDocument::save:
	 * @document: the #GcodeDocument.
	 *
	 * The "save" signal is emitted at the beginning of a file saving.
	 *
	 * Before gcode 3.14 this signal contained parameters to configure the
	 * file saving (the location, encoding, etc). Plugins should not need
	 * those parameters.
	 *
	 * Since: 2.20
	 */
	document_signals[SAVE] =
		g_signal_new ("save",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcodeDocumentClass, save),
			      NULL, NULL, NULL,
			      G_TYPE_NONE, 0);

	/**
	 * GcodeDocument::saved:
	 * @document: the #GcodeDocument.
	 *
	 * The "saved" signal is emitted at the end of a successful file saving.
	 *
	 * Before gcode 3.14 this signal contained a #GError parameter, and the
	 * signal was also emitted if an error occurred. To save a document, a
	 * plugin can use the gcode_commands_save_document_async() function and
	 * get the result of the operation with
	 * gcode_commands_save_document_finish().
	 */
	document_signals[SAVED] =
		g_signal_new ("saved",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GcodeDocumentClass, saved),
			      NULL, NULL, NULL,
			      G_TYPE_NONE, 0);
}

static void
set_language (GcodeDocument     *doc,
              GtkSourceLanguage *lang,
              gboolean           set_by_user)
{
	GcodeDocumentPrivate *priv;
	GtkSourceLanguage *old_lang;

	gcode_debug (DEBUG_DOCUMENT);

	priv = gcode_document_get_instance_private (doc);

	old_lang = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (doc));

	if (old_lang == lang)
	{
		return;
	}

	gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (doc), lang);

	if (set_by_user)
	{
		const gchar *language = get_language_string (doc);

		gcode_document_set_metadata (doc,
					     GCODE_METADATA_ATTRIBUTE_LANGUAGE, language,
					     NULL);
	}

	priv->language_set_by_user = set_by_user;
}

static void
save_encoding_metadata (GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;
	const GtkSourceEncoding *encoding;
	const gchar *charset;

	gcode_debug (DEBUG_DOCUMENT);

	priv = gcode_document_get_instance_private (doc);

	encoding = gtk_source_file_get_encoding (priv->file);

	if (encoding == NULL)
	{
		encoding = gtk_source_encoding_get_utf8 ();
	}

	charset = gtk_source_encoding_get_charset (encoding);

	gcode_document_set_metadata (doc,
				     GCODE_METADATA_ATTRIBUTE_ENCODING, charset,
				     NULL);
}

static GtkSourceLanguage *
guess_language (GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;
	gchar *data;
	GtkSourceLanguageManager *manager = gtk_source_language_manager_get_default ();
	GtkSourceLanguage *language = NULL;

	priv = gcode_document_get_instance_private (doc);

	data = gcode_document_get_metadata (doc, GCODE_METADATA_ATTRIBUTE_LANGUAGE);

	if (data != NULL)
	{
		gcode_debug_message (DEBUG_DOCUMENT, "Language from metadata: %s", data);

		if (!g_str_equal (data, NO_LANGUAGE_NAME))
		{
			language = gtk_source_language_manager_get_language (manager, data);
		}

		g_free (data);
	}
	else
	{
		GFile *location;
		gchar *basename = NULL;

		location = gtk_source_file_get_location (priv->file);
		gcode_debug_message (DEBUG_DOCUMENT, "Sniffing Language");

		if (location != NULL)
		{
			basename = g_file_get_basename (location);
		}
		else if (priv->short_name != NULL)
		{
			basename = g_strdup (priv->short_name);
		}

		language = gtk_source_language_manager_guess_language (manager,
								       basename,
								       priv->content_type);

		g_free (basename);
	}

	return language;
}

static void
on_content_type_changed (GcodeDocument *doc,
			 GParamSpec    *pspec,
			 gpointer       useless)
{
	GcodeDocumentPrivate *priv;

	priv = gcode_document_get_instance_private (doc);

	if (!priv->language_set_by_user)
	{
		GtkSourceLanguage *language = guess_language (doc);

		gcode_debug_message (DEBUG_DOCUMENT, "Language: %s",
				     language != NULL ? gtk_source_language_get_name (language) : "None");

		set_language (doc, language, FALSE);
	}
}

static gchar *
get_default_content_type (void)
{
	return g_content_type_from_mime_type ("text/plain");
}

static void
on_location_changed (GtkSourceFile *file,
		     GParamSpec    *pspec,
		     GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;
	GFile *location;

	gcode_debug (DEBUG_DOCUMENT);

	priv = gcode_document_get_instance_private (doc);

	location = gtk_source_file_get_location (file);

	if (location != NULL && priv->untitled_number > 0)
	{
		release_untitled_number (priv->untitled_number);
		priv->untitled_number = 0;
	}

	if (priv->short_name == NULL)
	{
		g_object_notify (G_OBJECT (doc), "shortname");
	}

	/* Load metadata for this location: we load sync since metadata is
	 * always local so it should be fast and we need the information
	 * right after the location was set.
	 */
	if (priv->use_gvfs_metadata && location != NULL)
	{
		GError *error = NULL;

		if (priv->metadata_info != NULL)
		{
			g_object_unref (priv->metadata_info);
		}

		priv->metadata_info = g_file_query_info (location,
		                                         METADATA_QUERY,
		                                         G_FILE_QUERY_INFO_NONE,
		                                         NULL,
		                                         &error);

		if (error != NULL)
		{
			/* Do not complain about metadata if we are opening a
			 * non existing file.
			 */
			if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_ISDIR) &&
			    !g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOTDIR) &&
			    !g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT) &&
			    !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
			{
				g_warning ("%s", error->message);
			}

			g_error_free (error);
		}
	}
}

static void
gcode_document_init (GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;

	gcode_debug (DEBUG_DOCUMENT);

	priv = gcode_document_get_instance_private (doc);

	priv->untitled_number = get_untitled_number ();
	priv->content_type = get_default_content_type ();
	priv->readonly = FALSE;
	priv->language_set_by_user = FALSE;

	g_get_current_time (&priv->time_of_last_save_or_load);

	priv->file = gtk_source_file_new ();

	g_signal_connect_object (priv->file,
				 "notify::location",
				 G_CALLBACK (on_location_changed),
				 doc,
				 0);

	g_signal_connect (doc,
			  "notify::content-type",
			  G_CALLBACK (on_content_type_changed),
			  NULL);
}

GcodeDocument *
gcode_document_new (void)
{
	return g_object_new (GCODE_TYPE_DOCUMENT, NULL);
}

static gchar *
get_content_type_from_content (GcodeDocument *doc)
{
	gchar *content_type;
	gchar *data;
	GtkTextBuffer *buffer;
	GtkTextIter start;
	GtkTextIter end;

	buffer = GTK_TEXT_BUFFER (doc);

	gtk_text_buffer_get_start_iter (buffer, &start);
	end = start;
	gtk_text_iter_forward_chars (&end, 255);

	data = gtk_text_buffer_get_text (buffer, &start, &end, TRUE);

	content_type = g_content_type_guess (NULL,
	                                     (const guchar *)data,
	                                     strlen (data),
	                                     NULL);

	g_free (data);

	return content_type;
}

static void
set_content_type_no_guess (GcodeDocument *doc,
			   const gchar   *content_type)
{
	GcodeDocumentPrivate *priv;
	gchar *dupped_content_type;

	gcode_debug (DEBUG_DOCUMENT);

	priv = gcode_document_get_instance_private (doc);

	if (priv->content_type != NULL &&
	    content_type != NULL &&
	    g_str_equal (priv->content_type, content_type))
	{
		return;
	}

	g_free (priv->content_type);

	/* For compression types, we try to just guess from the content */
	if (gcode_utils_get_compression_type_from_content_type (content_type) !=
	    GTK_SOURCE_COMPRESSION_TYPE_NONE)
	{
		dupped_content_type = get_content_type_from_content (doc);
	}
	else
	{
		dupped_content_type = g_strdup (content_type);
	}

	if (dupped_content_type == NULL ||
	    g_content_type_is_unknown (dupped_content_type))
	{
		priv->content_type = get_default_content_type ();
		g_free (dupped_content_type);
	}
	else
	{
		priv->content_type = dupped_content_type;
	}

	g_object_notify (G_OBJECT (doc), "content-type");
}

/**
 * gcode_document_set_content_type:
 * @doc:
 * @content_type: (allow-none):
 */
void
gcode_document_set_content_type (GcodeDocument *doc,
                                 const gchar   *content_type)
{
	GcodeDocumentPrivate *priv;

	g_return_if_fail (GCODE_IS_DOCUMENT (doc));

	gcode_debug (DEBUG_DOCUMENT);

	priv = gcode_document_get_instance_private (doc);

	if (content_type == NULL)
	{
		GFile *location;
		gchar *guessed_type = NULL;

		/* If content type is null, we guess from the filename */
		location = gtk_source_file_get_location (priv->file);
		if (location != NULL)
		{
			gchar *basename;

			basename = g_file_get_basename (location);
			guessed_type = g_content_type_guess (basename, NULL, 0, NULL);

			g_free (basename);
		}

		set_content_type_no_guess (doc, guessed_type);
		g_free (guessed_type);
	}
	else
	{
		set_content_type_no_guess (doc, content_type);
	}
}

/**
 * gcode_document_get_location:
 * @doc: a #GcodeDocument
 *
 * Returns: (allow-none) (transfer full): a copy of the internal #GFile
 *
 * Deprecated: 3.14: use gtk_source_file_get_location() instead. Attention,
 * gcode_document_get_location() has a transfer full for the return value, while
 * gtk_source_file_get_location() has a transfer none.
 */
GFile *
gcode_document_get_location (GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;
	GFile *location;

	priv = gcode_document_get_instance_private (doc);

	g_return_val_if_fail (GCODE_IS_DOCUMENT (doc), NULL);

	location = gtk_source_file_get_location (priv->file);

	return location != NULL ? g_object_ref (location) : NULL;
}

/**
 * gcode_document_set_location:
 * @doc: a #GcodeDocument.
 * @location: the new location.
 *
 * Deprecated: 3.14: use gtk_source_file_set_location() instead.
 */
void
gcode_document_set_location (GcodeDocument *doc,
			     GFile         *location)
{
	GcodeDocumentPrivate *priv;

	g_return_if_fail (GCODE_IS_DOCUMENT (doc));
	g_return_if_fail (G_IS_FILE (location));

	priv = gcode_document_get_instance_private (doc);

	gtk_source_file_set_location (priv->file, location);
	gcode_document_set_content_type (doc, NULL);
}

/**
 * gcode_document_get_uri_for_display:
 * @doc: a #GcodeDocument.
 *
 * Note: this never returns %NULL.
 **/
gchar *
gcode_document_get_uri_for_display (GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;
	GFile *location;

	g_return_val_if_fail (GCODE_IS_DOCUMENT (doc), g_strdup (""));

	priv = gcode_document_get_instance_private (doc);

	location = gtk_source_file_get_location (priv->file);

	if (location == NULL)
	{
		return g_strdup_printf (_("Unsaved Document %d"),
					priv->untitled_number);
	}
	else
	{
		return g_file_get_parse_name (location);
	}
}

/**
 * gcode_document_get_short_name_for_display:
 * @doc: a #GcodeDocument.
 *
 * Note: this never returns %NULL.
 **/
gchar *
gcode_document_get_short_name_for_display (GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;
	GFile *location;

	g_return_val_if_fail (GCODE_IS_DOCUMENT (doc), g_strdup (""));

	priv = gcode_document_get_instance_private (doc);

	location = gtk_source_file_get_location (priv->file);

	if (priv->short_name != NULL)
	{
		return g_strdup (priv->short_name);
	}
	else if (location == NULL)
	{
		return g_strdup_printf (_("Unsaved Document %d"),
					priv->untitled_number);
	}
	else
	{
		return gcode_utils_basename_for_display (location);
	}
}

/**
 * gcode_document_set_short_name_for_display:
 * @doc:
 * @short_name: (allow-none):
 */
void
gcode_document_set_short_name_for_display (GcodeDocument *doc,
                                           const gchar   *short_name)
{
	GcodeDocumentPrivate *priv;

	g_return_if_fail (GCODE_IS_DOCUMENT (doc));

	priv = gcode_document_get_instance_private (doc);

	g_free (priv->short_name);
	priv->short_name = g_strdup (short_name);

	g_object_notify (G_OBJECT (doc), "shortname");
}

gchar *
gcode_document_get_content_type (GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;

	g_return_val_if_fail (GCODE_IS_DOCUMENT (doc), NULL);

	priv = gcode_document_get_instance_private (doc);

	return g_strdup (priv->content_type);
}

/**
 * gcode_document_get_mime_type:
 * @doc: a #GcodeDocument.
 *
 * Note: this never returns %NULL.
 **/
gchar *
gcode_document_get_mime_type (GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;

	g_return_val_if_fail (GCODE_IS_DOCUMENT (doc), g_strdup ("text/plain"));

	priv = gcode_document_get_instance_private (doc);

	if (priv->content_type != NULL &&
	    !g_content_type_is_unknown (priv->content_type))
	{
		return g_content_type_get_mime_type (priv->content_type);
	}

	return g_strdup ("text/plain");
}

static void
set_readonly (GcodeDocument *doc,
	      gboolean       readonly)
{
	GcodeDocumentPrivate *priv;

	gcode_debug (DEBUG_DOCUMENT);

	g_return_if_fail (GCODE_IS_DOCUMENT (doc));

	priv = gcode_document_get_instance_private (doc);

	readonly = readonly != FALSE;

	if (priv->readonly != readonly)
	{
		priv->readonly = readonly;
		g_object_notify (G_OBJECT (doc), "read-only");
	}
}

gboolean
gcode_document_get_readonly (GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;

	g_return_val_if_fail (GCODE_IS_DOCUMENT (doc), TRUE);

	priv = gcode_document_get_instance_private (doc);

	return priv->readonly;
}

static void
loaded_query_info_cb (GFile         *location,
		      GAsyncResult  *result,
		      GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;
	GFileInfo *info;
	GError *error = NULL;

	priv = gcode_document_get_instance_private (doc);

	info = g_file_query_info_finish (location, result, &error);

	if (error != NULL)
	{
		/* Ignore not found error as it can happen when opening a
		 * non-existent file from the command line.
		 */
		if (error->domain != G_IO_ERROR ||
		    error->code != G_IO_ERROR_NOT_FOUND)
		{
			g_warning ("Document loading: query info error: %s", error->message);
		}

		g_error_free (error);
		error = NULL;
	}

	if (info != NULL)
	{
		if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE))
		{
			const gchar *content_type;

			content_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);

			gcode_document_set_content_type (doc, content_type);
		}

		if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
		{
			gboolean read_only;

			read_only = !g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);

			set_readonly (doc, read_only);
		}

		if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_TIME_MODIFIED))
		{
			g_file_info_get_modification_time (info, &priv->mtime);
			priv->mtime_set = TRUE;
		}

		g_object_unref (info);
	}

	/* Async operation finished. */
	g_object_unref (doc);
}

static void
gcode_document_loaded_real (GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;
	GFile *location;

	priv = gcode_document_get_instance_private (doc);

	if (!priv->language_set_by_user)
	{
		GtkSourceLanguage *language = guess_language (doc);

		gcode_debug_message (DEBUG_DOCUMENT, "Language: %s",
				     language != NULL ? gtk_source_language_get_name (language) : "None");

		set_language (doc, language, FALSE);
	}

	priv->mtime_set = FALSE;
	priv->externally_modified = FALSE;
	priv->deleted = FALSE;

	g_get_current_time (&priv->time_of_last_save_or_load);

	set_readonly (doc, FALSE);

	gcode_document_set_content_type (doc, NULL);

	location = gtk_source_file_get_location (priv->file);

	if (location != NULL)
	{
		/* Keep the doc alive during the async operation. */
		g_object_ref (doc);

		g_file_query_info_async (location,
					 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
					 G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE ","
					 G_FILE_ATTRIBUTE_TIME_MODIFIED,
					 G_FILE_QUERY_INFO_NONE,
					 G_PRIORITY_DEFAULT,
					 NULL,
					 (GAsyncReadyCallback) loaded_query_info_cb,
					 doc);
	}
}

static void
saved_query_info_cb (GFile         *location,
		     GAsyncResult  *result,
		     GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;
	GFileInfo *info;
	const gchar *content_type = NULL;
	GError *error = NULL;

	priv = gcode_document_get_instance_private (doc);

	info = g_file_query_info_finish (location, result, &error);

	if (error != NULL)
	{
		g_warning ("Document saving: query info error: %s", error->message);
		g_error_free (error);
		error = NULL;
	}

	priv->mtime_set = FALSE;

	if (info != NULL)
	{
		if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE))
		{
			content_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
		}

		if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_TIME_MODIFIED))
		{
			g_file_info_get_modification_time (info, &priv->mtime);
			priv->mtime_set = TRUE;
		}
	}

	gcode_document_set_content_type (doc, content_type);

	if (info != NULL)
	{
		/* content_type (owned by info) is no longer needed. */
		g_object_unref (info);
	}

	g_get_current_time (&priv->time_of_last_save_or_load);

	priv->externally_modified = FALSE;
	priv->deleted = FALSE;
	priv->create = FALSE;

	set_readonly (doc, FALSE);

	gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (doc), FALSE);

	save_encoding_metadata (doc);

	/* Async operation finished. */
	g_object_unref (doc);
}

static void
gcode_document_saved_real (GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;
	GFile *location;

	priv = gcode_document_get_instance_private (doc);

	location = gtk_source_file_get_location (priv->file);

	/* Keep the doc alive during the async operation. */
	g_object_ref (doc);

	g_file_query_info_async (location,
				 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
				 G_FILE_ATTRIBUTE_TIME_MODIFIED,
				 G_FILE_QUERY_INFO_NONE,
				 G_PRIORITY_DEFAULT,
				 NULL,
				 (GAsyncReadyCallback) saved_query_info_cb,
				 doc);
}

gboolean
gcode_document_is_untouched (GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;
	GFile *location;

	g_return_val_if_fail (GCODE_IS_DOCUMENT (doc), TRUE);

	priv = gcode_document_get_instance_private (doc);

	location = gtk_source_file_get_location (priv->file);

	return location == NULL && !gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (doc));
}

gboolean
gcode_document_is_untitled (GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;

	g_return_val_if_fail (GCODE_IS_DOCUMENT (doc), TRUE);

	priv = gcode_document_get_instance_private (doc);

	return gtk_source_file_get_location (priv->file) == NULL;
}

gboolean
gcode_document_is_local (GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;
	GFile *location;

	g_return_val_if_fail (GCODE_IS_DOCUMENT (doc), FALSE);

	priv = gcode_document_get_instance_private (doc);

	location = gtk_source_file_get_location (priv->file);

	if (location == NULL)
	{
		return FALSE;
	}

	return g_file_has_uri_scheme (location, "file");
}

static void
check_file_on_disk (GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;
	GFile *location;
	GFileInfo *info;

	priv = gcode_document_get_instance_private (doc);

	location = gtk_source_file_get_location (priv->file);

	if (location == NULL)
	{
		return;
	}

	info = g_file_query_info (location,
				  G_FILE_ATTRIBUTE_TIME_MODIFIED "," \
				  G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
				  G_FILE_QUERY_INFO_NONE,
				  NULL, NULL);

	if (info != NULL)
	{
		/* While at it also check if permissions changed */
		if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
		{
			gboolean read_only;

			read_only = !g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);

			set_readonly (doc, read_only);
		}

		if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_TIME_MODIFIED) && priv->mtime_set)
		{
			GTimeVal timeval;

			g_file_info_get_modification_time (info, &timeval);

			/* Note that mtime can even go backwards if the
			 * user is copying over a file with an old mtime
			 */
			if (timeval.tv_sec != priv->mtime.tv_sec ||
			    timeval.tv_usec != priv->mtime.tv_usec)
			{
				priv->externally_modified = TRUE;
			}
		}

		g_object_unref (info);
	}
	else
	{
		priv->deleted = TRUE;
	}
}

gboolean
_gcode_document_check_externally_modified (GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;

	g_return_val_if_fail (GCODE_IS_DOCUMENT (doc), FALSE);

	priv = gcode_document_get_instance_private (doc);

	if (!priv->externally_modified)
	{
		check_file_on_disk (doc);
	}

	return priv->externally_modified;
}

gboolean
gcode_document_get_deleted (GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;

	g_return_val_if_fail (GCODE_IS_DOCUMENT (doc), FALSE);

	priv = gcode_document_get_instance_private (doc);

	if (!priv->deleted)
	{
		check_file_on_disk (doc);
	}

	return priv->deleted;
}

/*
 * Deletion and external modification is only checked for local files.
 */
gboolean
_gcode_document_needs_saving (GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;

	g_return_val_if_fail (GCODE_IS_DOCUMENT (doc), FALSE);

	priv = gcode_document_get_instance_private (doc);

	if (gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (doc)))
	{
		return TRUE;
	}

	if (gcode_document_is_local (doc))
	{
		check_file_on_disk (doc);
	}

	return (priv->externally_modified || priv->deleted) && !priv->create;
}

/* If @line is bigger than the lines of the document, the cursor is moved
 * to the last line and FALSE is returned.
 */
gboolean
gcode_document_goto_line (GcodeDocument *doc,
			  gint           line)
{
	gboolean ret = TRUE;
	guint line_count;
	GtkTextIter iter;

	gcode_debug (DEBUG_DOCUMENT);

	g_return_val_if_fail (GCODE_IS_DOCUMENT (doc), FALSE);
	g_return_val_if_fail (line >= -1, FALSE);

	line_count = gtk_text_buffer_get_line_count (GTK_TEXT_BUFFER (doc));

	if (line >= line_count)
	{
		ret = FALSE;
		gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (doc),
					      &iter);
	}
	else
	{
		gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (doc),
						  &iter,
						  line);
	}

	gtk_text_buffer_place_cursor (GTK_TEXT_BUFFER (doc), &iter);

	return ret;
}

gboolean
gcode_document_goto_line_offset (GcodeDocument *doc,
				 gint           line,
				 gint           line_offset)
{
	gboolean ret;
	GtkTextIter iter;

	g_return_val_if_fail (GCODE_IS_DOCUMENT (doc), FALSE);
	g_return_val_if_fail (line >= -1, FALSE);
	g_return_val_if_fail (line_offset >= -1, FALSE);

	ret = gcode_document_goto_line (doc, line);

	if (ret)
	{
		guint offset_count;

		gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (doc),
						  &iter,
						  line);

		offset_count = gtk_text_iter_get_chars_in_line (&iter);
		if (line_offset > offset_count)
		{
			ret = FALSE;
		}
		else
		{
			gtk_text_iter_set_line_offset (&iter, line_offset);
			gtk_text_buffer_place_cursor (GTK_TEXT_BUFFER (doc), &iter);
		}
	}

	return ret;
}

/**
 * gcode_document_set_language:
 * @doc:
 * @lang: (allow-none):
 **/
void
gcode_document_set_language (GcodeDocument     *doc,
			     GtkSourceLanguage *lang)
{
	g_return_if_fail (GCODE_IS_DOCUMENT (doc));

	set_language (doc, lang, TRUE);
}

/**
 * gcode_document_get_language:
 * @doc:
 *
 * Return value: (transfer none):
 */
GtkSourceLanguage *
gcode_document_get_language (GcodeDocument *doc)
{
	g_return_val_if_fail (GCODE_IS_DOCUMENT (doc), NULL);

	return gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (doc));
}

/**
 * gcode_document_get_encoding:
 * @doc: a #GcodeDocument.
 *
 * Returns: the encoding.
 * Deprecated: 3.14: use gtk_source_file_get_encoding() instead.
 */
const GtkSourceEncoding *
gcode_document_get_encoding (GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;

	g_return_val_if_fail (GCODE_IS_DOCUMENT (doc), NULL);

	priv = gcode_document_get_instance_private (doc);

	return gtk_source_file_get_encoding (priv->file);
}

glong
_gcode_document_get_seconds_since_last_save_or_load (GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;
	GTimeVal current_time;
	gcode_debug (DEBUG_DOCUMENT);

	g_return_val_if_fail (GCODE_IS_DOCUMENT (doc), -1);

	priv = gcode_document_get_instance_private (doc);

	g_get_current_time (&current_time);

	return (current_time.tv_sec - priv->time_of_last_save_or_load.tv_sec);
}

/**
 * gcode_document_get_newline_type:
 * @doc: a #GcodeDocument.
 *
 * Returns: the newline type.
 * Deprecated: 3.14: use gtk_source_file_get_newline_type() instead.
 */
GtkSourceNewlineType
gcode_document_get_newline_type (GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;

	g_return_val_if_fail (GCODE_IS_DOCUMENT (doc), 0);

	priv = gcode_document_get_instance_private (doc);

	return gtk_source_file_get_newline_type (priv->file);
}

/**
 * gcode_document_get_compression_type:
 * @doc: a #GcodeDocument.
 *
 * Returns: the compression type.
 * Deprecated: 3.14: use gtk_source_file_get_compression_type() instead.
 */
GtkSourceCompressionType
gcode_document_get_compression_type (GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;

	g_return_val_if_fail (GCODE_IS_DOCUMENT (doc), 0);

	priv = gcode_document_get_instance_private (doc);

	return gtk_source_file_get_compression_type (priv->file);
}

static gchar *
get_metadata_from_metadata_manager (GcodeDocument *doc,
				    const gchar   *key)
{
	GcodeDocumentPrivate *priv;
	GFile *location;

	priv = gcode_document_get_instance_private (doc);

	location = gtk_source_file_get_location (priv->file);

	if (location != NULL)
	{
		return gcode_metadata_manager_get (location, key);
	}

	return NULL;
}

static gchar *
get_metadata_from_gvfs (GcodeDocument *doc,
			const gchar   *key)
{
	GcodeDocumentPrivate *priv;

	priv = gcode_document_get_instance_private (doc);

	if (priv->metadata_info != NULL &&
	    g_file_info_has_attribute (priv->metadata_info, key))
	{
		return g_strdup (g_file_info_get_attribute_string (priv->metadata_info, key));
	}

	return NULL;
}

static void
set_gvfs_metadata (GcodeDocument *doc,
		   GFileInfo     *info,
		   const gchar   *key,
		   const gchar   *value)
{
	GcodeDocumentPrivate *priv;

	priv = gcode_document_get_instance_private (doc);

	if (value != NULL)
	{
		g_file_info_set_attribute_string (info, key, value);

		if (priv->metadata_info != NULL)
		{
			g_file_info_set_attribute_string (priv->metadata_info, key, value);
		}
	}
	else
	{
		/* Unset the key */
		g_file_info_remove_attribute (info, key);

		if (priv->metadata_info != NULL)
		{
			g_file_info_remove_attribute (priv->metadata_info, key);
		}
	}
}

/**
 * gcode_document_get_metadata:
 * @doc: a #GcodeDocument
 * @key: name of the key
 *
 * Gets the metadata assigned to @key.
 *
 * Returns: the value assigned to @key. Free with g_free().
 */
gchar *
gcode_document_get_metadata (GcodeDocument *doc,
			     const gchar   *key)
{
	GcodeDocumentPrivate *priv;

	g_return_val_if_fail (GCODE_IS_DOCUMENT (doc), NULL);
	g_return_val_if_fail (key != NULL, NULL);

	priv = gcode_document_get_instance_private (doc);

	if (priv->use_gvfs_metadata)
	{
		return get_metadata_from_gvfs (doc, key);
	}

	return get_metadata_from_metadata_manager (doc, key);
}

/**
 * gcode_document_set_metadata:
 * @doc: a #GcodeDocument
 * @first_key: name of the first key to set
 * @...: (allow-none): value for the first key, followed optionally by more key/value pairs,
 * followed by %NULL.
 *
 * Sets metadata on a document.
 */
void
gcode_document_set_metadata (GcodeDocument *doc,
			     const gchar   *first_key,
			     ...)
{
	GcodeDocumentPrivate *priv;
	GFile *location;
	const gchar *key;
	va_list var_args;
	GFileInfo *info = NULL;

	g_return_if_fail (GCODE_IS_DOCUMENT (doc));
	g_return_if_fail (first_key != NULL);

	priv = gcode_document_get_instance_private (doc);

	location = gtk_source_file_get_location (priv->file);

	/* With the metadata manager, can't set metadata for untitled documents.
	 * With GVFS metadata, if the location is NULL the metadata is stored in
	 * priv->metadata_info, so that it can be saved later if the document is
	 * saved.
	 */
	if (!priv->use_gvfs_metadata && location == NULL)
	{
		return;
	}

	if (priv->use_gvfs_metadata)
	{
		info = g_file_info_new ();
	}

	va_start (var_args, first_key);

	for (key = first_key; key; key = va_arg (var_args, const gchar *))
	{
		const gchar *value = va_arg (var_args, const gchar *);

		if (priv->use_gvfs_metadata)
		{
			/* Collect the metadata into @info. */
			set_gvfs_metadata (doc, info, key, value);
		}
		else
		{
			gcode_metadata_manager_set (location, key, value);
		}
	}

	va_end (var_args);

	if (priv->use_gvfs_metadata && location != NULL)
	{
		GError *error = NULL;

		/* We save synchronously since metadata is always local so it
		 * should be fast. Moreover this function can be called on
		 * application shutdown, when the main loop has already exited,
		 * so an async operation would not terminate.
		 * https://bugzilla.gnome.org/show_bug.cgi?id=736591
		 */
		g_file_set_attributes_from_info (location,
						 info,
						 G_FILE_QUERY_INFO_NONE,
						 NULL,
						 &error);

		if (error != NULL)
		{
			/* Do not complain about metadata if we are closing a
			 * document for a non existing file.
			 */
			if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT) &&
			    !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
			{
				g_warning ("Set document metadata failed: %s", error->message);
			}

			g_error_free (error);
		}
	}

	g_clear_object (&info);
}

/**
 * gcode_document_get_file:
 * @doc: a #GcodeDocument.
 *
 * Gets the associated #GtkSourceFile. You should use it only for reading
 * purposes, not for creating a #GtkSourceFileLoader or #GtkSourceFileSaver,
 * because gcode does some extra work when loading or saving a file and
 * maintains an internal state. If you use in a plugin a file loader or saver on
 * the returned #GtkSourceFile, the internal state of gcode won't be updated.
 *
 * If you want to save the #GcodeDocument to a secondary file, you can create a
 * new #GtkSourceFile and use a #GtkSourceFileSaver.
 *
 * Returns: (transfer none): the associated #GtkSourceFile.
 * Since: 3.14
 */
GtkSourceFile *
gcode_document_get_file (GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;

	g_return_val_if_fail (GCODE_IS_DOCUMENT (doc), NULL);

	priv = gcode_document_get_instance_private (doc);

	return priv->file;
}

void
_gcode_document_set_create (GcodeDocument *doc,
			    gboolean       create)
{
	GcodeDocumentPrivate *priv;

	g_return_if_fail (GCODE_IS_DOCUMENT (doc));

	priv = gcode_document_get_instance_private (doc);

	priv->create = create != FALSE;
}

gboolean
_gcode_document_get_create (GcodeDocument *doc)
{
	GcodeDocumentPrivate *priv;

	g_return_val_if_fail (GCODE_IS_DOCUMENT (doc), FALSE);

	priv = gcode_document_get_instance_private (doc);

	return priv->create;
}

/* ex:set ts=8 noet: */
