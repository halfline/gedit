/*
 * This file is part of gcode.
 *
 * Copyright 1998, 1999 - Alex Roberts, Evan Lawrence
 * Copyright 2000, 2001 - Chema Celorio
 * Copyright 2000-2005 - Paolo Maggi
 * Copyright 2015 - Sébastien Wilmet <swilmet@gnome.org>
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

#include "gcode-debug.h"
#include <stdio.h>

#define ENABLE_PROFILING

#ifdef ENABLE_PROFILING
static GTimer *timer = NULL;
static gdouble last_time = 0.0;
#endif

static GcodeDebugSection enabled_sections = GCODE_NO_DEBUG;

#define DEBUG_IS_ENABLED(section) (enabled_sections & (section))

/**
 * gcode_debug_init:
 *
 * Initializes the debugging subsystem of gcode.
 *
 * The function checks for the existence of certain environment variables to
 * determine whether to enable output for a debug section. To enable output
 * for a specific debug section, set an environment variable of the same name;
 * e.g. to enable output for the %GCODE_DEBUG_PLUGINS section, set a
 * <code>GCODE_DEBUG_PLUGINS</code> environment variable. To enable output
 * for all debug sections, set the <code>GCODE_DEBUG</code> environment
 * variable.
 *
 * This function must be called before any of the other debug functions are
 * called. It must only be called once.
 */
void
gcode_debug_init (void)
{
	if (g_getenv ("GCODE_DEBUG") != NULL)
	{
		/* enable all debugging */
		enabled_sections = ~GCODE_NO_DEBUG;
		goto out;
	}

	if (g_getenv ("GCODE_DEBUG_UTILS") != NULL)
	{
		enabled_sections |= GCODE_DEBUG_UTILS;
	}

out:

#ifdef ENABLE_PROFILING
	if (enabled_sections != GCODE_NO_DEBUG)
	{
		timer = g_timer_new ();
	}
#endif
}

/**
 * gcode_debug:
 * @section: debug section.
 * @file: file name.
 * @line: line number.
 * @function: name of the function that is calling gcode_debug().
 *
 * If @section is enabled, then logs the trace information @file, @line, and
 * @function.
 */
void
gcode_debug (GcodeDebugSection  section,
	     const gchar       *file,
	     gint               line,
	     const gchar       *function)
{
	gcode_debug_message (section, file, line, function, "%s", "");
}

/**
 * gcode_debug_message:
 * @section: debug section.
 * @file: file name.
 * @line: line number.
 * @function: name of the function that is calling gcode_debug_message().
 * @format: A g_vprintf() format string.
 * @...: The format string arguments.
 *
 * If @section is enabled, then logs the trace information @file, @line, and
 * @function along with the message obtained by formatting @format with the
 * given format string arguments.
 */
void
gcode_debug_message (GcodeDebugSection  section,
		     const gchar       *file,
		     gint               line,
		     const gchar       *function,
		     const gchar       *format,
		     ...)
{
	if (G_UNLIKELY (DEBUG_IS_ENABLED (section)))
	{
		va_list args;
		gchar *msg;

#ifdef ENABLE_PROFILING
		gdouble seconds;

		g_return_if_fail (timer != NULL);

		seconds = g_timer_elapsed (timer, NULL);
#endif

		g_return_if_fail (format != NULL);

		va_start (args, format);
		msg = g_strdup_vprintf (format, args);
		va_end (args);

#ifdef ENABLE_PROFILING
		g_print ("[%f (%f)] %s:%d (%s) %s\n",
			 seconds,
			 seconds - last_time,
			 file,
			 line,
			 function,
			 msg);

		last_time = seconds;
#else
		g_print ("%s:%d (%s) %s\n", file, line, function, msg);
#endif

		fflush (stdout);

		g_free (msg);
	}
}

/* ex:set ts=8 noet: */
