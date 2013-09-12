/*
 * gedit-document-loader.c
 * This file is part of gedit
 *
 * Copyright (C) 2005 - Paolo Maggi
 * Copyright (C) 2007 - Paolo Maggi, Steve Frécinaux
 * Copyright (C) 2008 - Jesse van den Kieboom
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include "gedit-document-loader.h"
#include "gedit-document-output-stream.h"
#include "gedit-utils.h"
#include "gedit-marshal.h"
#include "gedit-enum-types.h"
#include "gedit-settings.h"

#ifndef ENABLE_GVFS_METADATA
#include "gedit-metadata-manager.h"
#endif

#if 0
#define DEBUG(x) (x)
#else
#define DEBUG(x)
#endif

typedef struct
{
	GeditDocumentLoader *loader;
	GCancellable 	    *cancellable;

	gssize               read;
	gboolean             tried_mount;
} AsyncData;

/* Signals */

enum {
	LOADING,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/* Properties */

enum
{
	PROP_0,
	PROP_DOCUMENT,
	PROP_LOCATION,
	PROP_ENCODING,
	PROP_NEWLINE_TYPE,
	PROP_STREAM,
	PROP_COMPRESSION_TYPE
};

#define READ_CHUNK_SIZE 8192
#define LOADER_QUERY_ATTRIBUTES G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE "," \
				G_FILE_ATTRIBUTE_STANDARD_TYPE "," \
				G_FILE_ATTRIBUTE_TIME_MODIFIED "," \
				G_FILE_ATTRIBUTE_STANDARD_SIZE "," \
				G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE "," \
				GEDIT_METADATA_ATTRIBUTE_ENCODING

static void open_async_read (AsyncData *async);

struct _GeditDocumentLoaderPrivate
{
	GSettings		 *enc_settings;
	GSettings		 *editor_settings;

	GeditDocument		 *document;
	gboolean		  used;

	/* Info on the current file */
	GFileInfo		 *info;
	GFile			 *location;
	const GeditEncoding	 *encoding;
	const GeditEncoding	 *auto_detected_encoding;
	GeditDocumentNewlineType  auto_detected_newline_type;
	GeditDocumentCompressionType auto_detected_compression_type;

	goffset                   bytes_read;

	GCancellable 	         *cancellable;
	GInputStream	         *stream;
	GOutputStream            *output;

	gchar                     buffer[READ_CHUNK_SIZE];

