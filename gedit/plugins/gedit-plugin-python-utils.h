/*
 * gedit-plugin-python-utils.h
 * This file is part of gedit
 *
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

#ifndef __GEDIT_PLUGIN_PYTHON_UTILS_H__
#define __GEDIT_PLUGIN_PYTHON_UTILS_H__

#include <gedit/plugins/gedit-plugin.h>
#include <pygobject.h>

/* Utility functions which C plugins can use to register python bindings */
typedef void (*GeditPluginPythonUtilsClasses)(PyObject *classes);
typedef void (*GeditPluginPythonUtilsConstants)(PyObject *module, const gchar *strip_prefix);

PyObject *gedit_plugin_python_utils_init (GeditPlugin				*plugin,
					  GeditPluginPythonUtilsClasses		 classreg,
					  PyMethodDef				*plugins,
					  GeditPluginPythonUtilsConstants	 constreg,
					  const gchar				*prefix);
				     
					

#endif /* __GEDIT_PLUGIN_PYTHON_UTILS_H__ */
