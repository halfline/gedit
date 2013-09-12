/*
 * gedit-document-output-stream.c
 * This file is part of gedit
 *
 * Copyright (C) 2010 - Ignacio Casal Quinteiro
 *
 * gedit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * gedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gedit; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <errno.h>
#include "gedit-document-output-stream.h"

/* NOTE: never use async methods on this stream, the stream is just
 * a wrapper around GtkTextBuffer api so that we can use GIO Stream
 * methods, but the undelying code operates on a GtkTextBuffer, so
 * there is no I/O involved and should be accessed only by the main
 * thread */

/* NOTE2: welcome to a really big headache. At the beginning this was
 * splitted in several classes, one for encoding detection, another
 * for UTF-8 conversion and another for validation. The reason this is
 * all together is because we need specific information from all parts
 * in other to be able to mark characters as invalid if there was some
 * specific problem on the conversion */

#if 0
#define DEBUG(x) (x)
#else
#define DEBUG(x)
#endif

#define MAX_UNICHAR_LEN 6

struct _GeditDocumentOutputStreamPrivate
{
	GeditDocument *doc;
	GtkTextIter    pos;

	gchar *buffer;
	gsize buflen;

	gchar *iconv_buffer;
	gsize iconv_buflen;

	/* Encoding detection */
	GIConv iconv;
	GCharsetConverter *charset_conv;

	GSList *encodings;
	GSList *current_encoding;

	gint error_offset;
	guint n_fallback_errors;

	guint is_utf8 : 1;
	guint use_first : 1;

	guint is_initialized : 1;
	guint is_closed : 1;

	guint ensure_trailing_newline : 1;
};

enum
{
	PROP_0,
	PROP_DOCUMENT,
	PROP_ENSURE_TRAILING_NEWLINE
};

G_DEFINE_TYPE_WITH_PRIVATE (GeditDocumentOutputStream, gedit_document_output_stream, G_TYPE_OUTPUT_STREAM)

static gssize gedit_document_output_stream_write   (GOutputStream  *stream,
                                                    const void     *buffer,
                                                    gsize           count,
                                                    GCancellable   *cancellable,
                                                    GError        **error);

static gboolean gedit_document_output_stream_close (GOutputStream  *stream,
                                                    GCancellable   *cancellable,
                                                    GError        **error);

static gboolean gedit_document_output_stream_flush (GOutputStream  *stream,
                                                    GCancellable   *cancellable,
                                                    GError        **error);

