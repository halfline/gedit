/*
 * gedit-document.c
 * This file is part of gedit
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi
 * Copyright (C) 2002-2005 Paolo Maggi
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
 */

/*
 * Modified by the gedit Team, 1998-2005. See the AUTHORS file for a
 * list of people on the gedit Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>

#include "gedit-settings.h"
#include "gedit-document.h"
#include "gedit-debug.h"
#include "gedit/gedit-utils.h"
#include "gedit-document-loader.h"
#include "gedit-document-saver.h"
#include "gedit-marshal.h"
#include "gedit-enum-types.h"

#ifndef ENABLE_GVFS_METADATA
#include "gedit/gedit-metadata-manager.h"
#else
#define METADATA_QUERY "metadata::*"
#endif

#undef ENABLE_PROFILE

#ifdef ENABLE_PROFILE
#define PROFILE(x) x
#else
#define PROFILE(x)
#endif

PROFILE (static GTimer *timer = NULL)

#ifdef MAXPATHLEN
#define GEDIT_MAX_PATH_LEN  MAXPATHLEN
#elif defined (PATH_MAX)
#define GEDIT_MAX_PATH_LEN  PATH_MAX
#else
#define GEDIT_MAX_PATH_LEN  2048
#endif

static void	gedit_document_load_real	(GeditDocument                *doc,
						 GFile                        *location,
						 const GeditEncoding          *encoding,
						 gint                          line_pos,
						 gint                          column_pos,
						 gboolean                      create);
static void	gedit_document_save_real	(GeditDocument                *doc,
						 GFile                        *location,
						 const GeditEncoding          *encoding,
						 GeditDocumentNewlineType      newline_type,
						 GeditDocumentCompressionType  compression_type,
						 GeditDocumentSaveFlags        flags);

struct _GeditDocumentPrivate
{
	GSettings   *editor_settings;

	GFile       *location;

	gint 	     untitled_number;
	gchar       *short_name;

	GFileInfo   *metadata_info;

	const GeditEncoding *encoding;

	gchar	    *content_type;

	GTimeVal     mtime;
	GTimeVal     time_of_last_save_or_load;

	/* The search context for the incremental search, or the search and
	 * replace. They are mutually exclusive.
	 */
	GtkSourceSearchContext *search_context;

	GtkSourceSearchSettings *deprecated_search_settings;

	GeditDocumentNewlineType newline_type;
	GeditDocumentCompressionType compression_type;

	/* Temp data while loading */
	GeditDocumentLoader *loader;
	gboolean             create; /* Create file if uri points
	                              * to a non existing file */
	const GeditEncoding *requested_encoding;
	gint                 requested_line_pos;
	gint                 requested_column_pos;

	/* Saving stuff */
	GeditDocumentSaver *saver;

	GtkTextTag *error_tag;

	/* Mount operation factory */
	GeditMountOperationFactory  mount_operation_factory;
	gpointer		    mount_operation_userdata;

	guint readonly : 1;
	guint externally_modified : 1;
	guint deleted : 1;
	guint last_save_was_manually : 1;
	guint language_set_by_user : 1;
	guint stop_cursor_moved_emission : 1;
	guint dispose_has_run : 1;

	/* The search is empty if there is no search context, or if the
	 * search text is empty. It is used for the sensitivity of some menu
	 * actions.
	 */
	guint empty_search : 1;
};

enum {
	PROP_0,

	PROP_LOCATION,
	PROP_SHORTNAME,
	PROP_CONTENT_TYPE,
	PROP_MIME_TYPE,
	PROP_READ_ONLY,
	PROP_ENCODING,
	PROP_CAN_SEARCH_AGAIN,
	PROP_ENABLE_SEARCH_HIGHLIGHTING,
	PROP_NEWLINE_TYPE,
	PROP_COMPRESSION_TYPE,
	PROP_EMPTY_SEARCH
};

enum {
	CURSOR_MOVED,
	LOAD,
	LOADING,
	LOADED,
	SAVE,
	SAVING,
	SAVED,
	SEARCH_HIGHLIGHT_UPDATED,
	LAST_SIGNAL
};

static guint document_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GeditDocument, gedit_document, GTK_SOURCE_TYPE_BUFFER)

GQuark
gedit_document_error_quark (void)
{
	static GQuark quark = 0;

	if (G_UNLIKELY (quark == 0))
		quark = g_quark_from_static_string ("gedit_io_load_error");

	return quark;
}

static GHashTable *allocated_untitled_numbers = NULL;

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

static void
set_newline_type (GeditDocument           *doc,
		  GeditDocumentNewlineType newline_type)
{
	if (doc->priv->newline_type != newline_type)
	{
		doc->priv->newline_type = newline_type;

		g_object_notify (G_OBJECT (doc), "newline-type");
	}
}

static void
set_compression_type (GeditDocument *doc,
                      GeditDocumentCompressionType compression_type)
{
	if (doc->priv->compression_type != compression_type)
	{
		doc->priv->compression_type = compression_type;

		g_object_notify (G_OBJECT (doc), "compression-type");
	}
}

static void
gedit_document_dispose (GObject *object)
{
	GeditDocument *doc = GEDIT_DOCUMENT (object);

	gedit_debug (DEBUG_DOCUMENT);

	/* Metadata must be saved here and not in finalize
	 * because the language is gone by the time finalize runs.
	 * beside if some plugin prevents proper finalization by
	 * holding a ref to the doc, we still save the metadata */
	if ((!doc->priv->dispose_has_run) && (doc->priv->location != NULL))
	{
		GtkTextIter iter;
		gchar *position;
		const gchar *language = NULL;

		if (doc->priv->language_set_by_user)
		{
			GtkSourceLanguage *lang;

			lang = gedit_document_get_language (doc);

			if (lang == NULL)
				language = "_NORMAL_";
			else
				language = gtk_source_language_get_id (lang);
		}

		gtk_text_buffer_get_iter_at_mark (
				GTK_TEXT_BUFFER (doc),
				&iter,
				gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (doc)));

		position = g_strdup_printf ("%d",
					    gtk_text_iter_get_offset (&iter));

		if (language == NULL)
		{
			gedit_document_set_metadata (doc, GEDIT_METADATA_ATTRIBUTE_POSITION,
						     position, NULL);
		}
		else
		{
			gedit_document_set_metadata (doc, GEDIT_METADATA_ATTRIBUTE_POSITION,
						     position, GEDIT_METADATA_ATTRIBUTE_LANGUAGE,
						     language, NULL);
		}

		g_free (position);
	}

	g_clear_object (&doc->priv->loader);
	g_clear_object (&doc->priv->editor_settings);
	g_clear_object (&doc->priv->metadata_info);
	g_clear_object (&doc->priv->location);
	g_clear_object (&doc->priv->search_context);
	g_clear_object (&doc->priv->deprecated_search_settings);

	doc->priv->dispose_has_run = TRUE;

	G_OBJECT_CLASS (gedit_document_parent_class)->dispose (object);
}

static void
gedit_document_finalize (GObject *object)
{
	GeditDocument *doc = GEDIT_DOCUMENT (object);

	gedit_debug (DEBUG_DOCUMENT);

	if (doc->priv->untitled_number > 0)
	{
		release_untitled_number (doc->priv->untitled_number);
	}

	g_free (doc->priv->content_type);

	G_OBJECT_CLASS (gedit_document_parent_class)->finalize (object);
}

