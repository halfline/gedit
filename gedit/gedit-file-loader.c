/*
 * gedit-file-loader.c
 * This file is part of gedit
 *
 * Copyright (C) 2015 - Sébastien Wilmet <swilmet@gnome.org>
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

#include "gedit-file-loader.h"
#include "gedit-progress-info-bar.h"
#include "gedit-utils.h"

/* TODO do not depend on GeditTab. */
#include "gedit-tab.h"

/* TODO do not depend on GeditDocument. */
#include "gedit-document.h"

/* When you modify this class, keep in mind that it must remain re-usable for
 * other text editors.
 * This class wraps GtkSourceFileLoader to add an info bar that shows the
 * progress of the file loading, and handles errors by asking some questions to
 * stop or relaunch the file loading with different options.
 */

struct _GeditFileLoaderPrivate
{
	/* GtkInfoBars can be added/removed shown/hidden in @info_bar. */
	GtkGrid *info_bar;
	GeditProgressInfoBar *progress_info_bar;
	GtkInfoBar *error_info_bar;

	/* FIXME is the state useful? If yes, create a new enum to not depend on
	 * GeditTab.
	 */
	GeditTabState state;
};

typedef struct _TaskData TaskData;
struct _TaskData
{
	GTimer *timer;
};

G_DEFINE_TYPE_WITH_PRIVATE (GeditFileLoader, _gedit_file_loader, G_TYPE_OBJECT)

/* Prototypes */
static void launch_loading (GTask *task);

GeditDocument *
get_document (GeditFileLoader *loader)
{
	return GEDIT_DOCUMENT (gtk_source_file_loader_get_buffer (GTK_SOURCE_FILE_LOADER (loader)));
}

static TaskData *
task_data_new (void)
{
	return g_slice_new0 (TaskData);
}

static void
task_data_free (TaskData *data)
{
	if (data != NULL)
	{
		if (data->timer != NULL)
		{
			g_timer_destroy (data->timer);
		}

		g_slice_free (TaskData, data);
	}
}

static void
_gedit_file_loader_dispose (GObject *object)
{
	GeditFileLoader *loader = GEDIT_FILE_LOADER (object);

	if (loader->priv->progress_info_bar != NULL)
	{
		gtk_widget_destroy (GTK_WIDGET (loader->priv->progress_info_bar));
		loader->priv->progress_info_bar = NULL;
	}

	if (loader->priv->error_info_bar != NULL)
	{
		gtk_widget_destroy (GTK_WIDGET (loader->priv->error_info_bar));
		loader->priv->error_info_bar = NULL;
	}

	G_OBJECT_CLASS (_gedit_file_loader_parent_class)->dispose (object);
}

static void
_gedit_file_loader_class_init (GeditFileLoaderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = _gedit_file_loader_dispose;
}