static void
gedit_document_output_stream_set_property (GObject      *object,
					   guint         prop_id,
					   const GValue *value,
					   GParamSpec   *pspec)
{
	GeditDocumentOutputStream *stream = GEDIT_DOCUMENT_OUTPUT_STREAM (object);

	switch (prop_id)
	{
		case PROP_DOCUMENT:
			stream->priv->doc = GEDIT_DOCUMENT (g_value_get_object (value));
			break;

		case PROP_ENSURE_TRAILING_NEWLINE:
			stream->priv->ensure_trailing_newline = g_value_get_boolean (value);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_document_output_stream_get_property (GObject    *object,
					   guint       prop_id,
					   GValue     *value,
					   GParamSpec *pspec)
{
	GeditDocumentOutputStream *stream = GEDIT_DOCUMENT_OUTPUT_STREAM (object);

	switch (prop_id)
	{
		case PROP_DOCUMENT:
			g_value_set_object (value, stream->priv->doc);
			break;

		case PROP_ENSURE_TRAILING_NEWLINE:
			g_value_set_boolean (value, stream->priv->ensure_trailing_newline);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_document_output_stream_dispose (GObject *object)
{
	GeditDocumentOutputStream *stream = GEDIT_DOCUMENT_OUTPUT_STREAM (object);

	g_clear_object (&stream->priv->charset_conv);

	G_OBJECT_CLASS (gedit_document_output_stream_parent_class)->dispose (object);
}

static void
gedit_document_output_stream_finalize (GObject *object)
{
	GeditDocumentOutputStream *stream = GEDIT_DOCUMENT_OUTPUT_STREAM (object);

	g_free (stream->priv->buffer);
	g_free (stream->priv->iconv_buffer);
	g_slist_free (stream->priv->encodings);

	G_OBJECT_CLASS (gedit_document_output_stream_parent_class)->finalize (object);
}

static void
gedit_document_output_stream_constructed (GObject *object)
{
	GeditDocumentOutputStream *stream = GEDIT_DOCUMENT_OUTPUT_STREAM (object);

	if (!stream->priv->doc)
	{
		g_critical ("This should never happen, a problem happened constructing the Document Output Stream!");
		return;
	}

	/* Init the undoable action */
	gtk_source_buffer_begin_not_undoable_action (GTK_SOURCE_BUFFER (stream->priv->doc));
	/* clear the buffer */
	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (stream->priv->doc),
				  "", 0);
	gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (stream->priv->doc),
				      FALSE);

	gtk_source_buffer_end_not_undoable_action (GTK_SOURCE_BUFFER (stream->priv->doc));

	G_OBJECT_CLASS (gedit_document_output_stream_parent_class)->constructed (object);
}

static void
gedit_document_output_stream_class_init (GeditDocumentOutputStreamClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GOutputStreamClass *stream_class = G_OUTPUT_STREAM_CLASS (klass);

	object_class->get_property = gedit_document_output_stream_get_property;
	object_class->set_property = gedit_document_output_stream_set_property;
	object_class->dispose = gedit_document_output_stream_dispose;
	object_class->finalize = gedit_document_output_stream_finalize;
	object_class->constructed = gedit_document_output_stream_constructed;

	stream_class->write_fn = gedit_document_output_stream_write;
	stream_class->close_fn = gedit_document_output_stream_close;
	stream_class->flush = gedit_document_output_stream_flush;

	g_object_class_install_property (object_class,
					 PROP_DOCUMENT,
					 g_param_spec_object ("document",
							      "Document",
							      "The document which is written",
							      GEDIT_TYPE_DOCUMENT,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));

	/**
	 * GeditDocumentOutputStream:ensure-trailing-newline:
	 *
	 * The :ensure-trailing-newline property specifies whether or not to
	 * ensure (enforce) the document ends with a trailing newline.
	 */
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
}

static void
gedit_document_output_stream_init (GeditDocumentOutputStream *stream)
{
	stream->priv = gedit_document_output_stream_get_instance_private (stream);

	stream->priv->buffer = NULL;
	stream->priv->buflen = 0;

	stream->priv->charset_conv = NULL;
	stream->priv->encodings = NULL;
	stream->priv->current_encoding = NULL;

	stream->priv->error_offset = -1;

	stream->priv->is_initialized = FALSE;
	stream->priv->is_closed = FALSE;
	stream->priv->is_utf8 = FALSE;
	stream->priv->use_first = FALSE;
}

static const GeditEncoding *
get_encoding (GeditDocumentOutputStream *stream)
{
	if (stream->priv->current_encoding == NULL)
	{
		stream->priv->current_encoding = stream->priv->encodings;
	}
	else
	{
		stream->priv->current_encoding = g_slist_next (stream->priv->current_encoding);
	}

	if (stream->priv->current_encoding != NULL)
	{
		return (const GeditEncoding *)stream->priv->current_encoding->data;
	}

	stream->priv->use_first = TRUE;
	stream->priv->current_encoding = stream->priv->encodings;

	return (const GeditEncoding *)stream->priv->current_encoding->data;
}

static gboolean
try_convert (GCharsetConverter *converter,
             const void        *inbuf,
             gsize              inbuf_size)
{
	GError *err;
	gsize bytes_read, nread;
	gsize bytes_written, nwritten;
	GConverterResult res;
	gchar *out;
	gboolean ret;
	gsize out_size;

	if (inbuf == NULL || inbuf_size == 0)
	{
		return FALSE;
	}

	err = NULL;
	nread = 0;
	nwritten = 0;
	out_size = inbuf_size * 4;
	out = g_malloc (out_size);

	do
	{
		res = g_converter_convert (G_CONVERTER (converter),
		                           (gchar *)inbuf + nread,
		                           inbuf_size - nread,
		                           (gchar *)out + nwritten,
		                           out_size - nwritten,
		                           G_CONVERTER_INPUT_AT_END,
		                           &bytes_read,
		                           &bytes_written,
		                           &err);

		nread += bytes_read;
		nwritten += bytes_written;
	} while (res != G_CONVERTER_FINISHED && res != G_CONVERTER_ERROR && err == NULL);

	if (err != NULL)
	{
		if (err->code == G_CONVERT_ERROR_PARTIAL_INPUT)
		{
			/* FIXME We can get partial input while guessing the
			   encoding because we just take some amount of text
			   to guess from. */
			ret = TRUE;
		}
		else
		{
			ret = FALSE;
		}

		g_error_free (err);
	}
	else
	{
		ret = TRUE;
	}

	/* FIXME: Check the remainder? */
	if (ret == TRUE && !g_utf8_validate (out, nwritten, NULL))
	{
		ret = FALSE;
	}

	g_free (out);

	return ret;
}

static GCharsetConverter *
guess_encoding (GeditDocumentOutputStream *stream,
		const void                *inbuf,
		gsize                      inbuf_size)
{
	GCharsetConverter *conv = NULL;

	if (inbuf == NULL || inbuf_size == 0)
	{
		stream->priv->is_utf8 = TRUE;
		return NULL;
	}

	if (stream->priv->encodings != NULL &&
	    stream->priv->encodings->next == NULL)
	{
		stream->priv->use_first = TRUE;
	}

	/* We just check the first block */
	while (TRUE)
	{
		const GeditEncoding *enc;

		if (conv != NULL)
		{
			g_object_unref (conv);
			conv = NULL;
		}

		/* We get an encoding from the list */
		enc = get_encoding (stream);

		/* if it is NULL we didn't guess anything */
		if (enc == NULL)
		{
			break;
		}

		DEBUG ({
		       g_print ("trying charset: %s\n",
				gedit_encoding_get_charset (stream->priv->current_encoding->data));
		});

		if (enc == gedit_encoding_get_utf8 ())
		{
			gsize remainder;
			const gchar *end;

			if (g_utf8_validate (inbuf, inbuf_size, &end) ||
			    stream->priv->use_first)
			{
				stream->priv->is_utf8 = TRUE;
				break;
			}

			/* Check if the end is less than one char */
			remainder = inbuf_size - (end - (gchar *)inbuf);
			if (remainder < 6)
			{
				stream->priv->is_utf8 = TRUE;
				break;
			}

			continue;
		}

		conv = g_charset_converter_new ("UTF-8",
						gedit_encoding_get_charset (enc),
						NULL);

		/* If we tried all encodings we use the first one */
		if (stream->priv->use_first)
		{
			break;
		}

		/* Try to convert */
		if (try_convert (conv, inbuf, inbuf_size))
		{
			break;
		}
	}

	if (conv != NULL)
	{
		g_converter_reset (G_CONVERTER (conv));
	}

	return conv;
}

static GeditDocumentNewlineType
get_newline_type (GtkTextIter *end)
{
	GeditDocumentNewlineType res;
	GtkTextIter copy;
	gunichar c;

	copy = *end;
	c = gtk_text_iter_get_char (&copy);

	if (g_unichar_break_type (c) == G_UNICODE_BREAK_CARRIAGE_RETURN)
	{
		if (gtk_text_iter_forward_char (&copy) &&
		    g_unichar_break_type (gtk_text_iter_get_char (&copy)) == G_UNICODE_BREAK_LINE_FEED)
		{
			res = GEDIT_DOCUMENT_NEWLINE_TYPE_CR_LF;
		}
		else
		{
			res = GEDIT_DOCUMENT_NEWLINE_TYPE_CR;
		}
	}
	else
	{
		res = GEDIT_DOCUMENT_NEWLINE_TYPE_LF;
	}

	return res;
}

GOutputStream *
gedit_document_output_stream_new (GeditDocument *doc,
                                  GSList        *candidate_encodings,
                                  gboolean       ensure_trailing_newline)
{
	GeditDocumentOutputStream *stream;

	stream = g_object_new (GEDIT_TYPE_DOCUMENT_OUTPUT_STREAM,
	                       "document", doc,
	                       "ensure-trailing-newline", ensure_trailing_newline,
	                       NULL);

	stream->priv->encodings = g_slist_copy (candidate_encodings);

	return G_OUTPUT_STREAM (stream);
}

GeditDocumentNewlineType
gedit_document_output_stream_detect_newline_type (GeditDocumentOutputStream *stream)
{
	GeditDocumentNewlineType type;
	GtkTextIter iter;

	g_return_val_if_fail (GEDIT_IS_DOCUMENT_OUTPUT_STREAM (stream),
			      GEDIT_DOCUMENT_NEWLINE_TYPE_DEFAULT);

	type = GEDIT_DOCUMENT_NEWLINE_TYPE_DEFAULT;

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (stream->priv->doc),
					&iter);

	if (gtk_text_iter_ends_line (&iter) || gtk_text_iter_forward_to_line_end (&iter))
	{
		type = get_newline_type (&iter);
	}

	return type;
}

const GeditEncoding *
gedit_document_output_stream_get_guessed (GeditDocumentOutputStream *stream)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT_OUTPUT_STREAM (stream), NULL);

	if (stream->priv->current_encoding != NULL)
	{
		return (const GeditEncoding *)stream->priv->current_encoding->data;
	}
	else if (stream->priv->is_utf8 || !stream->priv->is_initialized)
	{
		/* If it is not initialized we assume that we are trying to convert
		   the empty string */
		return gedit_encoding_get_utf8 ();
	}

	return NULL;
}