static void
gedit_document_get_property (GObject    *object,
			     guint       prop_id,
			     GValue     *value,
			     GParamSpec *pspec)
{
	GeditDocument *doc = GEDIT_DOCUMENT (object);

	switch (prop_id)
	{
		case PROP_LOCATION:
			g_value_set_object (value, doc->priv->location);
			break;
		case PROP_SHORTNAME:
			g_value_take_string (value, gedit_document_get_short_name_for_display (doc));
			break;
		case PROP_CONTENT_TYPE:
			g_value_take_string (value, gedit_document_get_content_type (doc));
			break;
		case PROP_MIME_TYPE:
			g_value_take_string (value, gedit_document_get_mime_type (doc));
			break;
		case PROP_READ_ONLY:
			g_value_set_boolean (value, doc->priv->readonly);
			break;
		case PROP_ENCODING:
			g_value_set_boxed (value, doc->priv->encoding);
			break;
		case PROP_CAN_SEARCH_AGAIN:
			G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
			g_value_set_boolean (value, gedit_document_get_can_search_again (doc));
			G_GNUC_END_IGNORE_DEPRECATIONS;
			break;
		case PROP_ENABLE_SEARCH_HIGHLIGHTING:
			g_value_set_boolean (value,
					     doc->priv->search_context != NULL &&
					     gtk_source_search_context_get_highlight (doc->priv->search_context));
			break;
		case PROP_NEWLINE_TYPE:
			g_value_set_enum (value, doc->priv->newline_type);
			break;
		case PROP_COMPRESSION_TYPE:
			g_value_set_enum (value, doc->priv->compression_type);
			break;
		case PROP_EMPTY_SEARCH:
			g_value_set_boolean (value, doc->priv->empty_search);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_document_set_property (GObject      *object,
			     guint         prop_id,
			     const GValue *value,
			     GParamSpec   *pspec)
{
	GeditDocument *doc = GEDIT_DOCUMENT (object);

	switch (prop_id)
	{
		case PROP_LOCATION:
		{
			GFile *location;

			location = g_value_get_object (value);

			if (location != NULL)
			{
				gedit_document_set_location (doc, location);
			}

			break;
		}
		case PROP_SHORTNAME:
			gedit_document_set_short_name_for_display (doc,
			                                           g_value_get_string (value));
			break;
		case PROP_CONTENT_TYPE:
			gedit_document_set_content_type (doc,
			                                 g_value_get_string (value));
			break;
		case PROP_ENABLE_SEARCH_HIGHLIGHTING:
			if (doc->priv->search_context != NULL)
			{
				gtk_source_search_context_set_highlight (doc->priv->search_context,
									 g_value_get_boolean (value));
			}
			break;
		case PROP_NEWLINE_TYPE:
			set_newline_type (doc,
					  g_value_get_enum (value));
			break;
		case PROP_COMPRESSION_TYPE:
			set_compression_type (doc,
			                      g_value_get_enum (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
emit_cursor_moved (GeditDocument *doc)
{
	if (!doc->priv->stop_cursor_moved_emission)
	{
		g_signal_emit (doc,
			       document_signals[CURSOR_MOVED],
			       0);
	}
}

static void
gedit_document_mark_set (GtkTextBuffer     *buffer,
                         const GtkTextIter *iter,
                         GtkTextMark       *mark)
{
	GeditDocument *doc = GEDIT_DOCUMENT (buffer);

	if (GTK_TEXT_BUFFER_CLASS (gedit_document_parent_class)->mark_set)
	{
		GTK_TEXT_BUFFER_CLASS (gedit_document_parent_class)->mark_set (buffer,
									       iter,
									       mark);
	}

	if (mark == gtk_text_buffer_get_insert (buffer))
	{
		emit_cursor_moved (doc);
	}
}

static void
gedit_document_changed (GtkTextBuffer *buffer)
{
	emit_cursor_moved (GEDIT_DOCUMENT (buffer));

	GTK_TEXT_BUFFER_CLASS (gedit_document_parent_class)->changed (buffer);
}

static void
gedit_document_class_init (GeditDocumentClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkTextBufferClass *buf_class = GTK_TEXT_BUFFER_CLASS (klass);

	object_class->dispose = gedit_document_dispose;
	object_class->finalize = gedit_document_finalize;
	object_class->get_property = gedit_document_get_property;
	object_class->set_property = gedit_document_set_property;

	buf_class->mark_set = gedit_document_mark_set;
	buf_class->changed = gedit_document_changed;

	klass->load = gedit_document_load_real;
	klass->save = gedit_document_save_real;

	g_object_class_install_property (object_class, PROP_LOCATION,
					 g_param_spec_object ("location",
							      "Location",
							      "The document's location",
							      G_TYPE_FILE,
							      G_PARAM_READWRITE |
							      G_PARAM_STATIC_STRINGS));

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

	g_object_class_install_property (object_class, PROP_ENCODING,
					 g_param_spec_boxed ("encoding",
							     "Encoding",
							     "The GeditEncoding used for the document",
							     GEDIT_TYPE_ENCODING,
							     G_PARAM_READABLE |
							     G_PARAM_STATIC_STRINGS));

	/**
	 * GeditDocument:can-search-again:
	 *
	 * Deprecated: 3.10: Use the search and replace API of GtkSourceView
	 * instead.
	 */
	g_object_class_install_property (object_class, PROP_CAN_SEARCH_AGAIN,
					 g_param_spec_boolean ("can-search-again",
							       "Can search again",
							       "Wheter it's possible to search again in the document",
							       FALSE,
							       G_PARAM_READABLE |
							       G_PARAM_STATIC_STRINGS |
							       G_PARAM_DEPRECATED));

	/**
	 * GeditDocument:enable-search-highlighting:
	 *
	 * Deprecated: 3.10: Use the search and replace API of GtkSourceView
	 * instead.
	 */
	g_object_class_install_property (object_class, PROP_ENABLE_SEARCH_HIGHLIGHTING,
					 g_param_spec_boolean ("enable-search-highlighting",
							       "Enable Search Highlighting",
							       "Whether all the occurences of the searched string must be highlighted",
							       FALSE,
							       G_PARAM_READWRITE |
							       G_PARAM_STATIC_STRINGS |
							       G_PARAM_DEPRECATED));

	/**
	 * GeditDocument:newline-type:
	 *
	 * The :newline-type property determines what is considered
	 * as a line ending when saving the document
	 */
	g_object_class_install_property (object_class, PROP_NEWLINE_TYPE,
	                                 g_param_spec_enum ("newline-type",
	                                                    "Newline type",
	                                                    "The accepted types of line ending",
	                                                    GEDIT_TYPE_DOCUMENT_NEWLINE_TYPE,
	                                                    GEDIT_DOCUMENT_NEWLINE_TYPE_LF,
	                                                    G_PARAM_READWRITE |
	                                                    G_PARAM_CONSTRUCT |
	                                                    G_PARAM_STATIC_NAME |
	                                                    G_PARAM_STATIC_BLURB));

	/**
	 * GeditDocument:compression-type:
	 *
	 * The :compression-type property determines how to compress the
	 * document when saving
	 */
	g_object_class_install_property (object_class, PROP_COMPRESSION_TYPE,
	                                 g_param_spec_enum ("compression-type",
	                                                    "Compression type",
	                                                    "The save compression type",
	                                                    GEDIT_TYPE_DOCUMENT_COMPRESSION_TYPE,
	                                                    GEDIT_DOCUMENT_COMPRESSION_TYPE_NONE,
	                                                    G_PARAM_READWRITE |
	                                                    G_PARAM_CONSTRUCT |
	                                                    G_PARAM_STATIC_NAME |
	                                                    G_PARAM_STATIC_BLURB));

	/**
	 * GeditDocument:empty-search:
	 *
	 * <warning>
	 * The property is used internally by gedit. It must not be used in a
	 * gedit plugin. The property can be modified or removed at any time.
	 * </warning>
	 */
	g_object_class_install_property (object_class, PROP_EMPTY_SEARCH,
					 g_param_spec_boolean ("empty-search",
							       "Empty search",
							       "Whether the search is empty",
							       TRUE,
							       G_PARAM_READABLE |
							       G_PARAM_STATIC_STRINGS));

	/* This signal is used to update the cursor position is the statusbar,
	 * it's emitted either when the insert mark is moved explicitely or
	 * when the buffer changes (insert/delete).
	 * We prevent the emission of the signal during replace_all to
	 * improve performance.
	 */
	document_signals[CURSOR_MOVED] =
		g_signal_new ("cursor-moved",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GeditDocumentClass, cursor_moved),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	/**
	 * GeditDocument::load:
	 * @document: the #GeditDocument.
	 * @location: the location where to load the document from.
	 * @encoding: the #GeditEncoding to encode the document.
	 * @line_pos: the line to show.
	 * @column_pos: the column to show.
	 * @create: whether the document should be created if it doesn't exist.
	 *
	 * The "load" signal is emitted when a document is loaded.
	 *
	 * Since: 2.22
	 */
	document_signals[LOAD] =
		g_signal_new ("load",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GeditDocumentClass, load),
			      NULL, NULL,
			      gedit_marshal_VOID__OBJECT_BOXED_INT_BOOLEAN,
			      G_TYPE_NONE,
			      4,
			      G_TYPE_FILE,
			      /* we rely on the fact that the GeditEncoding pointer stays
			       * the same forever */
			      GEDIT_TYPE_ENCODING | G_SIGNAL_TYPE_STATIC_SCOPE,
			      G_TYPE_INT,
			      G_TYPE_INT,
			      G_TYPE_BOOLEAN);


	document_signals[LOADING] =
		g_signal_new ("loading",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GeditDocumentClass, loading),
			      NULL, NULL,
			      gedit_marshal_VOID__UINT64_UINT64,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_UINT64,
			      G_TYPE_UINT64);

	document_signals[LOADED] =
		g_signal_new ("loaded",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GeditDocumentClass, loaded),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__BOXED,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_ERROR);

	/**
	 * GeditDocument::save:
	 * @document: the #GeditDocument.
	 * @location: the location where the document is about to be saved.
	 * @encoding: the #GeditEncoding used to save the document.
	 * @newline_type: the #GeditDocumentNewlineType used to save the document.
	 * @compression_type: the #GeditDocumentCompressionType used to save the document.
	 * @flags: the #GeditDocumentSaveFlags for the save operation.
	 *
	 * The "save" signal is emitted when the document is saved.
	 *
	 * Since: 2.20
	 */
	document_signals[SAVE] =
		g_signal_new ("save",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GeditDocumentClass, save),
			      NULL, NULL,
			      gedit_marshal_VOID__OBJECT_BOXED_ENUM_ENUM_FLAGS,
			      G_TYPE_NONE,
			      5,
			      G_TYPE_FILE,
			      /* we rely on the fact that the GeditEncoding pointer stays
			       * the same forever */
			      GEDIT_TYPE_ENCODING | G_SIGNAL_TYPE_STATIC_SCOPE,
			      GEDIT_TYPE_DOCUMENT_NEWLINE_TYPE,
			      GEDIT_TYPE_DOCUMENT_COMPRESSION_TYPE,
			      GEDIT_TYPE_DOCUMENT_SAVE_FLAGS);

	document_signals[SAVING] =
		g_signal_new ("saving",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GeditDocumentClass, saving),
			      NULL, NULL,
			      gedit_marshal_VOID__UINT64_UINT64,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_UINT64,
			      G_TYPE_UINT64);

	document_signals[SAVED] =
		g_signal_new ("saved",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GeditDocumentClass, saved),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__BOXED,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_ERROR);

	/**
	 * GeditDocument::search-highlight-updated:
	 * @document:
	 * @start:
	 * @end:
	 * @user_data:
	 *
	 * Deprecated: 3.10: Use the #GtkSourceBuffer::highlight-updated signal instead.
	 */
	document_signals[SEARCH_HIGHLIGHT_UPDATED] =
		g_signal_new ("search-highlight-updated",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_DEPRECATED,
			      G_STRUCT_OFFSET (GeditDocumentClass, search_highlight_updated),
			      NULL, NULL,
			      gedit_marshal_VOID__BOXED_BOXED,
			      G_TYPE_NONE,
			      2,
			      GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE,
			      GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
set_language (GeditDocument     *doc,
              GtkSourceLanguage *lang,
              gboolean           set_by_user)
{
	GtkSourceLanguage *old_lang;

	gedit_debug (DEBUG_DOCUMENT);

	old_lang = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (doc));

	if (old_lang == lang)
		return;

	gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (doc), lang);

	if (lang != NULL)
	{
		gboolean syntax_hl;

		syntax_hl = g_settings_get_boolean (doc->priv->editor_settings,
						    GEDIT_SETTINGS_SYNTAX_HIGHLIGHTING);

		gtk_source_buffer_set_highlight_syntax (GTK_SOURCE_BUFFER (doc),
							syntax_hl);
	}
	else
	{
		gtk_source_buffer_set_highlight_syntax (GTK_SOURCE_BUFFER (doc),
							FALSE);
	}

	if (set_by_user)
	{
		gedit_document_set_metadata (doc, GEDIT_METADATA_ATTRIBUTE_LANGUAGE,
			(lang == NULL) ? "_NORMAL_" : gtk_source_language_get_id (lang),
			NULL);
	}

	doc->priv->language_set_by_user = set_by_user;
}

