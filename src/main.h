/* vi:set ts=4 sts=0 sw=4:
 *
 * gEdit
 * Copyright (C) 1998, 1999 Alex Roberts and Evan Lawrence
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __MAIN_H__
#define __MAIN_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define STRING_LENGTH_MAX	256
#define GEDIT_ID	"gEdit "VERSION

#define UNKNOWN		N_("Unknown")
#define UNTITLED	N_("Untitled")

#define DEFAULT_FONT \
	"-adobe-courier-medium-r-normal-*-*-120-*-*-m-*-iso8859-1"
#define DEFAULT_FONTSET \
	"-*-*-medium-r-normal-*-14-*-*-*-*-*-*-*,*"

typedef struct _gE_window {
	GtkWidget *window;
	GtkWidget *statusbox;
	GtkWidget *statusbar;
	GtkWidget *menubar;
	GtkWidget *toolbar;
	GtkWidget *notebook;
	GtkWidget *open_fileselector;
	GtkWidget *save_fileselector;
	GtkWidget *line_label, *col_label;
	GtkWidget *files_list_window;
	GtkWidget *files_list_window_data;
	GtkWidget *files_list_window_toolbar;

/*	GList *documents;	Pah.. i dunno.. */
	GtkWidget *popup;

	GtkPositionType tab_pos;

} gE_window;

extern gE_window *window;

typedef struct _gE_document {

	GnomeMDIChild mdi_child;
	
	GtkWidget *tab_label;
	gchar *filename;

	guchar *buf;
	guint buf_size;

	struct stat *sb;

} gE_document;

typedef struct _gE_data {
	gE_window *window;
	gE_document *document;
	gpointer temp1;
	gpointer temp2;
	gboolean flag;	/* general purpose flag to indicate if action completed */

} gE_data;

typedef struct _gE_function {
	gchar *name;
	gchar *tooltip_text;
	gchar *icon;
	GtkSignalFunc callback;

} gE_function;

extern GList *window_list;
GList *gE_documents;

extern GnomeMDI *mdi;
extern gint mdiMode;
extern gboolean use_fontset;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __MAIN_H__ */
