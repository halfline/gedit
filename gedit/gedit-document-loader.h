/*
 * gedit-document-loader.h
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

#ifndef __GEDIT_DOCUMENT_LOADER_H__
#define __GEDIT_DOCUMENT_LOADER_H__

#include <gedit/gedit-document.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_DOCUMENT_LOADER              (gedit_document_loader_get_type())
#define GEDIT_DOCUMENT_LOADER(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GEDIT_TYPE_DOCUMENT_LOADER, GeditDocumentLoader))
#define GEDIT_DOCUMENT_LOADER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GEDIT_TYPE_DOCUMENT_LOADER, GeditDocumentLoaderClass))
#define GEDIT_IS_DOCUMENT_LOADER(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GEDIT_TYPE_DOCUMENT_LOADER))
#define GEDIT_IS_DOCUMENT_LOADER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_DOCUMENT_LOADER))
#define GEDIT_DOCUMENT_LOADER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GEDIT_TYPE_DOCUMENT_LOADER, GeditDocumentLoaderClass))

typedef struct _GeditDocumentLoader        GeditDocumentLoader;
typedef struct _GeditDocumentLoaderClass   GeditDocumentLoaderClass;
typedef struct _GeditDocumentLoaderPrivate GeditDocumentLoaderPrivate;

struct _GeditDocumentLoader
{
	GObject object;

	GeditDocumentLoaderPrivate *priv;
};

struct _GeditDocumentLoaderClass
{
	GObjectClass parent_class;

	/* Signals */
	void (* loading) (GeditDocumentLoader *loader,
			  gboolean             completed,
			  const GError        *error);
};

GType 		 	 gedit_document_loader_get_type		(void) G_GNUC_CONST;

/* If enconding == NULL, the encoding will be autodetected */
GeditDocumentLoader 	*gedit_document_loader_new 		(GeditDocument       *doc,
								 GFile               *location,
								 const GeditEncoding *encoding);

void			 gedit_document_loader_loading		(GeditDocumentLoader *loader,
								 gboolean             completed,
								 GError              *error);

void			 gedit_document_loader_load		(GeditDocumentLoader *loader);

GeditDocumentLoader	*gedit_document_loader_new_from_stream	(GeditDocument       *doc,
								 GInputStream        *stream,
								 const GeditEncoding *encoding);

gboolean		 gedit_document_loader_cancel		(GeditDocumentLoader *loader);

GeditDocument		*gedit_document_loader_get_document	(GeditDocumentLoader *loader);

/* Returns STDIN_URI if loading from stdin */
#define STDIN_URI "stdin:"
GFile			*gedit_document_loader_get_location	(GeditDocumentLoader *loader);

const GeditEncoding	*gedit_document_loader_get_encoding	(GeditDocumentLoader *loader);

GeditDocumentNewlineType gedit_document_loader_get_newline_type (GeditDocumentLoader *loader);

GeditDocumentCompressionType gedit_document_loader_get_compression_type
								(GeditDocumentLoader *loader);

goffset			 gedit_document_loader_get_bytes_read	(GeditDocumentLoader *loader);

/* You can get from the info: content_type, time_modified, standard_size, access_can_write
   and also the metadata*/
GFileInfo		*gedit_document_loader_get_info		(GeditDocumentLoader *loader);

G_END_DECLS

#endif  /* __GEDIT_DOCUMENT_LOADER_H__  */

/* ex:set ts=8 noet: */