static void
_gedit_file_loader_init (GeditFileLoader *loader)
{
	loader->priv = _gedit_file_loader_get_instance_private (loader);

	loader->priv->info_bar = GTK_GRID (gtk_grid_new ());
	gtk_orientable_set_orientation (GTK_ORIENTABLE (loader->priv->info_bar),
					GTK_ORIENTATION_VERTICAL);
	gtk_widget_show (GTK_WIDGET (loader->priv->info_bar);
}

GeditFileLoader *
_gedit_file_loader_new (void)
{
	return GEDIT_FILE_LOADER (g_object_new (GEDIT_TYPE_FILE_LOADER, NULL));
}

GtkWidget *
_gedit_file_loader_get_info_bar (GeditFileLoader *loader)
{
	g_return_val_if_fail (GEDIT_IS_FILE_LOADER (loader), NULL);

	return GTK_WIDGET (loader->priv->info_bar);
}

static void
cancelled_cb (GtkWidget *info_bar,
	      gint       response_id,
	      GTask     *task)
{
	g_cancellable_cancel (g_task_get_cancellable (task));
	/* FIXME finish task? */
}

#define MAX_MSG_LENGTH 100

static void
show_progress_info_bar (GTask *task)
{
	GeditFileLoader *loader = g_task_get_source_object (task);
	GtkWidget *info_bar;
	GeditDocument *doc;
	gchar *name;
	gchar *dirname = NULL;
	gchar *msg = NULL;
	gchar *name_markup;
	gchar *dirname_markup;
	gint len;

	if (loader->priv->progress_info_bar != NULL)
	{
		return;
	}

	doc = get_document (loader);

	name = gedit_document_get_short_name_for_display (doc);
	len = g_utf8_strlen (name, -1);

	/* If the name is awfully long, truncate it and be done with it,
	 * otherwise also show the directory (ellipsized if needed).
	 */
	if (len > MAX_MSG_LENGTH)
	{
		gchar *str;

		str = gedit_utils_str_middle_truncate (name, MAX_MSG_LENGTH);
		g_free (name);
		name = str;
	}
	else
	{
		GFile *location = gtk_source_file_loader_get_location (GTK_SOURCE_FILE_LOADER (loader));

		if (location != NULL)
		{
			gchar *str = gedit_utils_location_get_dirname_for_display (location);

			/* Use the remaining space for the dir, but use a min of 20 chars
			 * so that we do not end up with a dirname like "(a...b)".
			 * This means that in the worst case when the filename is long 99
			 * we have a title long 99 + 20, but I think it's a rare enough
			 * case to be acceptable. It's justa darn title afterall :)
			 */
			dirname = gedit_utils_str_middle_truncate (str,
								   MAX (20, MAX_MSG_LENGTH - len));
			g_free (str);
		}
	}

	name_markup = g_markup_printf_escaped ("<b>%s</b>", name);

	if (loader->priv->state == GEDIT_TAB_STATE_REVERTING)
	{
		if (dirname != NULL)
		{
			dirname_markup = g_markup_printf_escaped ("<b>%s</b>", dirname);

			/* Translators: the first %s is a file name (e.g. test.txt) the second one
			 * is a directory (e.g. ssh://master.gnome.org/home/users/paolo).
			 */
			msg = g_strdup_printf (_("Reverting %s from %s"),
					       name_markup,
					       dirname_markup);
			g_free (dirname_markup);
		}
		else
		{
			msg = g_strdup_printf (_("Reverting %s"), name_markup);
		}

		info_bar = gedit_progress_info_bar_new ("document-revert", msg, TRUE);
	}
	else
	{
		if (dirname != NULL)
		{
			dirname_markup = g_markup_printf_escaped ("<b>%s</b>", dirname);

			/* Translators: the first %s is a file name (e.g. test.txt) the second one
			 * is a directory (e.g. ssh://master.gnome.org/home/users/paolo).
			 */
			msg = g_strdup_printf (_("Loading %s from %s"),
					       name_markup,
					       dirname_markup);
			g_free (dirname_markup);
		}
		else
		{
			msg = g_strdup_printf (_("Loading %s"), name_markup);
		}

		info_bar = gedit_progress_info_bar_new ("document-open", msg, TRUE);
	}

	loader->priv->progress_info_bar = GEDIT_PROGRESS_INFO_BAR (info_bar);

	gtk_container_add (GTK_CONTAINER (loader->priv->info_bar), info_bar);
	gtk_widget_show (info_bar);

	g_signal_connect_object (info_bar,
				 "response",
				 G_CALLBACK (cancelled_cb),
				 task,
				 0);

	g_free (msg);
	g_free (name);
	g_free (name_markup);
	g_free (dirname);
}

static void
set_progress (GeditFileLoader *loader,
	      goffset          size,
	      goffset          total_size)
{
	if (loader->priv->progress_info_bar == NULL)
	{
		return;
	}

	if (total_size != 0)
	{
		gdouble frac;

		frac = (gdouble)size / (gdouble)total_size;

		gedit_progress_info_bar_set_fraction (loader->priv->progress_info_bar, frac);
	}
	else if (size != 0)
	{
		gedit_progress_info_bar_pulse (loader->priv->progress_info_bar);
	}
	else
	{
		gedit_progress_info_bar_set_fraction (loader->priv->progress_info_bar, 0);
	}
}

static void
progress_cb (goffset  size,
	     goffset  total_size,
	     GTask   *task)
{
	GeditFileLoader *loader = g_task_get_source_object (task);
	TaskData *data = g_task_get_task_data (task);
	gdouble elapsed_time;
	gdouble total_time;
	gdouble remaining_time;

	/* FIXME create timer before beginning the loading? Or is this callback
	 * called with size == 0?
	 */
	if (data->priv->timer == NULL)
	{
		data->priv->timer = g_timer_new ();
		return;
	}

	elapsed_time = g_timer_elapsed (data->priv->timer, NULL);

	/* elapsed_time / total_time = size / total_size */
	total_time = (elapsed_time * total_size) / size;

	remaining_time = total_time - elapsed_time;

	/* Approximately more than 3 seconds remaining. */
	if (remaining_time > 3.0)
	{
		show_progress_info_bar (task);
	}

	set_progress (loader, size, total_size);
}

static void
set_info_bar (GeditFileLoader *loader,
	      GtkInfoBar      *info_bar,
	      GtkResponseType  default_response)
{
	if (loader->priv->error_info_bar == info_bar)
	{
		return;
	}

	if (loader->priv->error_info_bar != NULL)
	{
		gtk_widget_destroy (GTK_WIDGET (loader->priv->error_info_bar));
	}

	loader->priv->error_info_bar = info_bar;

	if (info_bar != NULL)
	{
		gtk_container_add (GTK_CONTAINER (loader->priv->info_bar),
				   GTK_WIDGET (info_bar));

		if (default_response != GTK_RESPONSE_NONE)
		{
			gtk_info_bar_set_default_response (info_bar, default_response);
		}

		gtk_widget_show (GTK_WIDGET (info_bar));
	}
}

static void
io_loading_error_info_bar_response (GtkWidget *info_bar,
				    gint       response_id,
				    GTask     *task)
{
	GeditFileLoader *loader = g_task_get_source_object (task);

	switch (response_id)
	{
		case GTK_RESPONSE_OK:
		{
			const GtkSourceEncoding *encoding;
			GSList *candidate_encodings;

			encoding = gedit_conversion_error_info_bar_get_encoding (info_bar);
			candidate_encodings = g_slist_prepend (NULL, (gpointer) encoding);

			gtk_source_file_loader_set_candidate_encodings (GTK_SOURCE_FILE_LOADER (loader),
									candidate_encodings);
			g_slist_free (candidate_encodings);

			set_info_bar (loader, NULL, GTK_RESPONSE_NONE);
			loader->priv->state = GEDIT_TAB_STATE_LOADING;

			launch_loading (task);
			break;
		}

		case GTK_RESPONSE_YES:
			/* This means that we want to edit the document anyway */
			/* TODO set the textview as editable. */
			set_info_bar (loader, NULL, GTK_RESPONSE_NONE);
			g_task_return_boolean (task, TRUE);
			g_object_unref (task);
			break;

		default:
			g_task_return_boolean (task, FALSE);
			g_object_unref (task);
			break;
	}
}

static void
load_cb (GtkSourceFileLoader *source_loader,
	 GAsyncResult        *result,
	 GTask               *task)
{
	GeditFileLoader *gedit_loader;
	GFile *location;
	TaskData *data;
	GError *error = NULL;

	gedit_loader = GEDIT_FILE_LOADER (source_loader);
	location = gtk_source_file_loader_get_location (source_loader);
	data = g_task_get_task_data (task);

	gtk_source_file_loader_load_finish (source_loader, result, &error);

	if (data->priv->timer != NULL)
	{
		g_timer_destroy (data->priv->timer);
		data->priv->timer = NULL;
	}

	if (loader->priv->progress_info_bar != NULL)
	{
		gtk_widget_hide (GTK_WIDGET (loader->priv->progress_info_bar));
	}

	/* Load was successful. */
	if (error == NULL)
	{
		g_task_return_boolean (task, TRUE);
		g_object_unref (task);
		return;
	}

	if (error->domain == GTK_SOURCE_FILE_LOADER_ERROR &&
	    error->code == GTK_SOURCE_FILE_LOADER_ERROR_CONVERSION_FALLBACK)
	{
		GtkWidget *info_bar;
		const GtkSourceEncoding *encoding;

		/* TODO Set the textview as not editable as we have an error,
		 * the user can decide to make it editable again.
		 */

		encoding = gtk_source_file_loader_get_encoding (source_loader);

		info_bar = gedit_io_loading_error_info_bar_new (location, encoding, error);

		g_signal_connect (info_bar,
				  "response",
				  G_CALLBACK (io_loading_error_info_bar_response),
				  task);

		set_info_bar (gedit_loader,
			      GTK_INFO_BAR (info_bar),
			      GTK_RESPONSE_CANCEL);
	}

	if (error != NULL)
	{
		if (loader->priv->state == GEDIT_TAB_STATE_LOADING)
		{
			loader->priv->state = GEDIT_TAB_STATE_LOADING_ERROR;
		}
		else
		{
			loader->priv->state = GEDIT_TAB_STATE_REVERTING_ERROR;
		}

		if (error->domain == G_IO_ERROR &&
		    error->code == G_IO_ERROR_CANCELLED)
		{
			g_task_return_boolean (task, FALSE);
			g_object_unref (task);
			g_error_free (error);
			return;
		}
		else
		{
			GtkWidget *info_bar;

			if (location != NULL)
			{
				gedit_recent_remove_if_local (location);
			}

			if (tab->priv->state == GEDIT_TAB_STATE_LOADING_ERROR)
			{
				const GtkSourceEncoding *encoding;

				encoding = gtk_source_file_loader_get_encoding (loader);

				info_bar = gedit_io_loading_error_info_bar_new (location, encoding, error);

				g_signal_connect (info_bar,
						  "response",
						  G_CALLBACK (io_loading_error_info_bar_response),
						  tab);
			}
			else
			{
				g_return_if_fail (tab->priv->state == GEDIT_TAB_STATE_REVERTING_ERROR);

				info_bar = gedit_unrecoverable_reverting_error_info_bar_new (location, error);

				g_signal_connect (info_bar,
						  "response",
						  G_CALLBACK (unrecoverable_reverting_error_info_bar_response),
						  tab);
			}

			set_info_bar (tab, info_bar, GTK_RESPONSE_CANCEL);
		}

		goto end;
	}

	if (error != NULL &&
	    error->domain == GTK_SOURCE_FILE_LOADER_ERROR &&
	    error->code == GTK_SOURCE_FILE_LOADER_ERROR_CONVERSION_FALLBACK)
	{
	}

	loader->priv->state = GEDIT_TAB_STATE_NORMAL;

	if (error != NULL)
	{
		g_error_free (error);
	}

	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
}

static void
launch_loading (GTask *task)
{
	GtkSourceFileLoader *loader = g_task_get_source_object (task);

	gtk_source_file_loader_load_async (loader,
					   g_task_get_priority (task),
					   g_task_get_cancellable (task),
					   (GFileProgressCallback) progress_cb,
					   task,
					   NULL,
					   (GAsyncReadyCallback) load_cb,
					   task);
}

void
_gedit_file_loader_load_async (GeditFileLoader     *loader,
			       gint                 io_priority,
			       GCancellable        *cancellable,
			       GAsyncReadyCallback  callback,
			       gpointer             user_data)
{
	GTask *task;
	TaskData *data;

	task = g_task_new (loader, cancellable, callback, user_data);

	data = task_data_new ();
	g_task_set_task_data (task, data, (GDestroyNotify) task_data_free);

	g_task_set_priority (task, io_priority);

	launch_loading (task);
}

gboolean
_gedit_file_loader_load_finish (GeditFileLoader  *loader,
				GAsyncResult     *result,
				GError          **error)
{
	g_return_if_fail (g_task_is_valid (result, loader));

	return g_task_propagate_boolean (G_TASK (result), error);
}

/* ex:set ts=8 noet: */