	GError                   *error;
	gboolean                  guess_content_type_from_content;
};

G_DEFINE_TYPE_WITH_PRIVATE (GeditDocumentLoader, gedit_document_loader, G_TYPE_OBJECT)

static void
gedit_document_loader_set_property (GObject      *object,
				    guint         prop_id,
				    const GValue *value,
				    GParamSpec   *pspec)
{
	GeditDocumentLoader *loader = GEDIT_DOCUMENT_LOADER (object);

	switch (prop_id)
	{
		case PROP_DOCUMENT:
			g_return_if_fail (loader->priv->document == NULL);
			loader->priv->document = g_value_get_object (value);
			break;
		case PROP_LOCATION:
			g_return_if_fail (loader->priv->location == NULL);
			loader->priv->location = g_value_dup_object (value);
			break;
		case PROP_ENCODING:
			g_return_if_fail (loader->priv->encoding == NULL);
			loader->priv->encoding = g_value_get_boxed (value);
			break;
		case PROP_NEWLINE_TYPE:
			loader->priv->auto_detected_newline_type = g_value_get_enum (value);
			break;
		case PROP_STREAM:
			loader->priv->stream = g_value_dup_object (value);
			break;
		case PROP_COMPRESSION_TYPE:
			loader->priv->auto_detected_compression_type = g_value_get_enum (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_document_loader_get_property (GObject    *object,
				    guint       prop_id,
				    GValue     *value,
				    GParamSpec *pspec)
{
	GeditDocumentLoader *loader = GEDIT_DOCUMENT_LOADER (object);

	switch (prop_id)
	{
		case PROP_DOCUMENT:
			g_value_set_object (value, loader->priv->document);
			break;
		case PROP_LOCATION:
			g_value_set_object (value, loader->priv->location);
			break;
		case PROP_ENCODING:
			g_value_set_boxed (value, gedit_document_loader_get_encoding (loader));
			break;
		case PROP_NEWLINE_TYPE:
			g_value_set_enum (value, loader->priv->auto_detected_newline_type);
			break;
		case PROP_STREAM:
			g_value_set_object (value, loader->priv->stream);
			break;
		case PROP_COMPRESSION_TYPE:
			g_value_set_enum (value, loader->priv->auto_detected_compression_type);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_document_loader_dispose (GObject *object)
{
	GeditDocumentLoaderPrivate *priv;

	priv = GEDIT_DOCUMENT_LOADER (object)->priv;

	if (priv->cancellable != NULL)
	{
		g_cancellable_cancel (priv->cancellable);
		g_object_unref (priv->cancellable);
		priv->cancellable = NULL;
	}

	g_clear_error (&priv->error);

	g_clear_object (&priv->stream);
	g_clear_object (&priv->output);
	g_clear_object (&priv->info);
	g_clear_object (&priv->location);
	g_clear_object (&priv->enc_settings);
	g_clear_object (&priv->editor_settings);

	G_OBJECT_CLASS (gedit_document_loader_parent_class)->dispose (object);
}

static void
gedit_document_loader_class_init (GeditDocumentLoaderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gedit_document_loader_dispose;
	object_class->get_property = gedit_document_loader_get_property;
	object_class->set_property = gedit_document_loader_set_property;

	g_object_class_install_property (object_class,
					 PROP_DOCUMENT,
					 g_param_spec_object ("document",
							      "Document",
							      "The GeditDocument this GeditDocumentLoader is associated with",
							      GEDIT_TYPE_DOCUMENT,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class,
					 PROP_LOCATION,
					 g_param_spec_object ("location",
							      "LOCATION",
							      "The LOCATION this GeditDocumentLoader loads the document from",
							      G_TYPE_FILE,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
					 PROP_ENCODING,
					 g_param_spec_boxed ("encoding",
							     "Encoding",
							     "The encoding of the saved file",
							     GEDIT_TYPE_ENCODING,
							     G_PARAM_READWRITE |
							     G_PARAM_CONSTRUCT_ONLY |
							     G_PARAM_STATIC_STRINGS));

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

	g_object_class_install_property (object_class, PROP_COMPRESSION_TYPE,
	                                 g_param_spec_enum ("compression-type",
	                                                    "Compression type",
	                                                    "The compression type",
	                                                    GEDIT_TYPE_DOCUMENT_COMPRESSION_TYPE,
	                                                    GEDIT_DOCUMENT_COMPRESSION_TYPE_NONE,
	                                                    G_PARAM_READWRITE |
	                                                    G_PARAM_CONSTRUCT |
	                                                    G_PARAM_STATIC_NAME |
	                                                    G_PARAM_STATIC_BLURB));

	g_object_class_install_property (object_class,
					 PROP_STREAM,
					 g_param_spec_object ("stream",
							      "STREAM",
							      "The STREAM this GeditDocumentLoader loads the document from",
							      G_TYPE_INPUT_STREAM,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));

	signals[LOADING] =
		g_signal_new ("loading",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GeditDocumentLoaderClass, loading),
			      NULL, NULL,
			      gedit_marshal_VOID__BOOLEAN_POINTER,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_BOOLEAN,
			      G_TYPE_POINTER);
}

static void
gedit_document_loader_init (GeditDocumentLoader *loader)
{
	loader->priv = gedit_document_loader_get_instance_private (loader);

	loader->priv->enc_settings = g_settings_new ("org.gnome.gedit.preferences.encodings");
	loader->priv->editor_settings = g_settings_new ("org.gnome.gedit.preferences.editor");
}

GeditDocumentLoader *
gedit_document_loader_new_from_stream (GeditDocument       *doc,
                                       GInputStream        *stream,
                                       const GeditEncoding *encoding)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), NULL);
	g_return_val_if_fail (G_IS_INPUT_STREAM (stream), NULL);

	return GEDIT_DOCUMENT_LOADER (g_object_new (GEDIT_TYPE_DOCUMENT_LOADER,
						    "document", doc,
						    "stream", stream,
						    "encoding", encoding,
						    NULL));
}

GeditDocumentLoader *
gedit_document_loader_new (GeditDocument       *doc,
			   GFile               *location,
			   const GeditEncoding *encoding)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), NULL);

	return GEDIT_DOCUMENT_LOADER (g_object_new (GEDIT_TYPE_DOCUMENT_LOADER,
						    "document", doc,
						    "location", location,
						    "encoding", encoding,
						    NULL));
}

static AsyncData *
async_data_new (GeditDocumentLoader *loader)
{
	AsyncData *async;

	async = g_slice_new (AsyncData);
	async->loader = loader;
	async->cancellable = g_object_ref (loader->priv->cancellable);
	async->tried_mount = FALSE;

	return async;
}

static void
async_data_free (AsyncData *async)
{
	g_object_unref (async->cancellable);
	g_slice_free (AsyncData, async);
}

static const GeditEncoding *
get_metadata_encoding (GeditDocumentLoader *loader)
{
	const GeditEncoding *enc = NULL;
	GFileInfo *info;

	if (loader->priv->location == NULL)
	{
		/* If we are reading from a stream directly, then there is
		   nothing to do */
		return NULL;
	}

#ifndef ENABLE_GVFS_METADATA
	gchar *charset;

	charset = gedit_metadata_manager_get (loader->priv->location, "encoding");

	if (charset == NULL)
		return NULL;

	enc = gedit_encoding_get_from_charset (charset);

	g_free (charset);
#else

	info = gedit_document_loader_get_info (loader);

	/* check if the encoding was set in the metadata */
	if (g_file_info_has_attribute (info, GEDIT_METADATA_ATTRIBUTE_ENCODING))
	{
		const gchar *charset;

		charset = g_file_info_get_attribute_string (info,
							    GEDIT_METADATA_ATTRIBUTE_ENCODING);

		if (charset == NULL)
			return NULL;

		enc = gedit_encoding_get_from_charset (charset);
	}
#endif

	return enc;
}

static void
loader_load_completed_or_failed (GeditDocumentLoader *loader,
				 AsyncData           *async)
{
	gedit_document_loader_loading (loader,
				       TRUE,
				       loader->priv->error);

	if (async)
		async_data_free (async);
}

static void
async_failed (AsyncData *async,
	      GError    *error)
{
	g_propagate_error (&async->loader->priv->error, error);
	loader_load_completed_or_failed (async->loader, async);
}

static void
close_input_stream_ready_cb (GInputStream *stream,
			     GAsyncResult *res,
			     AsyncData    *async)
{
	GError *error = NULL;

	DEBUG ({
	       g_print ("%s\n", G_STRFUNC);
	});

	/* check cancelled state manually */
	if (g_cancellable_is_cancelled (async->cancellable))
	{
		async_data_free (async);
		return;
	}

	DEBUG ({
	       g_print ("Finished closing input stream\n");
	});

	if (!g_input_stream_close_finish (stream, res, &error))
	{
		DEBUG ({
		       g_print ("Closing input stream error: %s\n", error->message);
		});

		async_failed (async, error);
		return;
	}

	DEBUG ({
	       g_print ("Close output stream\n");
	});

	if (!g_output_stream_close (async->loader->priv->output,
				    async->cancellable, &error))
	{
		async_failed (async, error);
		return;
	}

	/* Check if we needed some fallback char, if so, check if there was
	   a previous error and if not set a fallback used error */
	if ((gedit_document_output_stream_get_num_fallbacks (GEDIT_DOCUMENT_OUTPUT_STREAM (async->loader->priv->output)) != 0) &&
	    async->loader->priv->error == NULL)
	{
		g_set_error_literal (&async->loader->priv->error,
		                     GEDIT_DOCUMENT_ERROR,
		                     GEDIT_DOCUMENT_ERROR_CONVERSION_FALLBACK,
		                     "There was a conversion error and it was "
		                     "needed to use a fallback char");
	}

	loader_load_completed_or_failed (async->loader, async);
}

static void
write_complete (AsyncData *async)
{
	if (async->loader->priv->stream)
	{
		g_input_stream_close_async (G_INPUT_STREAM (async->loader->priv->stream),
					    G_PRIORITY_HIGH,
					    async->cancellable,
					    (GAsyncReadyCallback)close_input_stream_ready_cb,
					    async);
	}
}

/* prototype, because they call each other... isn't C lovely */
static void	read_file_chunk		(AsyncData *async);

static void
write_file_chunk (AsyncData *async)
{
	GeditDocumentLoader *loader;
	gssize bytes_written;
	GError *error = NULL;

	loader = async->loader;

	/* we use sync methods on doc stream since it is in memory. Using async
	   would be racy and we can endup with invalidated iters */
	bytes_written = g_output_stream_write (G_OUTPUT_STREAM (loader->priv->output),
					       loader->priv->buffer,
					       async->read,
					       async->cancellable,
					       &error);

	DEBUG ({
	       g_print ("Written: %" G_GSSIZE_FORMAT "\n", bytes_written);
	});

	if (bytes_written == -1)
	{
		DEBUG ({
		       g_print ("Write error: %s\n", error->message);
		});

		async_failed (async, error);
		return;
	}

	/* note that this signal blocks the read... check if it isn't
	 * a performance problem
	 */
	gedit_document_loader_loading (loader,
				       FALSE,
				       NULL);

	read_file_chunk (async);
}

static void
async_read_cb (GInputStream *stream,
	       GAsyncResult *res,
	       AsyncData    *async)
{
	GeditDocumentLoader *loader;
	GError *error = NULL;

	DEBUG ({
	       g_print ("%s\n", G_STRFUNC);
	});

	/* manually check cancelled state */
	if (g_cancellable_is_cancelled (async->cancellable))
	{
		async_data_free (async);
		return;
	}

	loader = async->loader;

	async->read = g_input_stream_read_finish (stream, res, &error);

	/* error occurred */
	if (async->read == -1)
	{
		async_failed (async, error);
		return;
	}

	/* Check for the extremely unlikely case where the file size overflows. */
	if (loader->priv->bytes_read + async->read < loader->priv->bytes_read)
	{
		g_set_error (&loader->priv->error,
			     GEDIT_DOCUMENT_ERROR,
			     GEDIT_DOCUMENT_ERROR_TOO_BIG,
			     "File too big");

		async_failed (async, loader->priv->error);
		return;
	}

	if (loader->priv->guess_content_type_from_content &&
	    async->read > 0 &&
	    loader->priv->bytes_read == 0)
	{
		gchar *guessed;

		guessed = g_content_type_guess (NULL,
		                                (guchar *)loader->priv->buffer,
		                                async->read,
		                                NULL);

		if (guessed != NULL)
		{
			g_file_info_set_attribute_string (loader->priv->info,
			                                  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
			                                  guessed);

			g_free (guessed);
		}
	}

	/* Bump the size. */
	loader->priv->bytes_read += async->read;

	/* end of the file, we are done! */
	if (async->read == 0)
	{
		/* flush the stream to ensure proper line ending detection */
		g_output_stream_flush (loader->priv->output, NULL, NULL);

		loader->priv->auto_detected_encoding =
			gedit_document_output_stream_get_guessed (GEDIT_DOCUMENT_OUTPUT_STREAM (loader->priv->output));

		loader->priv->auto_detected_newline_type =
			gedit_document_output_stream_detect_newline_type (GEDIT_DOCUMENT_OUTPUT_STREAM (loader->priv->output));



		write_complete (async);

		return;
	}

	write_file_chunk (async);
}

static void
read_file_chunk (AsyncData *async)
{
	GeditDocumentLoader *loader;

	loader = async->loader;

	g_input_stream_read_async (G_INPUT_STREAM (loader->priv->stream),
				   loader->priv->buffer,
				   READ_CHUNK_SIZE,
				   G_PRIORITY_HIGH,
				   async->cancellable,
				   (GAsyncReadyCallback) async_read_cb,
				   async);
}

static GSList *
get_candidate_encodings (GeditDocumentLoader *loader)
{
	const GeditEncoding *metadata;
	GSList *encodings;
	gchar **enc_strv;

	enc_strv = g_settings_get_strv (loader->priv->enc_settings,
					GEDIT_SETTINGS_ENCODING_AUTO_DETECTED);

	encodings = _gedit_encoding_strv_to_list ((const gchar * const *)enc_strv);
	g_strfreev (enc_strv);

	metadata = get_metadata_encoding (loader);
	if (metadata != NULL)
	{
		encodings = g_slist_prepend (encodings, (gpointer)metadata);
	}

	return encodings;
}

static GInputStream *
compression_gzip_stream (GeditDocumentLoader *loader)
{
	GZlibDecompressor *decompressor;
	GInputStream *base_stream;

	decompressor = g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP);