static void
set_encoding (GeditDocument       *doc,
	      const GeditEncoding *encoding,
	      gboolean             set_by_user)
{
	g_return_if_fail (encoding != NULL);

	gedit_debug (DEBUG_DOCUMENT);

	if (doc->priv->encoding == encoding)
		return;

	doc->priv->encoding = encoding;

	if (set_by_user)
	{
		const gchar *charset;

		charset = gedit_encoding_get_charset (encoding);

		gedit_document_set_metadata (doc, GEDIT_METADATA_ATTRIBUTE_ENCODING,
					     charset, NULL);
	}

	g_object_notify (G_OBJECT (doc), "encoding");
}

static GtkSourceStyleScheme *
get_default_style_scheme (GSettings *editor_settings)
{
	GtkSourceStyleSchemeManager *manager;
	gchar *scheme_id;
	GtkSourceStyleScheme *def_style;

	manager = gtk_source_style_scheme_manager_get_default ();
	scheme_id = g_settings_get_string (editor_settings, GEDIT_SETTINGS_SCHEME);
	def_style = gtk_source_style_scheme_manager_get_scheme (manager, scheme_id);
	if (def_style == NULL)
	{
		g_warning ("Default style scheme '%s' cannot be found, falling back to 'classic' style scheme ", scheme_id);

		def_style = gtk_source_style_scheme_manager_get_scheme (manager, "classic");
		if (def_style == NULL)
		{
			g_warning ("Style scheme 'classic' cannot be found, check your GtkSourceView installation.");
		}
	}

	g_free (scheme_id);

	return def_style;
}

static void
on_location_changed (GeditDocument *doc,
		     GParamSpec    *pspec,
		     gpointer       useless)
{
#ifdef ENABLE_GVFS_METADATA
	GFile *location;

	location = gedit_document_get_location (doc);

	/* load metadata for this location: we load sync since metadata is
	 * always local so it should be fast and we need the information
	 * right after the location was set.
	 */
	if (location != NULL)
	{
		GError *error = NULL;

		if (doc->priv->metadata_info != NULL)
			g_object_unref (doc->priv->metadata_info);

		doc->priv->metadata_info = g_file_query_info (location,
							      METADATA_QUERY,
							      G_FILE_QUERY_INFO_NONE,
							      NULL,
							      &error);

		if (error != NULL)
		{
			if (error->code != G_FILE_ERROR_ISDIR &&
			    error->code != G_FILE_ERROR_NOTDIR &&
			    error->code != G_FILE_ERROR_NOENT)
			{
				g_warning ("%s", error->message);
			}

			g_error_free (error);
		}

		g_object_unref (location);
	}
#endif
}

static GtkSourceLanguage *
guess_language (GeditDocument *doc,
		const gchar   *content_type)
{
	gchar *data;
	GtkSourceLanguage *language = NULL;

	data = gedit_document_get_metadata (doc, GEDIT_METADATA_ATTRIBUTE_LANGUAGE);

	if (data != NULL)
	{
		gedit_debug_message (DEBUG_DOCUMENT, "Language from metadata: %s", data);

		if (strcmp (data, "_NORMAL_") != 0)
		{
			language = gtk_source_language_manager_get_language (
						gtk_source_language_manager_get_default (),
						data);
		}

		g_free (data);
	}
	else
	{
		GFile *location;
		gchar *basename = NULL;

		location = gedit_document_get_location (doc);
		gedit_debug_message (DEBUG_DOCUMENT, "Sniffing Language");

		if (location)
		{
			basename = g_file_get_basename (location);
		}
		else if (doc->priv->short_name != NULL)
		{
			basename = g_strdup (doc->priv->short_name);
		}

		language = gtk_source_language_manager_guess_language (
					gtk_source_language_manager_get_default (),
					basename,
					content_type);

		g_free (basename);

		if (location != NULL)
		{
			g_object_unref (location);
		}
	}

	return language;
}

static void
on_content_type_changed (GeditDocument *doc,
			 GParamSpec    *pspec,
			 gpointer       useless)
{
	if (!doc->priv->language_set_by_user)
	{
		GtkSourceLanguage *language;

		language = guess_language (doc, doc->priv->content_type);

		gedit_debug_message (DEBUG_DOCUMENT, "Language: %s",
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
on_highlight_updated (GeditDocument *doc,
		      GtkTextIter   *start,
		      GtkTextIter   *end)
{
	g_signal_emit (doc,
		       document_signals[SEARCH_HIGHLIGHT_UPDATED],
		       0,
		       start,
		       end);
}

static void
gedit_document_init (GeditDocument *doc)
{
	GeditDocumentPrivate *priv;
	GtkSourceStyleScheme *style_scheme;

	gedit_debug (DEBUG_DOCUMENT);

	doc->priv = gedit_document_get_instance_private (doc);
	priv = doc->priv;

	priv->editor_settings = g_settings_new ("org.gnome.gedit.preferences.editor");

	priv->location = NULL;
	priv->untitled_number = get_untitled_number ();

	priv->metadata_info = NULL;

	priv->content_type = get_default_content_type ();

	priv->readonly = FALSE;

	priv->stop_cursor_moved_emission = FALSE;

	priv->last_save_was_manually = TRUE;
	priv->language_set_by_user = FALSE;

	priv->empty_search = TRUE;

	priv->dispose_has_run = FALSE;

	priv->mtime.tv_sec = 0;
	priv->mtime.tv_usec = 0;

	g_get_current_time (&doc->priv->time_of_last_save_or_load);

	priv->encoding = gedit_encoding_get_utf8 ();

	g_settings_bind (priv->editor_settings,
	                 GEDIT_SETTINGS_MAX_UNDO_ACTIONS,
	                 doc,
	                 "max-undo-levels",
	                 G_SETTINGS_BIND_GET);

	g_settings_bind (priv->editor_settings,
	                 GEDIT_SETTINGS_BRACKET_MATCHING,
	                 doc,
	                 "highlight-matching-brackets",
	                 G_SETTINGS_BIND_GET);

	style_scheme = get_default_style_scheme (priv->editor_settings);
	if (style_scheme != NULL)
		gtk_source_buffer_set_style_scheme (GTK_SOURCE_BUFFER (doc),
						    style_scheme);

	g_signal_connect (doc,
			  "notify::content-type",
			  G_CALLBACK (on_content_type_changed),
			  NULL);

	g_signal_connect (doc,
			  "notify::location",
			  G_CALLBACK (on_location_changed),
			  NULL);

	g_signal_connect (doc,
			  "highlight-updated",
			  G_CALLBACK (on_highlight_updated),
			  NULL);
}

GeditDocument *
gedit_document_new (void)
{
	gedit_debug (DEBUG_DOCUMENT);

	return GEDIT_DOCUMENT (g_object_new (GEDIT_TYPE_DOCUMENT, NULL));
}

static gchar *
get_content_type_from_content (GeditDocument *doc)
{
	gchar *content_type;
	gchar *data;
	gsize datalen;
	GtkTextBuffer *buf;
	GtkTextIter start, end;

	buf = GTK_TEXT_BUFFER (doc);

	gtk_text_buffer_get_start_iter (buf, &start);
	end = start;
	gtk_text_iter_forward_chars (&end, 255);

	data = gtk_text_buffer_get_text (buf, &start, &end, TRUE);
	datalen = strlen (data);

	content_type = g_content_type_guess (NULL,
	                                     (const guchar *)data,
	                                     datalen,
	                                     NULL);

	g_free (data);

	return content_type;
}

static void
set_content_type_no_guess (GeditDocument *doc,
			   const gchar   *content_type)
{
	gchar *dupped_content_type;

	gedit_debug (DEBUG_DOCUMENT);

	if (doc->priv->content_type != NULL && content_type != NULL &&
	    (0 == strcmp (doc->priv->content_type, content_type)))
	{
		return;
	}

	g_free (doc->priv->content_type);

	/* For compression types, we try to just guess from the content */
	if (gedit_utils_get_compression_type_from_content_type (content_type) !=
	    GEDIT_DOCUMENT_COMPRESSION_TYPE_NONE)
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
		doc->priv->content_type = get_default_content_type ();
		g_free (dupped_content_type);
	}
	else
	{
		doc->priv->content_type = dupped_content_type;
	}

	g_object_notify (G_OBJECT (doc), "content-type");
}

static void
set_content_type (GeditDocument *doc,
		  const gchar   *content_type)
{
	gedit_debug (DEBUG_DOCUMENT);

	if (content_type == NULL)
	{
		GFile *location;
		gchar *guessed_type = NULL;

		/* If content type is null, we guess from the filename */
		location = gedit_document_get_location (doc);
		if (location != NULL)
		{
			gchar *basename;

			basename = g_file_get_basename (location);
			guessed_type = g_content_type_guess (basename, NULL, 0, NULL);

			g_free (basename);
			g_object_unref (location);
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
 * gedit_document_set_content_type:
 * @doc:
 * @content_type: (allow-none):
 */
void
gedit_document_set_content_type (GeditDocument *doc,
                                 const gchar   *content_type)
{
	g_return_if_fail (GEDIT_IS_DOCUMENT (doc));

	set_content_type (doc, content_type);
}

static void
set_location (GeditDocument *doc,
              GFile         *location)
{
	gedit_debug (DEBUG_DOCUMENT);

	g_return_if_fail ((location == NULL) || gedit_utils_is_valid_location (location));

	if (doc->priv->location == location)
		return;

	if (doc->priv->location != NULL)
	{
		g_object_unref (doc->priv->location);
		doc->priv->location = NULL;
	}

	if (location != NULL)
	{
		doc->priv->location = g_file_dup (location);

		if (doc->priv->untitled_number > 0)
		{
			release_untitled_number (doc->priv->untitled_number);
			doc->priv->untitled_number = 0;
		}
	}

	g_object_notify (G_OBJECT (doc), "location");

	if (doc->priv->short_name == NULL)
	{
		g_object_notify (G_OBJECT (doc), "shortname");
	}
}

/**
 * gedit_document_get_location:
 * @doc: a #GeditDocument
 *
 * Returns: (allow-none) (transfer full): a copy of the internal #GFile
 */
GFile *
gedit_document_get_location (GeditDocument *doc)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), NULL);

	return doc->priv->location == NULL ? NULL : g_file_dup (doc->priv->location);
}

void
gedit_document_set_location (GeditDocument *doc,
			     GFile         *location)
{
	g_return_if_fail (GEDIT_IS_DOCUMENT (doc));
	g_return_if_fail (G_IS_FILE (location));

	set_location (doc, location);
	set_content_type (doc, NULL);
}

/**
 * gedit_document_get_uri_for_display:
 * @doc:
 *
 * Note: this never returns %NULL.
 **/
gchar *
gedit_document_get_uri_for_display (GeditDocument *doc)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), g_strdup (""));

	if (doc->priv->location == NULL)
	{
		return g_strdup_printf (_("Unsaved Document %d"),
					doc->priv->untitled_number);
	}
	else
	{
		return g_file_get_parse_name (doc->priv->location);
	}
}

