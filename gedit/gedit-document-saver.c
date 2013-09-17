/*
 * gedit-document-saver.c
 * This file is part of gedit
 *
 * Copyright (C) 2005-2006 - Paolo Borelli and Paolo Maggi
 * Copyright (C) 2007 - Paolo Borelli, Paolo Maggi, Steve Frécinaux
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

#include <glib/gi18n.h>
#include <glib.h>
#include <gio/gio.h>
#include <string.h>

#include "gedit-document-saver.h"
#include "gedit-document.h"
#include "gedit/gedit-document-input-stream.h"
#include "gedit-marshal.h"
#include "gedit/gedit-utils.h"
#include "gedit-enum-types.h"

#define WRITE_CHUNK_SIZE 8192

#if 0
#define DEBUG(x) (x)
#else
#define DEBUG(x)
#endif

/* Signals */

enum {
	SAVING,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/* Properties */

enum {
	PROP_0,
	PROP_DOCUMENT,
	PROP_LOCATION,
	PROP_ENCODING,
	PROP_NEWLINE_TYPE,
	PROP_COMPRESSION_TYPE,
	PROP_FLAGS,
	PROP_ENSURE_TRAILING_NEWLINE
};

typedef struct
{
	GeditDocumentSaver    *saver;
	gchar 		       buffer[WRITE_CHUNK_SIZE];
	GCancellable 	      *cancellable;
	gboolean	       tried_mount;
	gssize		       written;
	gssize		       read;
	GError                *error;
} AsyncData;

#define REMOTE_QUERY_ATTRIBUTES G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE "," \
				G_FILE_ATTRIBUTE_TIME_MODIFIED

static void check_modified_async (AsyncData *async);

struct _GeditDocumentSaverPrivate
{
	GFileInfo		 *info;
	GeditDocument		 *document;

	GFile			 *location;
	const GeditEncoding      *encoding;
	GeditDocumentNewlineType  newline_type;
	GeditDocumentCompressionType compression_type;

	GeditDocumentSaveFlags    flags;

	GTimeVal		  old_mtime;

	goffset			  size;
	goffset			  bytes_written;

	GCancellable		 *cancellable;
	GOutputStream		 *stream;
	GInputStream		 *input;

	GError                   *error;

	GeditMountOperationFactory mount_operation_factory;
	gpointer                   mount_operation_userdata;

