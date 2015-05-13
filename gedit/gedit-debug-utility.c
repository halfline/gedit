/*
 * gedit-debug-utility.c
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

#include "gedit-debug-utility.h"
#include <stdio.h>

#define ENABLE_PROFILING

#ifdef ENABLE_PROFILING
static GTimer *timer = NULL;
static gdouble last_time = 0.0;
#endif

struct _GeditDebugUtility
{
	GObject parent;

	guint64 enabled_sections;
	guint64 all_sections;
};

G_DEFINE_TYPE (GeditDebugUtility, _gedit_debug_utility, G_TYPE_OBJECT)

static gboolean
section_is_enabled (GeditDebugUtility *debug,
		    guint64            section)
{
	return (debug->enabled_sections & section) != 0;
}

/* Checks that @section_flag contains only one flag (one bit at 1, all other
 * bits at 0).
 */
static gboolean
is_valid_flag (guint64 section_flag)
{
	guint64 value = section_flag;

	while (value > 1)
	{
		if ((value % 2) == 1)
		{
			return FALSE;
		}

		value >>= 1;
	}

	return value == 1;
}

static void
_gedit_debug_utility_class_init (GeditDebugUtilityClass *klass)
{
}

static void
_gedit_debug_utility_init (GeditDebugUtility *debug)
{
#ifdef ENABLE_PROFILING
	if (timer == NULL)
	{
		timer = g_timer_new ();
	}
#endif
}

GeditDebugUtility *
_gedit_debug_utility_new (void)
{
	return g_object_new (GEDIT_TYPE_DEBUG_UTILITY, NULL);
}

void
_gedit_debug_utility_add_section (GeditDebugUtility *debug,
				  const gchar       *section_name,
				  guint64            section_flag)
{
	guint64 all_sections_prev;

	g_return_if_fail (GEDIT_IS_DEBUG_UTILITY (debug));
	g_return_if_fail (is_valid_flag (section_flag));

	all_sections_prev = debug->all_sections;
	debug->all_sections |= section_flag;

	if (debug->all_sections == all_sections_prev)
	{
		g_warning ("Debug section '%s' uses the same flag value (%" G_GUINT64_FORMAT ") "
			   "as another section.",
			   section_name,
			   section_flag);
		return;
	}

	if (g_getenv (section_name) != NULL)
	{
		debug->enabled_sections |= section_flag;
	}
}

void
_gedit_debug_utility_add_all_section (GeditDebugUtility *debug,
				      const gchar       *all_section_name)
{
	if (g_getenv (all_section_name) != NULL)
	{
		debug->enabled_sections = G_MAXUINT64;
	}
}

/**
 * _gedit_debug_utility_message:
 * @debug: a #GeditDebugUtility instance.
 * @section: debug section.
 * @file: file name.
 * @line: line number.
 * @function: name of the function that is calling gedit_debug_message().
 * @format: A g_vprintf() format string.
 * @...: The format string arguments.
 *
 * If @section is enabled, then logs the trace information @file, @line, and
 * @function along with the message obtained by formatting @format with the
 * given format string arguments.
 */
void
_gedit_debug_utility_message (GeditDebugUtility *debug,
			      guint64            section,
			      const gchar       *file,
			      gint               line,
			      const gchar       *function,
			      const gchar       *format,
			      ...)
{
	va_list args;
	gchar *msg;
#ifdef ENABLE_PROFILING
	gdouble cur_time;
#endif

	g_return_if_fail (GEDIT_IS_DEBUG_UTILITY (debug));
	g_return_if_fail (format != NULL);

	if (G_LIKELY (!section_is_enabled (debug, section)))
	{
		return;
	}

	va_start (args, format);
	msg = g_strdup_vprintf (format, args);
	va_end (args);

#ifdef ENABLE_PROFILING
	cur_time = g_timer_elapsed (timer, NULL);

	g_print ("[%f (%f)] ",
		 cur_time,
		 cur_time - last_time);

	last_time = cur_time;
#endif

	g_print ("%s:%d (%s)%s%s\n",
		 file,
		 line,
		 function,
		 msg[0] != '\0' ? " " : "", /* avoid trailing space */
		 msg);

	fflush (stdout);
	g_free (msg);
}

/* ex:set ts=8 noet: */