/**
 * gedit_document_get_short_name_for_display:
 * @doc:
 *
 * Note: this never returns %NULL.
 **/
gchar *
gedit_document_get_short_name_for_display (GeditDocument *doc)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), g_strdup (""));

	if (doc->priv->short_name != NULL)
	{
		return g_strdup (doc->priv->short_name);
	}
	else if (doc->priv->location == NULL)
	{
		return g_strdup_printf (_("Unsaved Document %d"),
					doc->priv->untitled_number);
	}
	else
	{
		return gedit_utils_basename_for_display (doc->priv->location);
	}
}

/**
 * gedit_document_set_short_name_for_display:
 * @doc:
 * @short_name: (allow-none):
 */
void
gedit_document_set_short_name_for_display (GeditDocument *doc,
                                           const gchar   *short_name)
{
	g_return_if_fail (GEDIT_IS_DOCUMENT (doc));

	g_free (doc->priv->short_name);
	doc->priv->short_name = g_strdup (short_name);

	g_object_notify (G_OBJECT (doc), "shortname");
}

gchar *
gedit_document_get_content_type (GeditDocument *doc)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), NULL);

 	return g_strdup (doc->priv->content_type);
}

/**
 * gedit_document_get_mime_type:
 * @doc:
 *
 * Note: this never returns %NULL.
 **/
gchar *
gedit_document_get_mime_type (GeditDocument *doc)
{
	gchar *mime_type = NULL;

	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), g_strdup ("text/plain"));

	if ((doc->priv->content_type != NULL) &&
	    (!g_content_type_is_unknown (doc->priv->content_type)))
	{
		mime_type = g_content_type_get_mime_type (doc->priv->content_type);
	}

 	return mime_type != NULL ? mime_type : g_strdup ("text/plain");
}

/* Note: do not emit the notify::read-only signal */
static gboolean
set_readonly (GeditDocument *doc,
	      gboolean       readonly)
{
	gedit_debug (DEBUG_DOCUMENT);

	readonly = (readonly != FALSE);

	if (doc->priv->readonly == readonly)
		return FALSE;

	doc->priv->readonly = readonly;

	return TRUE;
}

/**
 * _gedit_document_set_readonly:
 * @doc: a #GeditDocument
 * @readonly: %TRUE to set the document as read-only
 *
 * If @readonly is %TRUE sets @doc as read-only.
 */
void
_gedit_document_set_readonly (GeditDocument *doc,
                              gboolean       readonly)
{
	gedit_debug (DEBUG_DOCUMENT);

	g_return_if_fail (GEDIT_IS_DOCUMENT (doc));

	if (set_readonly (doc, readonly))
	{
		g_object_notify (G_OBJECT (doc), "read-only");
	}
}

gboolean
gedit_document_get_readonly (GeditDocument *doc)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), TRUE);

	return doc->priv->readonly;
}

static void
reset_temp_loading_data (GeditDocument       *doc)
{
	/* the loader has been used, throw it away */
	g_object_unref (doc->priv->loader);
	doc->priv->loader = NULL;

	doc->priv->requested_encoding = NULL;
	doc->priv->requested_line_pos = 0;
	doc->priv->requested_column_pos = 0;
}

static void
document_loader_loaded (GeditDocumentLoader *loader,
			const GError        *error,
			GeditDocument       *doc)
{
	/* load was successful */
	if (error == NULL ||
	    (error->domain == GEDIT_DOCUMENT_ERROR &&
	     error->code == GEDIT_DOCUMENT_ERROR_CONVERSION_FALLBACK))
	{
		GtkTextIter iter;
		GFileInfo *info;
		const gchar *content_type = NULL;
		gboolean read_only = FALSE;
		GTimeVal mtime = {0, 0};

		info = gedit_document_loader_get_info (loader);

		if (info)
		{
			if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE))
			{
				content_type = g_file_info_get_attribute_string (info,
										 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
			}

			if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_TIME_MODIFIED))
				g_file_info_get_modification_time (info, &mtime);

			if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
			{
				read_only = !g_file_info_get_attribute_boolean (info,
										G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);
			}
		}

		doc->priv->mtime = mtime;

		set_readonly (doc, read_only);

		g_get_current_time (&doc->priv->time_of_last_save_or_load);

               doc->priv->externally_modified = FALSE;
               doc->priv->deleted = FALSE;

		set_encoding (doc,
			      gedit_document_loader_get_encoding (loader),
			      (doc->priv->requested_encoding != NULL));

		set_content_type (doc, content_type);

		set_newline_type (doc,
		                  gedit_document_loader_get_newline_type (loader));

		set_compression_type (doc,
		                      gedit_document_loader_get_compression_type (loader));

		/* move the cursor at the requested line if any */
		if (doc->priv->requested_line_pos > 0)
		{
			gedit_document_goto_line_offset (doc,
							 doc->priv->requested_line_pos - 1,
							 doc->priv->requested_column_pos < 1 ? 0 : doc->priv->requested_column_pos - 1);
		}
		else
		{
			/* if enabled, move to the position stored in the metadata */
			if (g_settings_get_boolean (doc->priv->editor_settings, GEDIT_SETTINGS_RESTORE_CURSOR_POSITION))
			{
				gchar *pos;
				gint offset;

				pos = gedit_document_get_metadata (doc, GEDIT_METADATA_ATTRIBUTE_POSITION);

				offset = pos ? atoi (pos) : 0;
				g_free (pos);

				gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (doc),
								    &iter,
								    MAX (offset, 0));

				/* make sure it's a valid position, if the file
				 * changed we may have ended up in the middle of
				 * a utf8 character cluster */
				if (!gtk_text_iter_is_cursor_position (&iter))
				{
					gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (doc),
									&iter);
				}
			}
			/* otherwise to the top */
			else
			{
				gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (doc),
								&iter);
			}

			gtk_text_buffer_place_cursor (GTK_TEXT_BUFFER (doc), &iter);
		}
	}

	/* special case creating a named new doc */
	else if (doc->priv->create &&
	         (error->domain == G_IO_ERROR && error->code == G_IO_ERROR_NOT_FOUND) &&
	         (g_file_has_uri_scheme (doc->priv->location, "file")))
	{
		reset_temp_loading_data (doc);

		g_signal_emit (doc,
			       document_signals[LOADED],
			       0,
			       NULL);

		return;
	}

	g_signal_emit (doc,
		       document_signals[LOADED],
		       0,
		       error);

	reset_temp_loading_data (doc);
}

static void
document_loader_loading (GeditDocumentLoader *loader,
			 gboolean             completed,
			 const GError        *error,
			 GeditDocument       *doc)
{
	if (completed)
	{
		document_loader_loaded (loader, error, doc);
	}
	else
	{
		goffset size = 0;
		goffset read;
		GFileInfo *info;

		info = gedit_document_loader_get_info (loader);

		if (info && g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_SIZE))
		{
			size = g_file_info_get_attribute_uint64 (info,
								 G_FILE_ATTRIBUTE_STANDARD_SIZE);
		}

		read = gedit_document_loader_get_bytes_read (loader);

		g_signal_emit (doc,
			       document_signals[LOADING],
			       0,
			       read,
			       size);
	}
}