	base_stream = g_converter_input_stream_new (loader->priv->stream,
	                                            G_CONVERTER (decompressor));

	g_object_unref (decompressor);

	loader->priv->auto_detected_compression_type = GEDIT_DOCUMENT_COMPRESSION_TYPE_GZIP;

	return base_stream;
}

static void
start_stream_read (AsyncData *async)
{
	GSList *candidate_encodings;
	GeditDocumentLoader *loader;
	GInputStream *base_stream = NULL;
	GFileInfo *info;
	gboolean ensure_trailing_newline;

	loader = async->loader;
	info = loader->priv->info;

	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE))
	{
		const gchar *content_type = g_file_info_get_content_type (info);

		switch (gedit_utils_get_compression_type_from_content_type (content_type))
		{
			case GEDIT_DOCUMENT_COMPRESSION_TYPE_GZIP:
				base_stream = compression_gzip_stream (loader);
				break;
			case GEDIT_DOCUMENT_COMPRESSION_TYPE_NONE:
				/* NOOP */
				break;
		}
	}

	if (base_stream == NULL)
	{
		base_stream = g_object_ref (loader->priv->stream);
		loader->priv->auto_detected_compression_type = GEDIT_DOCUMENT_COMPRESSION_TYPE_NONE;
	}

	g_object_unref (loader->priv->stream);
	loader->priv->stream = base_stream;

	/* Get the candidate encodings */
	if (loader->priv->encoding == NULL)
	{
		candidate_encodings = get_candidate_encodings (loader);
	}
	else
	{
		candidate_encodings = g_slist_prepend (NULL, (gpointer)loader->priv->encoding);
	}

	ensure_trailing_newline = g_settings_get_boolean (loader->priv->editor_settings,
	                                                  "ensure-trailing-newline");

	/* Output stream */
	loader->priv->output = gedit_document_output_stream_new (loader->priv->document,
	                                                         candidate_encodings,
	                                                         ensure_trailing_newline);

	g_slist_free (candidate_encodings);

	/* start reading */
	read_file_chunk (async);
}