guint
gedit_document_output_stream_get_num_fallbacks (GeditDocumentOutputStream *stream)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT_OUTPUT_STREAM (stream), 0);

	return stream->priv->n_fallback_errors;
}

static void
apply_error_tag (GeditDocumentOutputStream *stream)
{
	GtkTextIter start;

	if (stream->priv->error_offset == -1)
	{
		return;
	}

	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (stream->priv->doc),
	                                    &start, stream->priv->error_offset);

	_gedit_document_apply_error_style (stream->priv->doc,
	                                   &start, &stream->priv->pos);

	stream->priv->error_offset = -1;
}

static void
insert_fallback (GeditDocumentOutputStream *stream,
                 const gchar               *buffer)
{
	guint8 out[4];
	guint8 v;
	const gchar hex[] = "0123456789ABCDEF";

	/* if we are here is because we are pointing to an invalid char
	 * so we substitute it by an hex value */
	v = *(guint8 *)buffer;
	out[0] = '\\';
	out[1] = hex[(v & 0xf0) >> 4];
	out[2] = hex[(v & 0x0f) >> 0];
	out[3] = '\0';

	gtk_text_buffer_insert (GTK_TEXT_BUFFER (stream->priv->doc),
	                        &stream->priv->pos, (const gchar *)out, 3);

	++stream->priv->n_fallback_errors;
}