	guint                     used : 1;
	guint                     ensure_trailing_newline : 1;
};

G_DEFINE_TYPE_WITH_PRIVATE (GeditDocumentSaver, gedit_document_saver, G_TYPE_OBJECT)

static void
gedit_document_saver_set_property (GObject      *object,
				   guint         prop_id,
				   const GValue *value,
				   GParamSpec   *pspec)
{
	GeditDocumentSaver *saver = GEDIT_DOCUMENT_SAVER (object);

	switch (prop_id)
	{
		case PROP_DOCUMENT:
			g_return_if_fail (saver->priv->document == NULL);
			saver->priv->document = g_value_get_object (value);
			break;
		case PROP_LOCATION:
			g_return_if_fail (saver->priv->location == NULL);
			saver->priv->location = g_value_dup_object (value);
			break;
		case PROP_ENCODING:
			g_return_if_fail (saver->priv->encoding == NULL);
			saver->priv->encoding = g_value_get_boxed (value);
			break;
		case PROP_NEWLINE_TYPE:
			saver->priv->newline_type = g_value_get_enum (value);
			break;
		case PROP_COMPRESSION_TYPE:
			saver->priv->compression_type = g_value_get_enum (value);
			break;
		case PROP_FLAGS:
			saver->priv->flags = g_value_get_flags (value);
			break;
		case PROP_ENSURE_TRAILING_NEWLINE:
			saver->priv->ensure_trailing_newline = g_value_get_boolean (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_document_saver_get_property (GObject    *object,
				   guint       prop_id,
				   GValue     *value,
				   GParamSpec *pspec)
{
	GeditDocumentSaver *saver = GEDIT_DOCUMENT_SAVER (object);

	switch (prop_id)
	{
		case PROP_DOCUMENT:
			g_value_set_object (value, saver->priv->document);
			break;
		case PROP_LOCATION:
			g_value_set_object (value, saver->priv->location);
			break;
		case PROP_ENCODING:
			g_value_set_boxed (value, saver->priv->encoding);
			break;
		case PROP_NEWLINE_TYPE:
			g_value_set_enum (value, saver->priv->newline_type);
			break;
		case PROP_COMPRESSION_TYPE:
			g_value_set_enum (value, saver->priv->compression_type);
			break;
		case PROP_FLAGS:
			g_value_set_flags (value, saver->priv->flags);
			break;
		case PROP_ENSURE_TRAILING_NEWLINE:
			g_value_set_boolean (value, saver->priv->ensure_trailing_newline);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_document_saver_dispose (GObject *object)
{
	GeditDocumentSaverPrivate *priv = GEDIT_DOCUMENT_SAVER (object)->priv;

	if (priv->cancellable != NULL)
	{
		g_cancellable_cancel (priv->cancellable);
		g_object_unref (priv->cancellable);
		priv->cancellable = NULL;
	}

	g_clear_error (&priv->error);

	g_clear_object (&priv->stream);
	g_clear_object (&priv->input);
	g_clear_object (&priv->info);
	g_clear_object (&priv->location);

	G_OBJECT_CLASS (gedit_document_saver_parent_class)->dispose (object);
}

static AsyncData *
async_data_new (GeditDocumentSaver *saver)
{
	AsyncData *async;

	async = g_slice_new (AsyncData);
	async->saver = saver;
	async->cancellable = g_object_ref (saver->priv->cancellable);

	async->tried_mount = FALSE;
	async->written = 0;
	async->read = 0;

	async->error = NULL;

	return async;
}

static void
async_data_free (AsyncData *async)
{
	g_object_unref (async->cancellable);

	if (async->error)
	{
		g_error_free (async->error);
	}

	g_slice_free (AsyncData, async);
}

static void
gedit_document_saver_class_init (GeditDocumentSaverClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gedit_document_saver_dispose;
	object_class->set_property = gedit_document_saver_set_property;
	object_class->get_property = gedit_document_saver_get_property;

	g_object_class_install_property (object_class,
					 PROP_DOCUMENT,
					 g_param_spec_object ("document",
							      "Document",
							      "The GeditDocument this GeditDocumentSaver is associated with",
							      GEDIT_TYPE_DOCUMENT,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class,
					 PROP_LOCATION,
					 g_param_spec_object ("location",
							      "LOCATION",
							      "The LOCATION this GeditDocumentSaver saves the document to",
							      G_TYPE_FILE,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class,
					 PROP_ENCODING,
					 g_param_spec_boxed ("encoding",
							     "ENCODING",
							     "The encoding of the saved file",
							     GEDIT_TYPE_ENCODING,
							     G_PARAM_READWRITE |
							     G_PARAM_CONSTRUCT_ONLY |
							     G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class,
					 PROP_NEWLINE_TYPE,
					 g_param_spec_enum ("newline-type",
					                    "Newline type",
					                    "The accepted types of line ending",
					                    GEDIT_TYPE_DOCUMENT_NEWLINE_TYPE,
					                    GEDIT_DOCUMENT_NEWLINE_TYPE_LF,
					                    G_PARAM_READWRITE |
					                    G_PARAM_STATIC_NAME |
					                    G_PARAM_STATIC_BLURB |
					                    G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
					 PROP_COMPRESSION_TYPE,
					 g_param_spec_enum ("compression-type",
					                    "Compression type",
					                    "The compression type",
					                    GEDIT_TYPE_DOCUMENT_COMPRESSION_TYPE,
					                    GEDIT_DOCUMENT_COMPRESSION_TYPE_NONE,
					                    G_PARAM_READWRITE |
					                    G_PARAM_STATIC_NAME |
					                    G_PARAM_STATIC_BLURB |
					                    G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
					 PROP_FLAGS,
					 g_param_spec_flags ("flags",
							     "Flags",
							     "The flags for the saving operation",
							     GEDIT_TYPE_DOCUMENT_SAVE_FLAGS,
							     0,
							     G_PARAM_READWRITE |
							     G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
	                                 PROP_ENSURE_TRAILING_NEWLINE,
	                                 g_param_spec_boolean ("ensure-trailing-newline",
	                                                       "Ensure Trailing Newline",
	                                                       "Ensure the document ends with a trailing newline",
	                                                       TRUE,
	                                                       G_PARAM_READWRITE |
	                                                       G_PARAM_STATIC_NAME |
	                                                       G_PARAM_STATIC_BLURB |
	                                                       G_PARAM_CONSTRUCT_ONLY));

	signals[SAVING] =
		g_signal_new ("saving",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GeditDocumentSaverClass, saving),
			      NULL, NULL,
			      gedit_marshal_VOID__BOOLEAN_POINTER,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_BOOLEAN,
			      G_TYPE_POINTER);
}

static void
gedit_document_saver_init (GeditDocumentSaver *saver)
{
	saver->priv = gedit_document_saver_get_instance_private (saver);

	saver->priv->cancellable = g_cancellable_new ();
	saver->priv->error = NULL;
	saver->priv->used = FALSE;
}

GeditDocumentSaver *
gedit_document_saver_new (GeditDocument                *doc,
			  GFile                        *location,
			  const GeditEncoding          *encoding,
			  GeditDocumentNewlineType      newline_type,
			  GeditDocumentCompressionType  compression_type,
			  GeditDocumentSaveFlags        flags,
			  gboolean                      ensure_trailing_newline)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), NULL);

	if (encoding == NULL)
		encoding = gedit_encoding_get_utf8 ();

	return GEDIT_DOCUMENT_SAVER (g_object_new (GEDIT_TYPE_DOCUMENT_SAVER,
						   "document", doc,
						   "location", location,
						   "encoding", encoding,
						   "newline_type", newline_type,
						   "compression_type", compression_type,
						   "flags", flags,
						   "ensure-trailing-newline", ensure_trailing_newline,
						   NULL));
}

static void
remote_save_completed_or_failed (GeditDocumentSaver *saver,
				 AsyncData 	    *async)
{
	gedit_document_saver_saving (saver,
				     TRUE,
				     saver->priv->error);

	if (async)
		async_data_free (async);
}

static void
async_failed (AsyncData *async,
	      GError    *error)
{
	g_propagate_error (&async->saver->priv->error, error);
	remote_save_completed_or_failed (async->saver, async);
}

/* BEGIN NOTE:
 *
 * This fixes an issue in GOutputStream that applies the atomic replace
 * save strategy. The stream moves the written file to the original file
 * when the stream is closed. However, there is no way currently to tell
 * the stream that the save should be aborted (there could be a
 * conversion error). The patch explicitly closes the output stream
 * in all these cases with a GCancellable in the cancelled state, causing
 * the output stream to close, but not move the file. This makes use
 * of an implementation detail in the local  file stream and should be
 * properly fixed by adding the appropriate API in . Until then, at least
 * we prevent data corruption for now.
 *
 * Relevant bug reports:
 *
 * Bug 615110 - write file ignore encoding errors (gedit)
 * https://bugzilla.gnome.org/show_bug.cgi?id=615110
 *
 * Bug 602412 - g_file_replace does not restore original file when there is
 *              errors while writing (glib/)
 * https://bugzilla.gnome.org/show_bug.cgi?id=602412
 */
static void
cancel_output_stream_ready_cb (GOutputStream *stream,
                               GAsyncResult  *result,
                               AsyncData     *async)
{
	GError *error;

	g_output_stream_close_finish (stream, result, NULL);

	/* check cancelled state manually */
	if (g_cancellable_is_cancelled (async->cancellable) || async->error == NULL)
	{
		async_data_free (async);
		return;
	}

	error = async->error;
	async->error = NULL;

	async_failed (async, error);
}

static void
cancel_output_stream (AsyncData *async)
{
	GCancellable *cancellable;

	DEBUG ({
	       g_print ("Cancel output stream\n");
	});

	cancellable = g_cancellable_new ();
	g_cancellable_cancel (cancellable);

	g_output_stream_close_async (async->saver->priv->stream,
				     G_PRIORITY_HIGH,
				     cancellable,
				     (GAsyncReadyCallback)cancel_output_stream_ready_cb,
				     async);

	g_object_unref (cancellable);
}

static void
cancel_output_stream_and_fail (AsyncData *async,
                               GError    *error)
{
	DEBUG ({
	       g_print ("Cancel output stream and fail\n");
	});

	g_propagate_error (&async->error, error);
	cancel_output_stream (async);
}

/*
 * END NOTE
 */

static void
remote_get_info_cb (GFile        *source,
		    GAsyncResult *res,
		    AsyncData    *async)
{
	GeditDocumentSaver *saver;
	GFileInfo *info;
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

	saver = async->saver;

	DEBUG ({
	       g_print ("Finished query info on file\n");
	});

	info = g_file_query_info_finish (source, res, &error);

	if (info != NULL)
	{
		if (saver->priv->info != NULL)
			g_object_unref (saver->priv->info);

		saver->priv->info = info;
	}
	else
	{
		DEBUG ({
		       g_print ("Query info failed: %s\n", error->message);
		});

		g_propagate_error (&saver->priv->error, error);
	}

	remote_save_completed_or_failed (saver, async);
}

static void
close_async_ready_get_info_cb (GOutputStream *stream,
			       GAsyncResult  *res,
			       AsyncData     *async)
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
	       g_print ("Finished closing stream\n");
	});

	if (!g_output_stream_close_finish (stream, res, &error))
	{
		DEBUG ({
		       g_print ("Closing stream error: %s\n", error->message);
		});

		async_failed (async, error);
		return;
	}

	/* get the file info: note we cannot use
	 * g_file_output_stream_query_info_async since it is not able to get the
	 * content type etc, beside it is not supported by gvfs.
	 * I'm not sure this is actually necessary, can't we just use
	 * g_content_type_guess (since we have the file name and the data)
	 */
	DEBUG ({
	       g_print ("Query info on file\n");
	});

	g_file_query_info_async (async->saver->priv->location,
			         REMOTE_QUERY_ATTRIBUTES,
			         G_FILE_QUERY_INFO_NONE,
			         G_PRIORITY_HIGH,
			         async->cancellable,
			         (GAsyncReadyCallback) remote_get_info_cb,
			         async);
}

static void
write_complete (AsyncData *async)
{
	GError *error = NULL;

	/* first we close the input stream */
	DEBUG ({
	       g_print ("Close input stream\n");
	});

	if (!g_input_stream_close (async->saver->priv->input,
				   async->cancellable, &error))
	{
		DEBUG ({
		       g_print ("Closing input stream error: %s\n", error->message);
		});

		cancel_output_stream_and_fail (async, error);
		return;
	}

	/* now we close the output stream */
	DEBUG ({
	       g_print ("Close output stream\n");
	});

	g_output_stream_close_async (async->saver->priv->stream,
				     G_PRIORITY_HIGH,
				     async->cancellable,
				     (GAsyncReadyCallback)close_async_ready_get_info_cb,
				     async);
}

/* prototype, because they call each other... isn't C lovely */
static void read_file_chunk (AsyncData *async);
static void write_file_chunk (AsyncData *async);

static void
async_write_cb (GOutputStream *stream,
		GAsyncResult  *res,
		AsyncData     *async)
{
	GeditDocumentSaver *saver;
	gssize bytes_written;
	GError *error = NULL;

	DEBUG ({
	       g_print ("%s\n", G_STRFUNC);
	});

	/* Check cancelled state manually */
	if (g_cancellable_is_cancelled (async->cancellable))
	{
		cancel_output_stream (async);
		return;
	}

	bytes_written = g_output_stream_write_finish (stream, res, &error);

	DEBUG ({
	       g_print ("Written: %" G_GSSIZE_FORMAT "\n", bytes_written);
	});

	if (bytes_written == -1)
	{
		DEBUG ({
		       g_print ("Write error: %s\n", error->message);
		});

		cancel_output_stream_and_fail (async, error);
		return;
	}

	saver = async->saver;
	async->written += bytes_written;

	/* write again */
	if (async->written != async->read)
	{
		write_file_chunk (async);
		return;
	}

	/* note that this signal blocks the write... check if it isn't
	 * a performance problem
	 */
	gedit_document_saver_saving (saver,
				     FALSE,
				     NULL);

	read_file_chunk (async);
}

static void
write_file_chunk (AsyncData *async)
{
	GeditDocumentSaver *saver;

	DEBUG ({
	       g_print ("%s\n", G_STRFUNC);
	});

	saver = async->saver;

	g_output_stream_write_async (G_OUTPUT_STREAM (saver->priv->stream),
				     async->buffer + async->written,
				     async->read - async->written,
				     G_PRIORITY_HIGH,
				     async->cancellable,
				     (GAsyncReadyCallback) async_write_cb,
				     async);
}

static void
read_file_chunk (AsyncData *async)
{
	GeditDocumentSaver *saver;
	GeditDocumentInputStream *dstream;
	GError *error = NULL;

	DEBUG ({
	       g_print ("%s\n", G_STRFUNC);
	});

	saver = async->saver;
	async->written = 0;

	/* we use sync methods on doc stream since it is in memory. Using async
	   would be racy and we can endup with invalidated iters */
	async->read = g_input_stream_read (saver->priv->input,
					   async->buffer,
					   WRITE_CHUNK_SIZE,
					   async->cancellable,
					   &error);

	if (error != NULL)
	{
		cancel_output_stream_and_fail (async, error);
		return;
	}

	/* Check if we finished reading and writing */
	if (async->read == 0)
	{
		write_complete (async);
		return;
	}

	/* Get how many chars have been read */
	dstream = GEDIT_DOCUMENT_INPUT_STREAM (saver->priv->input);
	saver->priv->bytes_written = gedit_document_input_stream_tell (dstream);

	write_file_chunk (async);
}

static void
async_replace_ready_callback (GFile        *source,
			      GAsyncResult *res,
			      AsyncData    *async)
{
	GeditDocumentSaver *saver;
	GCharsetConverter *converter;
	GFileOutputStream *file_stream;
	GOutputStream *base_stream;
	GError *error = NULL;

	DEBUG ({
	       g_print ("%s\n", G_STRFUNC);
	});

	/* Check cancelled state manually */
	if (g_cancellable_is_cancelled (async->cancellable))
	{
		async_data_free (async);
		return;
	}

	saver = async->saver;
	file_stream = g_file_replace_finish (source, res, &error);

	/* handle any error that might occur */
	if (!file_stream)
	{
		DEBUG ({
		       g_print ("Opening file failed: %s\n", error->message);
		});

		async_failed (async, error);
		return;
	}

	if (saver->priv->compression_type == GEDIT_DOCUMENT_COMPRESSION_TYPE_GZIP)
	{
		GZlibCompressor *compressor;

		DEBUG ({
		       g_print ("Use gzip compressor\n");
		});

		compressor = g_zlib_compressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP,
		                                    -1);

		base_stream = g_converter_output_stream_new (G_OUTPUT_STREAM (file_stream),
		                                             G_CONVERTER (compressor));

		g_object_unref (compressor);
		g_object_unref (file_stream);
	}
	else
	{
		base_stream = G_OUTPUT_STREAM (file_stream);
	}

	/* FIXME: manage converter error? */
	DEBUG ({
	       g_print ("Encoding charset: %s\n",
			gedit_encoding_get_charset (saver->priv->encoding));
	});

	if (saver->priv->encoding != gedit_encoding_get_utf8 ())
	{
		converter = g_charset_converter_new (gedit_encoding_get_charset (saver->priv->encoding),
						     "UTF-8",
						     NULL);

		saver->priv->stream = g_converter_output_stream_new (base_stream,
		                                                     G_CONVERTER (converter));

		g_object_unref (converter);
		g_object_unref (base_stream);
	}
	else
	{
		saver->priv->stream = G_OUTPUT_STREAM (base_stream);
	}

	saver->priv->input = gedit_document_input_stream_new (GTK_TEXT_BUFFER (saver->priv->document),
							      saver->priv->newline_type,
							      saver->priv->ensure_trailing_newline);

	saver->priv->size = gedit_document_input_stream_get_total_size (GEDIT_DOCUMENT_INPUT_STREAM (saver->priv->input));

	read_file_chunk (async);
}

static void
begin_write (AsyncData *async)
{
	GeditDocumentSaver *saver;
	gboolean make_backup;

	DEBUG ({
	       g_print ("Start replacing file contents\n");
	});

	/* For remote files we simply use g_file_replace_async. There is no
	 * backup as of yet
	 */
	saver = async->saver;

	make_backup = (saver->priv->flags & GEDIT_DOCUMENT_SAVE_IGNORE_BACKUP) == 0 &&
		      (saver->priv->flags & GEDIT_DOCUMENT_SAVE_PRESERVE_BACKUP) == 0;

	DEBUG ({
	       g_print ("File contents size: %" G_GINT64_FORMAT "\n", saver->priv->size);
	       g_print ("Make backup: %s\n", make_backup ? "yes" : "no");
	});

	g_file_replace_async (saver->priv->location,
			      NULL,
			      make_backup,
			      G_FILE_CREATE_NONE,
			      G_PRIORITY_HIGH,
			      async->cancellable,
			      (GAsyncReadyCallback) async_replace_ready_callback,
			      async);
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
		/* try again to get the modified state */
		check_modified_async (async);
	}
}

static GMountOperation *
create_mount_operation (GeditDocumentSaver *saver)
{
	if (saver->priv->mount_operation_factory == NULL)
	{
		return g_mount_operation_new ();
	}
	else
	{
		return saver->priv->mount_operation_factory (saver->priv->mount_operation_userdata);
	}
}

static void
recover_not_mounted (AsyncData *async)
{
	GMountOperation *mount_operation = create_mount_operation (async->saver);

	DEBUG ({
	       g_print ("%s\n", G_STRFUNC);
	});

	async->tried_mount = TRUE;
	g_file_mount_enclosing_volume (async->saver->priv->location,
				       G_MOUNT_MOUNT_NONE,
				       mount_operation,
				       async->cancellable,
				       (GAsyncReadyCallback) mount_ready_callback,
				       async);

	g_object_unref (mount_operation);
}

static void
check_modification_callback (GFile        *source,
			     GAsyncResult *res,
			     AsyncData    *async)
{
	GeditDocumentSaver *saver;
	GError *error = NULL;
	GFileInfo *info;

	DEBUG ({
	       g_print ("%s\n", G_STRFUNC);
	});

	/* manually check cancelled state */
	if (g_cancellable_is_cancelled (async->cancellable))
	{
		async_data_free (async);
		return;
	}

	saver = async->saver;
	info = g_file_query_info_finish (source, res, &error);
	if (info == NULL)
	{
		if (error->code == G_IO_ERROR_NOT_MOUNTED && !async->tried_mount)
		{
			recover_not_mounted (async);
			g_error_free (error);
			return;
		}

		/* it's perfectly fine if the file doesn't exist yet */
		if (error->code != G_IO_ERROR_NOT_FOUND)
		{
			DEBUG ({
			       g_print ("Error getting modification: %s\n", error->message);
			});

			async_failed (async, error);
			return;
		}
	}

	/* check if the mtime is > what we know about it (if we have it) */
	if (info != NULL && g_file_info_has_attribute (info,
				       G_FILE_ATTRIBUTE_TIME_MODIFIED))
	{
		GTimeVal mtime;
		GTimeVal old_mtime;

		g_file_info_get_modification_time (info, &mtime);
		old_mtime = saver->priv->old_mtime;

		if ((old_mtime.tv_sec > 0 || old_mtime.tv_usec > 0) &&
		    (mtime.tv_sec != old_mtime.tv_sec || mtime.tv_usec != old_mtime.tv_usec) &&
		    (saver->priv->flags & GEDIT_DOCUMENT_SAVE_IGNORE_MTIME) == 0)
		{
			DEBUG ({
			       g_print ("File is externally modified\n");
			});

			g_set_error (&saver->priv->error,
				     GEDIT_DOCUMENT_ERROR,
				     GEDIT_DOCUMENT_ERROR_EXTERNALLY_MODIFIED,
				     "Externally modified");

			remote_save_completed_or_failed (saver, async);
			g_object_unref (info);

			return;
		}
	}

	if (info != NULL)
		g_object_unref (info);

	/* modification check passed, start write */
	begin_write (async);
}

static void
check_modified_async (AsyncData *async)
{
	DEBUG ({
	       g_print ("Check externally modified\n");
	});

	g_file_query_info_async (async->saver->priv->location,
				 G_FILE_ATTRIBUTE_TIME_MODIFIED,
				 G_FILE_QUERY_INFO_NONE,
				 G_PRIORITY_HIGH,
				 async->cancellable,
				 (GAsyncReadyCallback) check_modification_callback,
				 async);
}

static gboolean
save_remote_file_real (GeditDocumentSaver *saver)
{
	AsyncData *async;

	DEBUG ({
	       g_print ("Starting  save\n");
	});

	/* First find out if the file is modified externally. This requires
	 * a stat, but I don't think we can do this any other way
	 */
	async = async_data_new (saver);

	check_modified_async (async);

	/* return false to stop timeout */
	return FALSE;
}

void
gedit_document_saver_save (GeditDocumentSaver *saver,
			   GTimeVal           *old_mtime)
{
	DEBUG ({
	       g_print ("%s\n", G_STRFUNC);
	});

	g_return_if_fail (GEDIT_IS_DOCUMENT_SAVER (saver));
	g_return_if_fail (saver->priv->location != NULL);

	g_return_if_fail (saver->priv->used == FALSE);
	saver->priv->used = TRUE;

	/* CHECK:
	 report async (in an idle handler) or sync (bool ret)
	 async is extra work here, sync is special casing in the caller */

	saver->priv->old_mtime = *old_mtime;

	/* saving start */
	gedit_document_saver_saving (saver, FALSE, NULL);

	g_timeout_add_full (G_PRIORITY_HIGH,
			    0,
			    (GSourceFunc) save_remote_file_real,
			    saver,
			    NULL);
}

void
gedit_document_saver_saving (GeditDocumentSaver *saver,
			     gboolean            completed,
			     GError             *error)
{
	/* the object will be unrefed in the callback of the saving
	 * signal, so we need to prevent finalization.
	 */
	if (completed)
	{
		g_object_ref (saver);
	}

	g_signal_emit (saver, signals[SAVING], 0, completed, error);

	if (completed)
	{
		if (error == NULL)
		{
			DEBUG ({
			       g_print ("save completed\n");
			});
		}
		else
		{
			DEBUG ({
			       g_print ("save failed\n");
			});
		}

		g_object_unref (saver);
	}
}

GeditDocument *
gedit_document_saver_get_document (GeditDocumentSaver *saver)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT_SAVER (saver), NULL);

	return saver->priv->document;
}

GFile *
gedit_document_saver_get_location (GeditDocumentSaver *saver)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT_SAVER (saver), NULL);

	return g_file_dup (saver->priv->location);
}

/* Returns 0 if file size is unknown */
goffset
gedit_document_saver_get_file_size (GeditDocumentSaver *saver)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT_SAVER (saver), 0);

	return saver->priv->size;
}

goffset
gedit_document_saver_get_bytes_written (GeditDocumentSaver *saver)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT_SAVER (saver), 0);

	return saver->priv->bytes_written;
}

GFileInfo *
gedit_document_saver_get_info (GeditDocumentSaver *saver)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT_SAVER (saver), NULL);

	return saver->priv->info;
}

void
gedit_document_saver_set_mount_operation_factory (GeditDocumentSaver         *saver,
						  GeditMountOperationFactory  callback,
						  gpointer                    user_data)
{
	g_return_if_fail (GEDIT_IS_DOCUMENT_SAVER (saver));

	saver->priv->mount_operation_factory = callback;
	saver->priv->mount_operation_userdata = user_data;
}

/* ex:set ts=8 noet: */