static void
finish_query_info (AsyncData *async)
{
	GeditDocumentLoader *loader;
	GFileInfo *info;

	loader = async->loader;
	info = loader->priv->info;

	/* if it's not a regular file, error out... */
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_TYPE) &&
	    g_file_info_get_file_type (info) != G_FILE_TYPE_REGULAR)
	{
		g_set_error (&loader->priv->error,
			     G_IO_ERROR,
			     G_IO_ERROR_NOT_REGULAR_FILE,
			     "Not a regular file");

		loader_load_completed_or_failed (loader, async);

		return;
	}

	start_stream_read (async);
}

static void
query_info_cb (GFile        *source,
	       GAsyncResult *res,
	       AsyncData    *async)
{
	GFileInfo *info;
	GError *error = NULL;
	GeditDocumentLoaderPrivate *priv;

	DEBUG ({
	       g_print ("%s\n", G_STRFUNC);
	});

	/* manually check the cancelled state */
	if (g_cancellable_is_cancelled (async->cancellable))
	{
		async_data_free (async);
		return;
	}

	priv = async->loader->priv;

	/* finish the info query */
	info = g_file_query_info_finish (priv->location,
	                                 res,
	                                 &error);

	if (info == NULL)
	{
		/* propagate the error and clean up */
		async_failed (async, error);
		return;
	}

	priv->info = info;

	finish_query_info (async);
}

