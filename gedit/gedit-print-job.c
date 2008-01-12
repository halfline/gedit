/*
 * gedit-print.c
 * This file is part of gedit
 *
 * Copyright (C) 2000-2001 Chema Celorio, Paolo Maggi
 * Copyright (C) 2002-2008 Paolo Maggi  
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
 * $Id: gedit-print.c 6022 2007-12-09 14:38:57Z pborelli $
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include <sys/types.h>#define GEDIT_TAB_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), GEDIT_TYPE_TAB, GeditTabPrivate))
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>	/* For strlen */

#include "gedit-print-job.h"
#include "gedit-debug.h"
#include "gedit-utils.h"
#include "gedit-prefs-manager-app.h"
#include "gedit-tab.h"


#define GEDIT_PRINT_JOB_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), GEDIT_TYPE_PRINT_JOB, GeditPrintJobPrivate))

struct _GeditPrintJobPrivate
{
	GtkPrintOperation *operation;	
};

enum 
{
	PRINTING,
	DONE
	LAST_SIGNAL
};

static guint print_job_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GeditPrintJob, gedit_print_job, G_TYPE_OBJECT)


static void 
gedit_print_job_class_init (GeditPrintJobClass *klass)
{
	print_job_signals[PRINTING] =
		g_signal_new ("printing",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GeditPrintJobClass, printing),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_UINT);

	print_job_signals[DONE] =
		g_signal_new ("done",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GeditPrintJobClass, done),
			      NULL, NULL,
			      gedit_marshal_VOID__UINT_POINTER,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_UINT,
			      G_TYPE_POINTER);
			      			      
	g_type_class_add_private (object_class, sizeof (GeditPrintJobPrivate));
}

// Print setting and page setup vanno settati per document e fall back al file salvato
// Vedi buffer_set
#define GEDIT_PRINT_CONFIG "gedit-print-config-key"

#if 0
static void
buffer_set (GeditPrintJob *job, GParamSpec *pspec, gpointer d)
{
	GtkSourceBuffer *buffer;
	GtkSourcePrintJob *pjob;
	gpointer data;
	GnomePrintConfig *config;
	
	gedit_debug (DEBUG_PRINT);
	
	pjob = GTK_SOURCE_PRINT_JOB (job);
	
	buffer = gtk_source_print_job_get_buffer (pjob);
	
	data = g_object_get_data (G_OBJECT (buffer),
				  GEDIT_PRINT_CONFIG);

	if (data == NULL)	
	{			  
		config = load_print_config_from_file ();
		g_return_if_fail (config != NULL);
		
		g_object_set_data_full (G_OBJECT (buffer),
					GEDIT_PRINT_CONFIG,
					config,
					(GDestroyNotify)gnome_print_config_unref);
	}
	else
	{
		config = GNOME_PRINT_CONFIG (data);
	}

	gnome_print_config_set_int (config, (guchar *) GNOME_PRINT_KEY_NUM_COPIES, 1);
	gnome_print_config_set_boolean (config, (guchar *) GNOME_PRINT_KEY_COLLATE, FALSE);

	gtk_source_print_job_set_config (pjob, config);
	
	gtk_source_print_job_set_highlight (pjob, 
					    gtk_source_buffer_get_highlight_syntax (buffer) &&
					    gedit_prefs_manager_get_print_syntax_hl ());
		
	if (gedit_prefs_manager_get_print_header ())
	{
		gchar *doc_name;
		gchar *name_to_display;
		gchar *left;

		doc_name = gedit_document_get_uri_for_display (GEDIT_DOCUMENT (buffer));
		name_to_display = gedit_utils_str_middle_truncate (doc_name, 60);

		left = g_strdup_printf (_("File: %s"), name_to_display);

		/* Translators: %N is the current page number, %Q is the total
		 * number of pages (ex. Page 2 of 10) 
		 */
		gtk_source_print_job_set_header_format (pjob,
							left, 
							NULL, 
							_("Page %N of %Q"), 
							TRUE);

		gtk_source_print_job_set_print_header (pjob, TRUE);

		g_free (doc_name);
		g_free (name_to_display);
		g_free (left);
	}	
}
#endif

static void
gedit_print_job_init (GeditPrintJob *job)
{

	job->priv = GEDIT_PRINT_JOB_GET_PRIVATE (job);
#if 0
	GtkSourcePrintJob *pjob;

	gchar *print_font_body;
	gchar *print_font_header;
	gchar *print_font_numbers;
	
	PangoFontDescription *print_font_body_desc;
	PangoFontDescription *print_font_header_desc;
	PangoFontDescription *print_font_numbers_desc;
	
	gedit_debug (DEBUG_PRINT);
	
	pjob = GTK_SOURCE_PRINT_JOB (job);
		
	gtk_source_print_job_set_print_numbers (pjob,
			gedit_prefs_manager_get_print_line_numbers ());

	gtk_source_print_job_set_wrap_mode (pjob,
			gedit_prefs_manager_get_print_wrap_mode ());

	gtk_source_print_job_set_tabs_width (pjob,
			gedit_prefs_manager_get_tabs_size ());
	
	gtk_source_print_job_set_print_header (pjob, FALSE);
	gtk_source_print_job_set_print_footer (pjob, FALSE);

	print_font_body = gedit_prefs_manager_get_print_font_body ();
	print_font_header = gedit_prefs_manager_get_print_font_header ();
	print_font_numbers = gedit_prefs_manager_get_print_font_numbers ();
	
	gtk_source_print_job_set_font (pjob, print_font_body);
	gtk_source_print_job_set_numbers_font (pjob, print_font_numbers);
	gtk_source_print_job_set_header_footer_font (pjob, print_font_header);

	print_font_body_desc = pango_font_description_from_string (print_font_body);
	print_font_header_desc = pango_font_description_from_string (print_font_header);
	print_font_numbers_desc = pango_font_description_from_string (print_font_numbers);

	gtk_source_print_job_set_font_desc (pjob, print_font_body_desc);
	gtk_source_print_job_set_numbers_font_desc (pjob, print_font_numbers_desc);
	gtk_source_print_job_set_header_footer_font_desc (pjob, print_font_header_desc);

	g_free (print_font_body);
	g_free (print_font_header);
	g_free (print_font_numbers);

	pango_font_description_free (print_font_body_desc);
	pango_font_description_free (print_font_header_desc);
	pango_font_description_free (print_font_numbers_desc);
	
	g_signal_connect (job,
			  "notify::buffer",
			  G_CALLBACK (buffer_set),
			  NULL);
#endif
}

GeditPrintJob *
gedit_print_job_new (GeditDocument *doc)
{
	GeditPrintJob *job;
	
	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), NULL);
	
	job = GEDIT_PRINT_JOB (g_object_new (GEDIT_TYPE_PRINT_JOB,
					     "buffer", doc,
					      NULL));

	return job;
}

