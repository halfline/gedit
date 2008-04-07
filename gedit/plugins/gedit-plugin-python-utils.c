/*
 * gedit-plugin-python-utils.c
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

#include <string.h>
#include "gedit-plugin-info.h"
#include "gedit-plugin-python-utils.h"
#include "gedit-plugins-engine.h"
#include "gedit-python-module.h"

#define TYPE_PREFIX "Gedit"

PyObject *gedit_plugin_python_utils_init(GeditPlugin 			 *plugin,
					 GeditPluginPythonUtilsClasses 	  classreg,
					 const PyMethodDef 		 *functions,
					 GeditPluginPythonUtilsConstants  constreg,
					 const gchar 			 *prefix)
{
	PyObject *plugins, *mdict, *module;
	const gchar *name;
	gchar *mname;
	GeditPluginInfo *info;

	/* Register bindings for the plugin in a separate namespace in
	   gedit.plugins.(modulename) */
	   
	// Make sure python is initialized
	gedit_python_init ();

	plugins = PyImport_ImportModule("gedit.plugins");
	
	if (plugins == NULL)
	{
		PyErr_SetString (PyExc_ImportError,
				 "could not import gedit.plugins");
		return NULL;
	}
	
	mdict = PyModule_GetDict (plugins);
	
	/* Find plugin info, to get module name */
	info = _gedit_plugins_engine_info_for_plugin (gedit_plugins_engine_get_default (),
						      plugin);
	
	if (!info)
	{
		g_warning ("Corresponding GeditPluginInfo could not be found!");
		return NULL;
	}
	
	name = g_type_name (G_OBJECT_TYPE (plugin));
	
	if (g_str_has_prefix (name, TYPE_PREFIX))
		name = name + strlen(TYPE_PREFIX);
	
	mname = g_strdup_printf ("gedit.plugins.%s", name);
	module = Py_InitModule (mname, functions);
	g_free (mname);
	
	mdict = PyModule_GetDict (module);
	
	if (classreg)
		classreg (mdict);
	
	if (constreg)
		constreg (module, prefix);
	
	return module;
}