static void
gedit_document_load_real (GeditDocument       *doc,
			  GFile               *location,
			  const GeditEncoding *encoding,
			  gint                 line_pos,
			  gint                 column_pos,
			  gboolean             create)
{
	gchar *uri;

	g_return_if_fail (doc->priv->loader == NULL);

	uri = g_file_get_uri (location);
	gedit_debug_message (DEBUG_DOCUMENT, "load_real: uri = %s", uri);
	g_free (uri);

	/* create a loader. It will be destroyed when loading is completed */
	doc->priv->loader = gedit_document_loader_new (doc, location, encoding);

	g_signal_connect (doc->priv->loader,
			  "loading",
			  G_CALLBACK (document_loader_loading),
			  doc);

	doc->priv->create = create;
	doc->priv->requested_encoding = encoding;
	doc->priv->requested_line_pos = line_pos;
	doc->priv->requested_column_pos = column_pos;

	set_location (doc, location);
	set_content_type (doc, NULL);

	gedit_document_loader_load (doc->priv->loader);
}

/**
 * gedit_document_load_stream:
 * @doc:
 * @stream:
 * @encoding: (allow-none):
 * @line_pos:
 * @column_pos:
 **/
void
gedit_document_load_stream (GeditDocument       *doc,
                            GInputStream        *stream,
                            const GeditEncoding *encoding,
                            gint                 line_pos,
                            gint                 column_pos)
{
	g_return_if_fail (GEDIT_IS_DOCUMENT (doc));
	g_return_if_fail (G_IS_INPUT_STREAM (stream));
	g_return_if_fail (doc->priv->loader == NULL);

	gedit_debug_message (DEBUG_DOCUMENT, "load stream");

	/* create a loader. It will be destroyed when loading is completed */
	doc->priv->loader = gedit_document_loader_new_from_stream (doc, stream, encoding);

	g_signal_connect (doc->priv->loader,
			  "loading",
			  G_CALLBACK (document_loader_loading),
			  doc);

	doc->priv->create = FALSE;
	doc->priv->requested_encoding = encoding;
	doc->priv->requested_line_pos = line_pos;
	doc->priv->requested_column_pos = column_pos;

	set_location (doc, NULL);
	set_content_type (doc, NULL);

	gedit_document_loader_load (doc->priv->loader);
}

/**
 * gedit_document_load:
 * @doc: the #GeditDocument.
 * @location: the location where to load the document from.
 * @encoding: (allow-none): the #GeditEncoding to encode the document, or %NULL.
 * @line_pos: the line to show.
 * @column_pos: the column to show.
 * @create: whether the document should be created if it doesn't exist.
 *
 * Load a document. This results in the "load" signal to be emitted.
 */
void
gedit_document_load (GeditDocument       *doc,
		     GFile               *location,
		     const GeditEncoding *encoding,
		     gint                 line_pos,
		     gint                 column_pos,
		     gboolean             create)
{
	g_return_if_fail (GEDIT_IS_DOCUMENT (doc));
	g_return_if_fail (location != NULL);
	g_return_if_fail (gedit_utils_is_valid_location (location));

	g_signal_emit (doc, document_signals[LOAD], 0, location, encoding,
	               line_pos, column_pos, create);
}

/**
 * gedit_document_load_cancel:
 * @doc: the #GeditDocument.
 *
 * Cancel load of a document.
 */
gboolean
gedit_document_load_cancel (GeditDocument *doc)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), FALSE);

	if (doc->priv->loader == NULL)
		return FALSE;

	return gedit_document_loader_cancel (doc->priv->loader);
}

static gboolean
has_invalid_chars (GeditDocument *doc)
{
	GtkTextBuffer *buffer;
	GtkTextIter start;

	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), FALSE);

	gedit_debug (DEBUG_DOCUMENT);

	if (doc->priv->error_tag == NULL)
	{
		return FALSE;
	}

	buffer = GTK_TEXT_BUFFER (doc);

	gtk_text_buffer_get_start_iter (buffer, &start);

	if (gtk_text_iter_begins_tag (&start, doc->priv->error_tag) ||
	    gtk_text_iter_forward_to_tag_toggle (&start, doc->priv->error_tag))
	{
		return TRUE;
	}

	return FALSE;
}

static void
document_saver_saving (GeditDocumentSaver *saver,
		       gboolean            completed,
		       const GError       *error,
		       GeditDocument      *doc)
{
	gedit_debug (DEBUG_DOCUMENT);

	if (completed)
	{
		/* save was successful */
		if (error == NULL)
		{
			GFile *location;
			const gchar *content_type = NULL;
			GTimeVal mtime = {0, 0};
			GFileInfo *info;

			location = gedit_document_saver_get_location (saver);
			set_location (doc, location);
			g_object_unref (location);

			info = gedit_document_saver_get_info (saver);

			if (info != NULL)
			{
				if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE))
					content_type = g_file_info_get_attribute_string (info,
											 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);

				if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_TIME_MODIFIED))
					g_file_info_get_modification_time (info, &mtime);
			}

			set_content_type (doc, content_type);
			doc->priv->mtime = mtime;

			g_get_current_time (&doc->priv->time_of_last_save_or_load);

			doc->priv->externally_modified = FALSE;
			doc->priv->deleted = FALSE;

			_gedit_document_set_readonly (doc, FALSE);

			gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (doc),
						      FALSE);

			set_encoding (doc,
				      doc->priv->requested_encoding,
				      TRUE);
		}

		g_signal_emit (doc,
			       document_signals[SAVED],
			       0,
			       error);

		/* the saver has been used, throw it away */
		g_object_unref (doc->priv->saver);
		doc->priv->saver = NULL;
	}
	else
	{
		goffset size = 0;
		goffset written = 0;

		size = gedit_document_saver_get_file_size (saver);
		written = gedit_document_saver_get_bytes_written (saver);

		gedit_debug_message (DEBUG_DOCUMENT, "save progress: %" G_GINT64_FORMAT " of %" G_GINT64_FORMAT, written, size);

		g_signal_emit (doc,
			       document_signals[SAVING],
			       0,
			       written,
			       size);
	}
}

static void
gedit_document_save_real (GeditDocument                *doc,
			  GFile                        *location,
			  const GeditEncoding          *encoding,
			  GeditDocumentNewlineType      newline_type,
			  GeditDocumentCompressionType  compression_type,
			  GeditDocumentSaveFlags        flags)
{
	g_return_if_fail (doc->priv->saver == NULL);

	if (!(flags & GEDIT_DOCUMENT_SAVE_IGNORE_INVALID_CHARS) && has_invalid_chars (doc))
	{
		GError *error = NULL;

		g_set_error_literal (&error,
		                     GEDIT_DOCUMENT_ERROR,
		                     GEDIT_DOCUMENT_ERROR_CONVERSION_FALLBACK,
		                     "The document contains invalid characters");

		g_signal_emit (doc,
		               document_signals[SAVED],
		               0,
		               error);

		g_error_free (error);
	}
	else
	{
		gboolean ensure_trailing_newline;

		/* Do not make backups for remote files so they do not clutter
		 * remote systems. */
		if (!gedit_document_is_local (doc) ||
		    !g_settings_get_boolean (doc->priv->editor_settings,
					     GEDIT_SETTINGS_CREATE_BACKUP_COPY))
		{
			flags |= GEDIT_DOCUMENT_SAVE_IGNORE_BACKUP;
		}

		ensure_trailing_newline = g_settings_get_boolean (doc->priv->editor_settings,
								  GEDIT_SETTINGS_ENSURE_TRAILING_NEWLINE);

		/* create a saver, it will be destroyed once saving is complete */
		doc->priv->saver = gedit_document_saver_new (doc,
		                                             location,
		                                             encoding,
		                                             newline_type,
		                                             compression_type,
		                                             flags,
							     ensure_trailing_newline);

		gedit_document_saver_set_mount_operation_factory (doc->priv->saver,
								  doc->priv->mount_operation_factory,
								  doc->priv->mount_operation_userdata);

		g_signal_connect (doc->priv->saver,
		                  "saving",
		                  G_CALLBACK (document_saver_saving),
		                  doc);

		doc->priv->requested_encoding = encoding;
		doc->priv->newline_type = newline_type;
		doc->priv->compression_type = compression_type;

		gedit_document_saver_save (doc->priv->saver,
		                           &doc->priv->mtime);
	}
}

/**
 * gedit_document_save:
 * @doc: the #GeditDocument.
 * @flags: optionnal #GeditDocumentSaveFlags.
 *
 * Save the document to its previous location. This results in the "save"
 * signal to be emitted.
 */
void
gedit_document_save (GeditDocument          *doc,
		     GeditDocumentSaveFlags  flags)
{
	g_return_if_fail (GEDIT_IS_DOCUMENT (doc));
	g_return_if_fail (G_IS_FILE (doc->priv->location));

	g_signal_emit (doc,
		       document_signals[SAVE],
		       0,
		       doc->priv->location,
		       doc->priv->encoding,
		       doc->priv->newline_type,
		       doc->priv->compression_type,
		       flags);
}

/**
 * gedit_document_save_as:
 * @doc: the #GeditDocument.
 * @location: the location where to save the document.
 * @encoding: the #GeditEncoding to encode the document.
 * @newline_type: the #GeditDocumentNewlineType for the document.
 * @flags: optionnal #GeditDocumentSaveFlags.
 *
 * Save the document to a new location. This results in the "save" signal
 * to be emitted.
 */
void
gedit_document_save_as (GeditDocument                *doc,
			GFile                        *location,
			const GeditEncoding          *encoding,
			GeditDocumentNewlineType      newline_type,
			GeditDocumentCompressionType  compression_type,
			GeditDocumentSaveFlags        flags)
{
	GError *error = NULL;

	g_return_if_fail (GEDIT_IS_DOCUMENT (doc));
	g_return_if_fail (G_IS_FILE (location));
	g_return_if_fail (encoding != NULL);

	if (has_invalid_chars (doc))
	{
		g_set_error_literal (&error,
		                     GEDIT_DOCUMENT_ERROR,
		                     GEDIT_DOCUMENT_ERROR_CONVERSION_FALLBACK,
		                     "The document contains invalid chars");
	}

	/* priv->mtime refers to the the old location (if any). Thus, it should be
	 * ignored when saving as. */
	g_signal_emit (doc,
		       document_signals[SAVE],
		       0,
		       location,
		       encoding,
		       newline_type,
		       compression_type,
		       flags | GEDIT_DOCUMENT_SAVE_IGNORE_MTIME,
		       error);

	if (error != NULL)
	{
		g_error_free (error);
	}
}