static void
mount_ready_callback (GFile        *file,
		      GAsyncResult *res,
		      AsyncData    *async)
{
	GError *error = NULL;
	gboolean mounted;

	DEBUG ({
	       g_print ("%s\n", G_STRFUNC);
	});

	/* manual check for cancelled state */
	if (g_cancellable_is_cancelled (async->cancellable))
	{
		async_data_free (async);
		return;
	}

	mounted = g_file_mount_enclosing_volume_finish (file, res, &error);

	if (!mounted)
	{
		async_failed (async, error);
	}
	else
	{
		/* try again to open the file for reading */
		open_async_read (async);
	}
}

static void
recover_not_mounted (AsyncData *async)
{
	GeditDocument *doc;
	GMountOperation *mount_operation;

	DEBUG ({
	       g_print ("%s\n", G_STRFUNC);
	});

	doc = gedit_document_loader_get_document (async->loader);
	mount_operation = _gedit_document_create_mount_operation (doc);

	async->tried_mount = TRUE;
	g_file_mount_enclosing_volume (async->loader->priv->location,
				       G_MOUNT_MOUNT_NONE,
				       mount_operation,
				       async->cancellable,
				       (GAsyncReadyCallback) mount_ready_callback,
				       async);

	g_object_unref (mount_operation);
}