static void
validate_and_insert (GeditDocumentOutputStream *stream,
                     const gchar               *buffer,
                     gsize                      count)
{
	GtkTextBuffer *text_buffer;
	GtkTextIter   *iter;
	gsize len;

	text_buffer = GTK_TEXT_BUFFER (stream->priv->doc);
	iter = &stream->priv->pos;
	len = count;

	while (len != 0)
	{
		const gchar *end;
		gboolean valid;
		gsize nvalid;

		/* validate */
		valid = g_utf8_validate (buffer, len, &end);
		nvalid = end - buffer;

		/* Note: this is a workaround for a 'bug' in GtkTextBuffer where
		   inserting first a \r and then in a second insert, a \n,
		   will result in two lines being added instead of a single
		   one */

		if (valid)
		{
			gchar *ptr;

			ptr = g_utf8_find_prev_char (buffer, buffer + len);

			if (ptr && *ptr == '\r' && ptr - buffer == len - 1)
			{
				stream->priv->buffer = g_new (gchar, 1);
				stream->priv->buffer[0] = '\r';
				stream->priv->buflen = 1;

				/* Decrease also the len so in the check
				   nvalid == len we get out of this method */
				--nvalid;
				--len;
			}
		}

		/* if we've got any valid char we must tag the invalid chars */
		if (nvalid > 0)
		{
			apply_error_tag (stream);
		}

		gtk_text_buffer_insert (text_buffer, iter, buffer, nvalid);

		/* If we inserted all return */
		if (nvalid == len)
		{
			break;
		}

		buffer += nvalid;
		len = len - nvalid;

		if ((len < MAX_UNICHAR_LEN) &&
		    (g_utf8_get_char_validated (buffer, len) == (gunichar)-2))
		{
			stream->priv->buffer = g_strndup (end, len);
			stream->priv->buflen = len;

			break;
		}

		/* we need the start of the chunk of invalid chars */
		if (stream->priv->error_offset == -1)
		{
			stream->priv->error_offset = gtk_text_iter_get_offset (&stream->priv->pos);
		}

		insert_fallback (stream, buffer);
		++buffer;
		--len;
	}
}