gboolean
gedit_document_is_untouched (GeditDocument *doc)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), TRUE);

	return (doc->priv->location == NULL) &&
		(!gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (doc)));
}

gboolean
gedit_document_is_untitled (GeditDocument *doc)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), TRUE);

	return (doc->priv->location == NULL);
}

gboolean
gedit_document_is_local (GeditDocument *doc)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), FALSE);

	if (doc->priv->location == NULL)
	{
		return FALSE;
	}

	return g_file_has_uri_scheme (doc->priv->location, "file");
}

static void
check_file_on_disk (GeditDocument *doc)
{
	GFileInfo *info;

	if (doc->priv->location == NULL)
	{
		return;
	}

	info = g_file_query_info (doc->priv->location,
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

			read_only = !g_file_info_get_attribute_boolean (info,
									G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);

			_gedit_document_set_readonly (doc, read_only);
		}

		if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_TIME_MODIFIED))
		{
			GTimeVal timeval;

			g_file_info_get_modification_time (info, &timeval);

			if (timeval.tv_sec > doc->priv->mtime.tv_sec ||
			    (timeval.tv_sec == doc->priv->mtime.tv_sec &&
			     timeval.tv_usec > doc->priv->mtime.tv_usec))
			{
				doc->priv->externally_modified = TRUE;
			}
		}

		g_object_unref (info);
	}
	else
	{
		doc->priv->deleted = TRUE;
	}
}

gboolean
_gedit_document_check_externally_modified (GeditDocument *doc)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), FALSE);

	if (!doc->priv->externally_modified)
	{
		check_file_on_disk (doc);
	}

	return doc->priv->externally_modified;
}

gboolean
gedit_document_get_deleted (GeditDocument *doc)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), FALSE);

	if (!doc->priv->deleted)
	{
		check_file_on_disk (doc);
	}

	return doc->priv->deleted;
}

/*
 * If @line is bigger than the lines of the document, the cursor is moved
 * to the last line and FALSE is returned.
 */
gboolean
gedit_document_goto_line (GeditDocument *doc,
			  gint           line)
{
	gboolean ret = TRUE;
	guint line_count;
	GtkTextIter iter;

	gedit_debug (DEBUG_DOCUMENT);

	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), FALSE);
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
gedit_document_goto_line_offset (GeditDocument *doc,
				 gint           line,
				 gint           line_offset)
{
	gboolean ret;
	GtkTextIter iter;

	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), FALSE);
	g_return_val_if_fail (line >= -1, FALSE);
	g_return_val_if_fail (line_offset >= -1, FALSE);

	ret = gedit_document_goto_line (doc, line);

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

static GtkSourceSearchContext *
get_deprecated_search_context (GeditDocument *doc)
{
	GtkSourceSearchSettings *search_settings;

	if (doc->priv->search_context == NULL)
	{
		return NULL;
	}

	search_settings = gtk_source_search_context_get_settings (doc->priv->search_context);

	if (search_settings == doc->priv->deprecated_search_settings)
	{
		return doc->priv->search_context;
	}

	return NULL;
}

/**
 * gedit_document_set_search_text:
 * @doc:
 * @text: (allow-none):
 * @flags:
 *
 * Deprecated: 3.10: Use the search and replace API of GtkSourceView instead.
 **/

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

void
gedit_document_set_search_text (GeditDocument *doc,
				const gchar   *text,
				guint          flags)
{
	gchar *converted_text;
	gboolean notify = FALSE;
	GtkSourceSearchContext *search_context;

	g_return_if_fail (GEDIT_IS_DOCUMENT (doc));
	g_return_if_fail ((text == NULL) || g_utf8_validate (text, -1, NULL));

	gedit_debug_message (DEBUG_DOCUMENT, "text = %s", text);

	if (text != NULL)
	{
		if (*text != '\0')
		{
			converted_text = gtk_source_utils_unescape_search_text (text);
			notify = !gedit_document_get_can_search_again (doc);
		}
		else
		{
			converted_text = g_strdup ("");
			notify = gedit_document_get_can_search_again (doc);
		}

		gtk_source_search_settings_set_search_text (doc->priv->deprecated_search_settings,
							    converted_text);

		g_free (converted_text);
	}

	if (!GEDIT_SEARCH_IS_DONT_SET_FLAGS (flags))
	{
		gtk_source_search_settings_set_case_sensitive (doc->priv->deprecated_search_settings,
							       GEDIT_SEARCH_IS_CASE_SENSITIVE (flags));

		gtk_source_search_settings_set_at_word_boundaries (doc->priv->deprecated_search_settings,
								   GEDIT_SEARCH_IS_ENTIRE_WORD (flags));
	}

	search_context = get_deprecated_search_context (doc);

	if (search_context == NULL)
	{
		search_context = gtk_source_search_context_new (GTK_SOURCE_BUFFER (doc),
								doc->priv->deprecated_search_settings);

		_gedit_document_set_search_context (doc, search_context);

		g_object_unref (search_context);
	}

	if (notify)
	{
		g_object_notify (G_OBJECT (doc), "can-search-again");
	}
}

G_GNUC_END_IGNORE_DEPRECATIONS;

/**
 * gedit_document_get_search_text:
 * @doc:
 * @flags: (allow-none):
 *
 * Deprecated: 3.10: Use the search and replace API of GtkSourceView instead.
 */
gchar *
gedit_document_get_search_text (GeditDocument *doc,
				guint         *flags)
{
	const gchar *search_text;

	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), NULL);

	if (flags != NULL)
	{
		*flags = 0;

		if (gtk_source_search_settings_get_case_sensitive (doc->priv->deprecated_search_settings))
		{
			*flags |= GEDIT_SEARCH_CASE_SENSITIVE;
		}

		if (gtk_source_search_settings_get_at_word_boundaries (doc->priv->deprecated_search_settings))
		{
			*flags |= GEDIT_SEARCH_ENTIRE_WORD;
		}
	}

	search_text = gtk_source_search_settings_get_search_text (doc->priv->deprecated_search_settings);

	return gtk_source_utils_escape_search_text (search_text);
}

/**
 * gedit_document_get_can_search_again:
 * @doc:
 *
 * Deprecated: 3.10: Use the search and replace API of GtkSourceView instead.
 */
gboolean
gedit_document_get_can_search_again (GeditDocument *doc)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), FALSE);

	return gtk_source_search_settings_get_search_text (doc->priv->deprecated_search_settings) != NULL;
}

/**
 * gedit_document_search_forward:
 * @doc:
 * @start: (allow-none):
 * @end: (allow-none):
 * @match_start: (allow-none):
 * @match_end: (allow-none):
 *
 * Deprecated: 3.10: Use the search and replace API of GtkSourceView instead.
 **/
gboolean
gedit_document_search_forward (GeditDocument     *doc,
			       const GtkTextIter *start,
			       const GtkTextIter *end,
			       GtkTextIter       *match_start,
			       GtkTextIter       *match_end)
{
	const gchar *search_text;
	GtkTextIter iter;
	GtkTextSearchFlags search_flags;
	gboolean found = FALSE;
	GtkTextIter m_start;
	GtkTextIter m_end;

	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), FALSE);
	g_return_val_if_fail ((start == NULL) ||
			      (gtk_text_iter_get_buffer (start) ==  GTK_TEXT_BUFFER (doc)), FALSE);
	g_return_val_if_fail ((end == NULL) ||
			      (gtk_text_iter_get_buffer (end) ==  GTK_TEXT_BUFFER (doc)), FALSE);

	search_text = gtk_source_search_settings_get_search_text (doc->priv->deprecated_search_settings);

	if (search_text == NULL)
	{
		return FALSE;
	}

	if (start == NULL)
		gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (doc), &iter);
	else
		iter = *start;

	search_flags = GTK_TEXT_SEARCH_VISIBLE_ONLY | GTK_TEXT_SEARCH_TEXT_ONLY;

	if (!gtk_source_search_settings_get_case_sensitive (doc->priv->deprecated_search_settings))
	{
		search_flags |= GTK_TEXT_SEARCH_CASE_INSENSITIVE;
	}

	while (!found)
	{
		found = gtk_text_iter_forward_search (&iter,
		                                      search_text,
		                                      search_flags,
		                                      &m_start,
		                                      &m_end,
		                                      end);

		if (found &&
		    gtk_source_search_settings_get_at_word_boundaries (doc->priv->deprecated_search_settings))
		{
			found = gtk_text_iter_starts_word (&m_start) &&
					gtk_text_iter_ends_word (&m_end);

			if (!found)
				iter = m_end;
		}
		else
		{
			break;
		}
	}

	if (found && (match_start != NULL))
		*match_start = m_start;

	if (found && (match_end != NULL))
		*match_end = m_end;

	return found;
}

/**
 * gedit_document_search_backward:
 * @doc:
 * @start: (allow-none):
 * @end: (allow-none):
 * @match_start: (allow-none):
 * @match_end: (allow-none):
 *
 * Deprecated: 3.10: Use the search and replace API of GtkSourceView instead.
 **/