static void
async_read_ready_callback (GObject      *source,
			   GAsyncResult *res,
		           AsyncData    *async)
{
	GError *error = NULL;
	GeditDocumentLoader *loader;

	DEBUG ({
	       g_print ("%s\n", G_STRFUNC);
	});

	/* manual check for cancelled state */
	if (g_cancellable_is_cancelled (async->cancellable))
	{
		async_data_free (async);
		return;
	}

	loader = async->loader;

	loader->priv->stream = G_INPUT_STREAM (g_file_read_finish (loader->priv->location,
								     res, &error));

	if (!loader->priv->stream)
	{
		if (error->code == G_IO_ERROR_NOT_MOUNTED && !async->tried_mount)
		{
			recover_not_mounted (async);
			g_error_free (error);
			return;
		}

		/* Propagate error */
		g_propagate_error (&loader->priv->error, error);
		gedit_document_loader_loading (loader,
					       TRUE,
					       loader->priv->error);

		async_data_free (async);
		return;
	}

	/* get the file info: note we cannot use
	 * g_file_input_stream_query_info_async since it is not able to get the
	 * content type etc, beside it is not supported by gvfs.
	 * Using the file instead of the stream is slightly racy, but for
	 * loading this is not too bad...
	 */
	g_file_query_info_async (loader->priv->location,
				 LOADER_QUERY_ATTRIBUTES,
                                 G_FILE_QUERY_INFO_NONE,
				 G_PRIORITY_HIGH,
				 async->cancellable,
				 (GAsyncReadyCallback) query_info_cb,
				 async);
}

static void
open_async_read (AsyncData *async)
{
	g_file_read_async (async->loader->priv->location,
	                   G_PRIORITY_HIGH,
	                   async->cancellable,
	                   (GAsyncReadyCallback) async_read_ready_callback,
	                   async);
}