/* If the last char is a newline, remove it from the buffer (otherwise
   GtkTextView shows it as an empty line). See bug #324942. */
static void
remove_ending_newline (GeditDocumentOutputStream *stream)
{
	GtkTextIter end;
	GtkTextIter start;

	gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (stream->priv->doc), &end);
	start = end;

	gtk_text_iter_set_line_offset (&start, 0);

	if (gtk_text_iter_ends_line (&start) &&
	    gtk_text_iter_backward_line (&start))
	{
		if (!gtk_text_iter_ends_line (&start))
		{
			gtk_text_iter_forward_to_line_end (&start);
		}

		/* Delete the empty line which is from 'start' to 'end' */
		gtk_text_buffer_delete (GTK_TEXT_BUFFER (stream->priv->doc),
		                        &start,
		                        &end);
	}
}

static void
end_append_text_to_document (GeditDocumentOutputStream *stream)
{
	if (stream->priv->ensure_trailing_newline)
	{
		remove_ending_newline (stream);
	}

	gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (stream->priv->doc),
	                              FALSE);

	gtk_source_buffer_end_not_undoable_action (GTK_SOURCE_BUFFER (stream->priv->doc));
}

static gboolean
convert_text (GeditDocumentOutputStream *stream,
              const gchar               *inbuf,
              gsize                      inbuf_len,
              gchar                    **outbuf,
              gsize                     *outbuf_len,
              GError                   **error)
{
	gchar *out, *dest;
	gsize in_left, out_left, outbuf_size, res;
	gint errsv;
	gboolean done, have_error;

	in_left = inbuf_len;
	/* set an arbitrary length if inbuf_len is 0, this is needed to flush
	   the iconv data */
	outbuf_size = (inbuf_len > 0) ? inbuf_len : 100;

	out_left = outbuf_size;
	out = dest = g_malloc (outbuf_size);

	done = FALSE;
	have_error = FALSE;

	while (!done && !have_error)
	{
		/* If we reached here is because we need to convert the text,
		   so we convert it using iconv.
		   See that if inbuf is NULL the data will be flushed */
		res = g_iconv (stream->priv->iconv,
		               (gchar **)&inbuf, &in_left,
		               &out, &out_left);

		/* something went wrong */
		if (res == (gsize)-1)
		{
			errsv = errno;

			switch (errsv)
			{
				case EINVAL:
					/* Incomplete text, do not report an error */
					stream->priv->iconv_buffer = g_strndup (inbuf, in_left);
					stream->priv->iconv_buflen = in_left;
					done = TRUE;
					break;
				case E2BIG:
					{
						/* allocate more space */
						gsize used = out - dest;

						outbuf_size *= 2;
						dest = g_realloc (dest, outbuf_size);

						out = dest + used;
						out_left = outbuf_size - used;
					}
					break;
				case EILSEQ:
					/* TODO: we should escape this text.*/
					g_set_error_literal (error, G_CONVERT_ERROR,
					                     G_CONVERT_ERROR_ILLEGAL_SEQUENCE,
					                     _("Invalid byte sequence in conversion input"));
					have_error = TRUE;
					break;
				default:
					g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_FAILED,
					             _("Error during conversion: %s"),
					             g_strerror (errsv));
					have_error = TRUE;
					break;
			}
		}
		else
		{
			done = TRUE;
		}
	}

	if (have_error)
	{
		g_free (dest);
		*outbuf = NULL;
		*outbuf_len = 0;

		return FALSE;
	}

	*outbuf = dest;
	*outbuf_len = out - dest;

	return TRUE;
}