gboolean
gedit_document_search_backward (GeditDocument     *doc,
				const GtkTextIter *start,
				const GtkTextIter *end,
				GtkTextIter       *match_start,
				GtkTextIter       *match_end)
{
	const gchar *search_text;
	GtkTextIter iter;
	GtkTextSearchFlags search_flags;
	gboolean found = FALSE;
	GtkTextIter m_start;
	GtkTextIter m_end;

	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), FALSE);
	g_return_val_if_fail ((start == NULL) ||
			      (gtk_text_iter_get_buffer (start) ==  GTK_TEXT_BUFFER (doc)), FALSE);
	g_return_val_if_fail ((end == NULL) ||
			      (gtk_text_iter_get_buffer (end) ==  GTK_TEXT_BUFFER (doc)), FALSE);

	search_text = gtk_source_search_settings_get_search_text (doc->priv->deprecated_search_settings);

	if (search_text == NULL)
	{
		return FALSE;
	}

	if (end == NULL)
		gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (doc), &iter);
	else
		iter = *end;

	search_flags = GTK_TEXT_SEARCH_VISIBLE_ONLY | GTK_TEXT_SEARCH_TEXT_ONLY;

	if (!gtk_source_search_settings_get_case_sensitive (doc->priv->deprecated_search_settings))
	{
		search_flags |= GTK_TEXT_SEARCH_CASE_INSENSITIVE;
	}

	while (!found)
	{
		found = gtk_text_iter_backward_search (&iter,
		                                       search_text,
		                                       search_flags,
		                                       &m_start,
		                                       &m_end,
		                                       start);

		if (found &&
		    gtk_source_search_settings_get_at_word_boundaries (doc->priv->deprecated_search_settings))
		{
			found = gtk_text_iter_starts_word (&m_start) &&
					gtk_text_iter_ends_word (&m_end);

			if (!found)
				iter = m_start;
		}
		else
		{
			break;
		}
	}

	if (found && (match_start != NULL))
		*match_start = m_start;

	if (found && (match_end != NULL))
		*match_end = m_end;

	return found;
}

/**
 * gedit_document_replace_all:
 * @doc:
 * @find:
 * @replace:
 * @flags:
 *
 * Deprecated: 3.10: Use the search and replace API of GtkSourceView instead.
 */

/* FIXME this is an issue for introspection regardning @find */
gint
gedit_document_replace_all (GeditDocument       *doc,
			    const gchar         *find,
			    const gchar         *replace,
			    guint                flags)
{
	GtkTextIter iter;
	GtkTextIter m_start;
	GtkTextIter m_end;
	GtkTextSearchFlags search_flags = 0;
	gboolean found = TRUE;
	gint cont = 0;
	const gchar *search_text_from_settings;
	gchar *search_text;
	gchar *replace_text;
	gint replace_text_len;
	GtkTextBuffer *buffer;
	gboolean brackets_highlighting;

	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), 0);
	g_return_val_if_fail (replace != NULL, 0);

	search_text_from_settings = gtk_source_search_settings_get_search_text (doc->priv->deprecated_search_settings);

	g_return_val_if_fail ((find != NULL) || (search_text_from_settings != NULL), 0);

	buffer = GTK_TEXT_BUFFER (doc);

	if (find == NULL)
		search_text = g_strdup (search_text_from_settings);
	else
		search_text = gtk_source_utils_unescape_search_text (find);

	replace_text = gtk_source_utils_unescape_search_text (replace);

	gtk_text_buffer_get_start_iter (buffer, &iter);

	search_flags = GTK_TEXT_SEARCH_VISIBLE_ONLY | GTK_TEXT_SEARCH_TEXT_ONLY;

	if (!GEDIT_SEARCH_IS_CASE_SENSITIVE (flags))
	{
		search_flags = search_flags | GTK_TEXT_SEARCH_CASE_INSENSITIVE;
	}

	replace_text_len = strlen (replace_text);

	/* disable cursor_moved emission until the end of the
	 * replace_all so that we don't spend all the time
	 * updating the position in the statusbar
	 */
	doc->priv->stop_cursor_moved_emission = TRUE;

	/* also avoid spending time matching brackets */
	brackets_highlighting = gtk_source_buffer_get_highlight_matching_brackets (GTK_SOURCE_BUFFER (buffer));
	gtk_source_buffer_set_highlight_matching_brackets (GTK_SOURCE_BUFFER (buffer), FALSE);

	gtk_text_buffer_begin_user_action (buffer);

	do
	{
		found = gtk_text_iter_forward_search (&iter,
		                                      search_text,
		                                      search_flags,
		                                      &m_start,
		                                      &m_end,
		                                      NULL);

		if (found && GEDIT_SEARCH_IS_ENTIRE_WORD (flags))
		{
			gboolean word;

			word = gtk_text_iter_starts_word (&m_start) &&
			       gtk_text_iter_ends_word (&m_end);

			if (!word)
			{
				iter = m_end;
				continue;
			}
		}

		if (found)
		{
			++cont;

			gtk_text_buffer_delete (buffer,
						&m_start,
						&m_end);
			gtk_text_buffer_insert (buffer,
						&m_start,
						replace_text,
						replace_text_len);

			iter = m_start;
		}

	} while (found);

	gtk_text_buffer_end_user_action (buffer);

	/* re-enable cursor_moved emission and notify
	 * the current position
	 */
	doc->priv->stop_cursor_moved_emission = FALSE;
	emit_cursor_moved (doc);

	gtk_source_buffer_set_highlight_matching_brackets (GTK_SOURCE_BUFFER (buffer),
							   brackets_highlighting);

	g_free (search_text);
	g_free (replace_text);

	return cont;
}

static void
get_style_colors (GeditDocument *doc,
                  const gchar   *style_name,
                  gboolean      *foreground_set,
                  GdkRGBA       *foreground,
                  gboolean      *background_set,
                  GdkRGBA       *background,
                  gboolean      *line_background_set,
                  GdkRGBA       *line_background,
                  gboolean      *bold_set,
                  gboolean      *bold,
                  gboolean      *italic_set,
                  gboolean      *italic,
                  gboolean      *underline_set,
                  gboolean      *underline,
                  gboolean      *strikethrough_set,
                  gboolean      *strikethrough)
{
	GtkSourceStyleScheme *style_scheme;
	GtkSourceStyle *style;
	gchar *line_bg;
	gchar *bg;
	gchar *fg;

	style_scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (doc));
	if (style_scheme == NULL)
		goto fallback;

	style = gtk_source_style_scheme_get_style (style_scheme,
						   style_name);
	if (style == NULL)
		goto fallback;

	g_object_get (style,
		      "foreground-set", foreground_set,
		      "foreground", &fg,
		      "background-set", background_set,
		      "background", &bg,
		      "line-background-set", line_background_set,
		      "line-background", &line_bg,
		      "bold-set", bold_set,
		      "bold", bold,
		      "italic-set", italic_set,
		      "italic", italic,
		      "underline-set", underline_set,
		      "underline", underline,
		      "strikethrough-set", strikethrough_set,
		      "strikethrough", strikethrough,
		      NULL);

	if (*foreground_set)
	{
		if (fg == NULL || !gdk_rgba_parse (foreground, fg))
		{
			*foreground_set = FALSE;
		}
	}

	if (*background_set)
	{
		if (bg == NULL ||
		    !gdk_rgba_parse (background, bg))
		{
			*background_set = FALSE;
		}
	}

	if (*line_background_set)
	{
		if (line_bg == NULL ||
		    !gdk_rgba_parse (line_background, line_bg))
		{
			*line_background_set = FALSE;
		}
	}

	g_free (fg);
	g_free (bg);
	g_free (line_bg);

	return;

 fallback:
	gedit_debug_message (DEBUG_DOCUMENT,
			     "Falling back to hard-coded colors "
			     "for the \"found\" text tag.");

	gdk_rgba_parse (background, "#FFFF78");
	*background_set = TRUE;
	*foreground_set = FALSE;

	return;
}

static void
sync_tag_style (GeditDocument *doc,
                GtkTextTag    *tag,
                const gchar   *style_name)
{
	GdkRGBA fg;
	GdkRGBA bg;
	GdkRGBA line_bg;
	gboolean fg_set;
	gboolean bg_set;
	gboolean line_bg_set;
	gboolean bold;
	gboolean italic;
	gboolean underline;
	gboolean strikethrough;
	gboolean bold_set;
	gboolean italic_set;
	gboolean underline_set;
	gboolean strikethrough_set;

	gedit_debug (DEBUG_DOCUMENT);

	g_return_if_fail (tag != NULL);

	get_style_colors (doc,
	                  style_name,
	                  &fg_set, &fg,
	                  &bg_set, &bg,
	                  &line_bg_set, &line_bg,
	                  &bold_set, &bold,
	                  &italic_set, &italic,
	                  &underline_set, &underline,
	                  &strikethrough_set, &strikethrough);

	g_object_freeze_notify (G_OBJECT (tag));

	g_object_set (tag,
	              "foreground-rgba", fg_set ? &fg : NULL,
	              "background-rgba", bg_set ? &bg : NULL,
	              "paragraph-background-rgba", line_bg_set ? &line_bg : NULL,
	              "weight", bold_set && bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
	              "style", italic_set && italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL,
	              "underline", underline_set && underline ? PANGO_UNDERLINE_SINGLE : PANGO_UNDERLINE_NONE,
	              "strikethrough", strikethrough_set && strikethrough,
	              NULL);

	g_object_thaw_notify (G_OBJECT (tag));
}

static void
text_tag_set_highest_priority (GtkTextTag    *tag,
			       GtkTextBuffer *buffer)
{
	GtkTextTagTable *table;
	gint n;

	table = gtk_text_buffer_get_tag_table (buffer);
	n = gtk_text_tag_table_get_size (table);
	gtk_text_tag_set_priority (tag, n - 1);
}

/**
 * gedit_document_set_language:
 * @doc:
 * @lang: (allow-none):
 **/
void
gedit_document_set_language (GeditDocument     *doc,
			     GtkSourceLanguage *lang)
{
	g_return_if_fail (GEDIT_IS_DOCUMENT (doc));

	set_language (doc, lang, TRUE);
}