void
gedit_document_loader_loading (GeditDocumentLoader *loader,
			       gboolean             completed,
			       GError              *error)
{
	/* the object will be unrefed in the callback of the loading signal
	 * (when completed == TRUE), so we need to prevent finalization.
	 */
	if (completed)
	{
		g_object_ref (loader);
	}

	g_signal_emit (loader, signals[LOADING], 0, completed, error);

	if (completed)
	{
		if (error == NULL)
		{
			DEBUG ({
			       g_print ("load completed\n");
			});
		}
		else
		{
			DEBUG ({
			       g_print ("load failed\n");
			});
		}

		g_object_unref (loader);
	}
}

void
gedit_document_loader_load (GeditDocumentLoader *loader)
{
	AsyncData *async;

	DEBUG ({
	       g_print ("%s\n", G_STRFUNC);
	});

	g_return_if_fail (GEDIT_IS_DOCUMENT_LOADER (loader));

	/* the loader can be used just once, then it must be thrown away */
	g_return_if_fail (loader->priv->used == FALSE);
	loader->priv->used = TRUE;

	/* make sure no load operation is currently running */
	g_return_if_fail (loader->priv->cancellable == NULL);

	/* loading start */
	gedit_document_loader_loading (loader,
				       FALSE,
				       NULL);

	loader->priv->cancellable = g_cancellable_new ();
	async = async_data_new (loader);

	if (loader->priv->stream)
	{
		loader->priv->guess_content_type_from_content = TRUE;
		loader->priv->info = g_file_info_new ();

		start_stream_read (async);
	}
	else
	{
		open_async_read (async);
	}
}

gboolean
gedit_document_loader_cancel (GeditDocumentLoader *loader)
{
	DEBUG ({
	       g_print ("%s\n", G_STRFUNC);
	});

	g_return_val_if_fail (GEDIT_IS_DOCUMENT_LOADER (loader), FALSE);

	if (loader->priv->cancellable == NULL)
		return FALSE;

	g_cancellable_cancel (loader->priv->cancellable);

	g_set_error (&loader->priv->error,
		     G_IO_ERROR,
		     G_IO_ERROR_CANCELLED,
		     "Operation cancelled");

	loader_load_completed_or_failed (loader, NULL);

	return TRUE;
}

GeditDocument *
gedit_document_loader_get_document (GeditDocumentLoader *loader)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT_LOADER (loader), NULL);

	return loader->priv->document;
}

GFile *
gedit_document_loader_get_location (GeditDocumentLoader *loader)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT_LOADER (loader), NULL);

	if (loader->priv->location)
	{
		return g_file_dup (loader->priv->location);
	}
	else
	{
		return NULL;
	}
}

goffset
gedit_document_loader_get_bytes_read (GeditDocumentLoader *loader)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT_LOADER (loader), 0);

	return loader->priv->bytes_read;
}

const GeditEncoding *
gedit_document_loader_get_encoding (GeditDocumentLoader *loader)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT_LOADER (loader), NULL);

	if (loader->priv->encoding != NULL)
		return loader->priv->encoding;

	g_return_val_if_fail (loader->priv->auto_detected_encoding != NULL,
			      gedit_encoding_get_current ());

	return loader->priv->auto_detected_encoding;
}

GeditDocumentNewlineType
gedit_document_loader_get_newline_type (GeditDocumentLoader *loader)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT_LOADER (loader),
			      GEDIT_DOCUMENT_NEWLINE_TYPE_LF);

	return loader->priv->auto_detected_newline_type;
}

GeditDocumentCompressionType
gedit_document_loader_get_compression_type (GeditDocumentLoader *loader)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT_LOADER (loader),
			      GEDIT_DOCUMENT_COMPRESSION_TYPE_NONE);

	return loader->priv->auto_detected_compression_type;
}

GFileInfo *
gedit_document_loader_get_info (GeditDocumentLoader *loader)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT_LOADER (loader), NULL);

	return loader->priv->info;
}

/* ex:set ts=8 noet: */