static gssize
gedit_document_output_stream_write (GOutputStream            *stream,
				    const void               *buffer,
				    gsize                     count,
				    GCancellable             *cancellable,
				    GError                  **error)
{
	GeditDocumentOutputStream *ostream;
	gchar *text;
	gsize len;
	gboolean freetext = FALSE;

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
	{
		return -1;
	}

	ostream = GEDIT_DOCUMENT_OUTPUT_STREAM (stream);

	if (!ostream->priv->is_initialized)
	{
		ostream->priv->charset_conv = guess_encoding (ostream, buffer, count);

		/* If we still have the previous case is that we didn't guess
		   anything */
		if (ostream->priv->charset_conv == NULL &&
		    !ostream->priv->is_utf8)
		{
			g_set_error_literal (error, GEDIT_DOCUMENT_ERROR,
			                     GEDIT_DOCUMENT_ERROR_ENCODING_AUTO_DETECTION_FAILED,
			                     _("It is not possible to detect the encoding automatically"));

			return -1;
		}

		/* Do not initialize iconv if we are not going to convert anything */
		if (!ostream->priv->is_utf8)
		{
			gchar *from_charset;

			/* Initialize iconv */
			g_object_get (G_OBJECT (ostream->priv->charset_conv),
				      "from-charset", &from_charset,
				      NULL);

			ostream->priv->iconv = g_iconv_open ("UTF-8", from_charset);

			if (ostream->priv->iconv == (GIConv)-1)
			{
				if (errno == EINVAL)
				{
					g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
						     _("Conversion from character set '%s' to 'UTF-8' is not supported"),
						     from_charset);
				}
				else
				{
					g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
						     _("Could not open converter from '%s' to 'UTF-8'"),
						     from_charset);
				}

				g_free (from_charset);
				g_clear_object (&ostream->priv->charset_conv);

				return -1;
			}

			g_free (from_charset);
		}

		/* Init the undoable action */
		gtk_source_buffer_begin_not_undoable_action (GTK_SOURCE_BUFFER (ostream->priv->doc));

		gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (ostream->priv->doc),
		                                &ostream->priv->pos);

		ostream->priv->is_initialized = TRUE;
	}

	if (ostream->priv->buflen > 0)
	{
		len = ostream->priv->buflen + count;
		text = g_malloc (len + 1);

		memcpy (text, ostream->priv->buffer, ostream->priv->buflen);
		memcpy (text + ostream->priv->buflen, buffer, count);

		text[len] = '\0';

		g_free (ostream->priv->buffer);

		ostream->priv->buffer = NULL;
		ostream->priv->buflen = 0;

		freetext = TRUE;
	}
	else
	{
		text = (gchar *) buffer;
		len = count;
	}

	if (!ostream->priv->is_utf8)
	{
		gchar *outbuf;
		gsize outbuf_len;

		/* check if iconv was correctly initializated, this shouldn't
		   happen but better be safe */
		if (ostream->priv->iconv == NULL)
		{
			g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
			                     _("Invalid object, not initialized"));

			if (freetext)
			{
				g_free (text);
			}

			return -1;
		}

		/* manage the previous conversion buffer */
		if (ostream->priv->iconv_buflen > 0)
		{
			gchar *text2;
			gsize len2;

			len2 = len + ostream->priv->iconv_buflen;
			text2 = g_malloc (len2 + 1);

			memcpy (text2, ostream->priv->iconv_buffer, ostream->priv->iconv_buflen);
			memcpy (text2 + ostream->priv->iconv_buflen, text, len);

			text2[len2] = '\0';

			if (freetext)
			{
				g_free (text);
			}

			text = text2;
			len = len2;

			g_free (ostream->priv->iconv_buffer);

			ostream->priv->iconv_buffer = NULL;
			ostream->priv->iconv_buflen = 0;

			freetext = TRUE;
		}

		if (!convert_text (ostream, text, len, &outbuf, &outbuf_len, error))
		{
			if (freetext)
			{
				g_free (text);
			}

			return -1;
		}

		if (freetext)
		{
			g_free (text);
		}

		/* set the converted text as the text to validate */
		text = outbuf;
		len = outbuf_len;
	}

	validate_and_insert (ostream, text, len);

	if (freetext)
	{
		g_free (text);
	}

	return count;
}