/**
 * gedit_document_get_language:
 * @doc:
 *
 * Return value: (transfer none):
 */
GtkSourceLanguage *
gedit_document_get_language (GeditDocument *doc)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), NULL);

	return gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (doc));
}

const GeditEncoding *
gedit_document_get_encoding (GeditDocument *doc)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), NULL);

	return doc->priv->encoding;
}

glong
_gedit_document_get_seconds_since_last_save_or_load (GeditDocument *doc)
{
	GTimeVal current_time;

	gedit_debug (DEBUG_DOCUMENT);

	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), -1);

	g_get_current_time (&current_time);

	return (current_time.tv_sec - doc->priv->time_of_last_save_or_load.tv_sec);
}

/**
 * gedit_document_set_enable_search_highlighting:
 * @doc:
 * @enable:
 *
 * Deprecated: 3.10: Use the search and replace API of GtkSourceView instead.
 */
void
gedit_document_set_enable_search_highlighting (GeditDocument *doc,
					       gboolean       enable)
{
	g_return_if_fail (GEDIT_IS_DOCUMENT (doc));

	if (doc->priv->search_context != NULL)
	{
		gtk_source_search_context_set_highlight (doc->priv->search_context,
							 enable);
	}
}

/**
 * gedit_document_get_enable_search_highlighting:
 * @doc:
 *
 * Deprecated: 3.10: Use the search and replace API of GtkSourceView instead.
 */
gboolean
gedit_document_get_enable_search_highlighting (GeditDocument *doc)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), FALSE);

	if (doc->priv->search_context == NULL)
	{
		return FALSE;
	}

	return gtk_source_search_context_get_highlight (doc->priv->search_context);
}

GeditDocumentNewlineType
gedit_document_get_newline_type (GeditDocument *doc)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), 0);

	return doc->priv->newline_type;
}

GeditDocumentCompressionType
gedit_document_get_compression_type (GeditDocument *doc)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), 0);

	return doc->priv->compression_type;
}

void
_gedit_document_set_mount_operation_factory (GeditDocument 	       *doc,
					    GeditMountOperationFactory	callback,
					    gpointer	                userdata)
{
	g_return_if_fail (GEDIT_IS_DOCUMENT (doc));

	doc->priv->mount_operation_factory = callback;
	doc->priv->mount_operation_userdata = userdata;
}

GMountOperation *
_gedit_document_create_mount_operation (GeditDocument *doc)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), NULL);

	if (doc->priv->mount_operation_factory == NULL)
	{
		return g_mount_operation_new ();
	}
	else
	{
		return doc->priv->mount_operation_factory (doc->priv->mount_operation_userdata);
	}
}

#ifndef ENABLE_GVFS_METADATA
gchar *
gedit_document_get_metadata (GeditDocument *doc,
			     const gchar   *key)
{
	gchar *value = NULL;

	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), NULL);
	g_return_val_if_fail (key != NULL, NULL);

	if (!gedit_document_is_untitled (doc))
	{
		value = gedit_metadata_manager_get (doc->priv->location, key);
	}

	return value;
}

void
gedit_document_set_metadata (GeditDocument *doc,
			     const gchar   *first_key,
			     ...)
{
	const gchar *key;
	const gchar *value;
	va_list var_args;

	g_return_if_fail (GEDIT_IS_DOCUMENT (doc));
	g_return_if_fail (first_key != NULL);

	if (gedit_document_is_untitled (doc))
	{
		/* Can't set metadata for untitled documents */
		return;
	}

	va_start (var_args, first_key);

	for (key = first_key; key; key = va_arg (var_args, const gchar *))
	{
		value = va_arg (var_args, const gchar *);

		if (doc->priv->location != NULL)
		{
			gedit_metadata_manager_set (doc->priv->location, key, value);
		}
	}

	va_end (var_args);
}

#else

/**
 * gedit_document_get_metadata:
 * @doc: a #GeditDocument
 * @key: name of the key
 *
 * Gets the metadata assigned to @key.
 *
 * Returns: the value assigned to @key.
 */
gchar *
gedit_document_get_metadata (GeditDocument *doc,
			     const gchar   *key)
{
	gchar *value = NULL;

	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), NULL);
	g_return_val_if_fail (key != NULL, NULL);

	if (doc->priv->metadata_info && g_file_info_has_attribute (doc->priv->metadata_info,
								   key))
	{
		value = g_strdup (g_file_info_get_attribute_string (doc->priv->metadata_info,
								    key));
	}

	return value;
}

static void
set_attributes_cb (GObject      *source,
		   GAsyncResult *res,
		   gpointer      useless)
{
	g_file_set_attributes_finish (G_FILE (source),
				      res,
				      NULL,
				      NULL);
}

/**
 * gedit_document_set_metadata:
 * @doc: a #GeditDocument
 * @first_key: name of the first key to set
 * @...: (allow-none): value for the first key, followed optionally by more key/value pairs,
 * followed by %NULL.
 *
 * Sets metadata on a document.
 */
void
gedit_document_set_metadata (GeditDocument *doc,
			     const gchar   *first_key,
			     ...)
{
	const gchar *key;
	const gchar *value;
	va_list var_args;
	GFileInfo *info;
	GFile *location;

	g_return_if_fail (GEDIT_IS_DOCUMENT (doc));
	g_return_if_fail (first_key != NULL);

	info = g_file_info_new ();

	va_start (var_args, first_key);

	for (key = first_key; key; key = va_arg (var_args, const gchar *))
	{
		value = va_arg (var_args, const gchar *);

		if (value != NULL)
		{
			g_file_info_set_attribute_string (info,
							  key, value);
		}
		else
		{
			/* Unset the key */
			g_file_info_set_attribute (info, key,
						   G_FILE_ATTRIBUTE_TYPE_INVALID,
						   NULL);
		}
	}

	va_end (var_args);

	if (doc->priv->metadata_info != NULL)
		g_file_info_copy_into (info, doc->priv->metadata_info);

	location = gedit_document_get_location (doc);

	if (location != NULL)
	{
		g_file_set_attributes_async (location,
					     info,
					     G_FILE_QUERY_INFO_NONE,
					     G_PRIORITY_DEFAULT,
					     NULL,
					     set_attributes_cb,
					     NULL);

		g_object_unref (location);
	}

	g_object_unref (info);
}
#endif

static void
sync_error_tag (GeditDocument *doc,
                GParamSpec    *pspec,
                gpointer       data)
{
	sync_tag_style (doc, doc->priv->error_tag, "def:error");
}

void
_gedit_document_apply_error_style (GeditDocument *doc,
                                   GtkTextIter   *start,
                                   GtkTextIter   *end)
{
	GtkTextBuffer *buffer;

	gedit_debug (DEBUG_DOCUMENT);

	buffer = GTK_TEXT_BUFFER (doc);

	if (doc->priv->error_tag == NULL)
	{
		doc->priv->error_tag = gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (doc),
		                                                   "invalid-char-style",
		                                                   NULL);

		sync_error_tag (doc, NULL, NULL);

		g_signal_connect (doc,
		                  "notify::style-scheme",
		                  G_CALLBACK (sync_error_tag),
		                  NULL);
	}

	/* make sure the 'error' tag has the priority over
	 * syntax highlighting tags */
	text_tag_set_highest_priority (doc->priv->error_tag,
	                               GTK_TEXT_BUFFER (doc));

	gtk_text_buffer_apply_tag (buffer,
	                           doc->priv->error_tag,
	                           start,
	                           end);
}

static void
update_empty_search (GeditDocument *doc)
{
	gboolean new_value;

	if (doc->priv->search_context == NULL)
	{
		new_value = TRUE;
	}
	else
	{
		GtkSourceSearchSettings *search_settings;

		search_settings = gtk_source_search_context_get_settings (doc->priv->search_context);

		new_value = gtk_source_search_settings_get_search_text (search_settings) == NULL;
	}

	if (doc->priv->empty_search != new_value)
	{
		doc->priv->empty_search = new_value;
		g_object_notify (G_OBJECT (doc), "empty-search");
	}
}

static void
connect_search_settings (GeditDocument *doc)
{
	GtkSourceSearchSettings *search_settings;

	search_settings = gtk_source_search_context_get_settings (doc->priv->search_context);

	/* Note: the signal handler is never disconnected. If the search context
	 * changes its search settings, the old search settings will most
	 * probably be destroyed, anyway. So it shouldn't cause performance
	 * problems.
	 */
	g_signal_connect_object (search_settings,
				 "notify::search-text",
				 G_CALLBACK (update_empty_search),
				 doc,
				 G_CONNECT_SWAPPED);
}

void
_gedit_document_set_search_context (GeditDocument          *doc,
				    GtkSourceSearchContext *search_context)
{
	g_return_if_fail (GEDIT_IS_DOCUMENT (doc));

	if (doc->priv->search_context != NULL)
	{
		g_signal_handlers_disconnect_by_func (doc->priv->search_context,
						      connect_search_settings,
						      doc);

		g_object_unref (doc->priv->search_context);
	}

	doc->priv->search_context = search_context;

	if (search_context != NULL)
	{
		g_object_ref (search_context);

		g_settings_bind (doc->priv->editor_settings, GEDIT_SETTINGS_SEARCH_HIGHLIGHTING,
				 search_context, "highlight",
				 G_SETTINGS_BIND_GET);

		g_signal_connect_object (search_context,
					 "notify::settings",
					 G_CALLBACK (connect_search_settings),
					 doc,
					 G_CONNECT_SWAPPED);

		connect_search_settings (doc);
	}

	update_empty_search (doc);
}

GtkSourceSearchContext *
_gedit_document_get_search_context (GeditDocument *doc)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), NULL);

	return doc->priv->search_context;
}

gboolean
_gedit_document_get_empty_search (GeditDocument *doc)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), TRUE);

	return doc->priv->empty_search;
}

/* ex:set ts=8 noet: */