static gboolean
gedit_document_output_stream_flush (GOutputStream  *stream,
                                    GCancellable   *cancellable,
                                    GError        **error)
{
	GeditDocumentOutputStream *ostream;

	ostream = GEDIT_DOCUMENT_OUTPUT_STREAM (stream);

	if (ostream->priv->is_closed)
	{
		return TRUE;
	}

	/* if we have converted something flush residual data, validate and insert */
	if (ostream->priv->iconv != NULL)
	{
		gchar *outbuf;
		gsize outbuf_len;

		if (convert_text (ostream, NULL, 0, &outbuf, &outbuf_len, error))
		{
			validate_and_insert (ostream, outbuf, outbuf_len);
			g_free (outbuf);
		}
		else
		{
			return FALSE;
		}
	}

	if (ostream->priv->buflen > 0 && *ostream->priv->buffer != '\r')
	{
		/* If we reached here is because the last insertion was a half
		   correct char, which has to be inserted as fallback */
		gchar *text;

		if (ostream->priv->error_offset == -1)
		{
			ostream->priv->error_offset = gtk_text_iter_get_offset (&ostream->priv->pos);
		}

		text = ostream->priv->buffer;
		while (ostream->priv->buflen != 0)
		{
			insert_fallback (ostream, text);
			++text;
			--ostream->priv->buflen;
		}

		g_free (ostream->priv->buffer);
		ostream->priv->buffer = NULL;
	}
	else if (ostream->priv->buflen == 1 && *ostream->priv->buffer == '\r')
	{
		/* The previous chars can be invalid */
		apply_error_tag (ostream);

		/* See special case above, flush this */
		gtk_text_buffer_insert (GTK_TEXT_BUFFER (ostream->priv->doc),
		                        &ostream->priv->pos,
		                        "\r",
		                        1);

		g_free (ostream->priv->buffer);
		ostream->priv->buffer = NULL;
		ostream->priv->buflen = 0;
	}

	if (ostream->priv->iconv_buflen > 0 )
	{
		/* If we reached here is because the last insertion was a half
		   correct char, which has to be inserted as fallback */
		gchar *text;

		if (ostream->priv->error_offset == -1)
		{
			ostream->priv->error_offset = gtk_text_iter_get_offset (&ostream->priv->pos);
		}

		text = ostream->priv->iconv_buffer;
		while (ostream->priv->iconv_buflen != 0)
		{
			insert_fallback (ostream, text);
			++text;
			--ostream->priv->iconv_buflen;
		}

		g_free (ostream->priv->iconv_buffer);
		ostream->priv->iconv_buffer = NULL;
	}

	apply_error_tag (ostream);

	return TRUE;
}

static gboolean
gedit_document_output_stream_close (GOutputStream     *stream,
                                    GCancellable      *cancellable,
                                    GError           **error)
{
	GeditDocumentOutputStream *ostream = GEDIT_DOCUMENT_OUTPUT_STREAM (stream);

	if (!ostream->priv->is_closed && ostream->priv->is_initialized)
	{
		end_append_text_to_document (ostream);

		if (ostream->priv->iconv != NULL)
		{
			g_iconv_close (ostream->priv->iconv);
		}

		ostream->priv->is_closed = TRUE;
	}

	if (ostream->priv->buflen > 0 || ostream->priv->iconv_buflen > 0)
	{
		g_set_error (error,
		             G_IO_ERROR,
		             G_IO_ERROR_INVALID_DATA,
		             _("Incomplete UTF-8 sequence in input"));

		return FALSE;
	}

	return TRUE;
}

/* ex:set ts=8 noet: */
