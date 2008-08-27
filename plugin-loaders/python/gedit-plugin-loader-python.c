#include "gedit-plugin-loader-python.h"
#include "gedit-plugin-python.h"
#include <gedit/gedit-object-module.h>

#include <Python.h>
#include <pygobject.h>
#include <pygtk/pygtk.h>
#include <signal.h>
#include "config.h"

#if PY_VERSION_HEX < 0x02050000
typedef int Py_ssize_t;
#define PY_SSIZE_T_MAX INT_MAX
#define PY_SSIZE_T_MIN INT_MIN
#endif

#define GEDIT_PLUGIN_LOADER_PYTHON_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GEDIT_TYPE_PLUGIN_LOADER_PYTHON, GeditPluginLoaderPythonPrivate))

struct _GeditPluginLoaderPythonPrivate
{
	GHashTable *loaded_plugins;
	guint idle_gc;
	gboolean init_failed;
};

typedef struct
{
	PyObject *type;
	gchar    *path;
} PythonInfo;

static void gedit_plugin_loader_iface_init (gpointer g_iface, gpointer iface_data);

/* Exported by pygedit module */
void pygedit_register_classes (PyObject *d);
void pygedit_add_constants (PyObject *module, const gchar *strip_prefix);
extern PyMethodDef pygedit_functions[];

/* Exported by pygeditutils module */
void pygeditutils_register_classes (PyObject *d);
extern PyMethodDef pygeditutils_functions[];

/* Exported by pygeditcommands module */
void pygeditcommands_register_classes (PyObject *d);
extern PyMethodDef pygeditcommands_functions[];

/* We retreive this to check for correct class hierarchy */
static PyTypeObject *PyGeditPlugin_Type;

GEDIT_PLUGIN_LOADER_REGISTER_TYPE (GeditPluginLoaderPython, gedit_plugin_loader_python);

static void
destroy_pyobject (gpointer           data, 
                  GeditPluginPython *plugin, 
                  gboolean           is_last_ref)
{
        if (is_last_ref)
                _gedit_plugin_python_destroy (plugin);
}

static GeditPlugin *
new_plugin_from_info (GeditPluginLoaderPython *loader,
		      GeditPluginInfo         *info)
{
	PythonInfo *pyinfo;
	GObject *object;
	
	pyinfo = (PythonInfo *)g_hash_table_lookup (loader->priv->loaded_plugins, info);
	
	if (pyinfo == NULL)
		return NULL;
	
	PyObject *instance = PyObject_CallObject (pyinfo->type, NULL);
	
	/* CHECKME: not sure if we want to decrease the reference here, but
           should be OK since GeditPythonPlugin adds a reference */
        Py_XDECREF (instance);
        
        // FIXME: what does this do, and why?
        object = pygobject_get (instance);
        g_object_add_toggle_ref (object, (GToggleNotify)destroy_pyobject, NULL);

	return GEDIT_PLUGIN (pygobject_get (instance));
}

static GeditPlugin *
add_python_info (GeditPluginLoaderPython *loader,
		 GeditPluginInfo         *info,
		 const gchar             *path,
		 PyObject                *type)
{
	PythonInfo *pyinfo;
	
	pyinfo = g_new (PythonInfo, 1);
	pyinfo->path = g_strdup (path);
	pyinfo->type = type;
	
	Py_INCREF (pyinfo->type);
	
	g_hash_table_insert (loader->priv->loaded_plugins, info, pyinfo);
	
	return new_plugin_from_info (loader, info);
}

static GeditPlugin *
gedit_plugin_loader_iface_load (GeditPluginLoader *loader,
				GeditPluginInfo   *info,
				const gchar       *path)
{
	GeditPluginLoaderPython *pyloader = GEDIT_PLUGIN_LOADER_PYTHON (loader);
	PyObject *main_module, *main_locals, *locals, *key, *value;
	PyObject *pymodule, *fromlist;
	gchar *module_name;
	Py_ssize_t pos = 0;
	GeditPlugin *result;
	
	if (pyloader->priv->init_failed)
	{
		g_warning ("Cannot load python plugin Python '%s' since gedit was"
		           "not able to initialize the Python interpreter.",
		           gedit_plugin_info_get_name (info));
		return NULL;
	}
	
	/* see if py definition for the plugin is already loaded */
	result = new_plugin_from_info (pyloader, info);
	
	if (result != NULL)
		return result;
	
	main_module = PyImport_AddModule ("__main__");
	if (main_module == NULL)
	{
		g_warning ("Could not get __main__.");
		return NULL;
	}
	
	/* If we have a special path, we register it */
	if (path != NULL)
	{
		PyObject *sys_path = PySys_GetObject ("path");
		PyObject *pypath = PyString_FromString (path);

		if (PySequence_Contains (sys_path, pypath) == 0)
			PyList_Insert (sys_path, 0, pypath);

		Py_DECREF (pypath);
	}
	
	main_locals = PyModule_GetDict (main_module);
	
	/* we need a fromlist to be able to import modules with a '.' in the
	   name. */
	fromlist = PyTuple_New(0);
	module_name = g_strdup (gedit_plugin_info_get_module_name (info));
	pymodule = PyImport_ImportModuleEx (module_name, 
					    main_locals, 
					    main_locals, 
					    fromlist);
	g_free (module_name);
	Py_DECREF(fromlist);

	if (!pymodule)
	{
		PyErr_Print ();
		return NULL;
	}

	locals = PyModule_GetDict (pymodule);
	
	while (PyDict_Next (locals, &pos, &key, &value))
	{
		if (!PyType_Check(value))
			continue;

		if (PyObject_IsSubclass (value, (PyObject*) PyGeditPlugin_Type))
			return add_python_info (pyloader, info, path, value);
	}
	
	g_warning ("No GeditPlugin derivative found in Python plugin '%s'",
		   gedit_plugin_info_get_name (info));

	return NULL;
}

static void
gedit_plugin_loader_iface_unload (GeditPluginLoader *loader,
				  GeditPluginInfo   *info)
{
	//GeditPluginLoaderPython *pyloader = GEDIT_PLUGIN_LOADER_PYTHON (loader);
	
	/* this is a no-op, since the type module will be properly unused as
	   the last reference to the plugin dies. When the plugin is activated
	   again, the library will be reloaded */
}

static gboolean
run_gc (GeditPluginLoaderPython *loader)
{
	while (PyGC_Collect ())
		;

	loader->priv->idle_gc = 0;
	return FALSE;
}

static void
gedit_plugin_loader_iface_garbage_collect (GeditPluginLoader *loader)
{
	GeditPluginLoaderPython *pyloader;
	
	if (!Py_IsInitialized())
		return;

	pyloader = GEDIT_PLUGIN_LOADER_PYTHON (loader);

	/*
	 * We both run the GC right now and we schedule
	 * a further collection in the main loop.
	 */

	while (PyGC_Collect ())
		;

	if (pyloader->priv->idle_gc == 0)
		pyloader->priv->idle_gc = g_idle_add ((GSourceFunc)run_gc, pyloader);
}

static void
gedit_plugin_loader_iface_init (gpointer g_iface, 
				gpointer iface_data)
{
	GeditPluginLoaderInterface *iface = (GeditPluginLoaderInterface *)g_iface;
	
	iface->load = gedit_plugin_loader_iface_load;
	iface->unload = gedit_plugin_loader_iface_unload;
	iface->garbage_collect = gedit_plugin_loader_iface_garbage_collect;
}

static void
gedit_python_shutdown (GeditPluginLoaderPython *loader)
{
	if (!Py_IsInitialized ())
		return;

	if (loader->priv->idle_gc != 0)
	{
		g_source_remove (loader->priv->idle_gc);
		loader->priv->idle_gc = 0;
	}

	while (PyGC_Collect ())
		;	

	Py_Finalize ();
}


/* C equivalent of
 *    import pygtk
 *    pygtk.require ("2.0")
 */
static gboolean
gedit_check_pygtk2 (void)
{
	PyObject *pygtk, *mdict, *require;

	/* pygtk.require("2.0") */
	pygtk = PyImport_ImportModule ("pygtk");
	if (pygtk == NULL)
	{
		g_warning ("Error initializing Python interpreter: could not import pygtk.");
		return FALSE;
	}

	mdict = PyModule_GetDict (pygtk);
	require = PyDict_GetItemString (mdict, "require");
	PyObject_CallObject (require,
			     Py_BuildValue ("(S)", PyString_FromString ("2.0")));
	if (PyErr_Occurred())
	{
		g_warning ("Error initializing Python interpreter: pygtk 2 is required.");
		return FALSE;
	}

	return TRUE;
}

/* Note: the following two functions are needed because
 * init_pyobject and init_pygtk which are *macros* which in case
 * case of error set the PyErr and then make the calling
 * function return behind our back.
 * It's up to the caller to check the result with PyErr_Occurred()
 */
static void
gedit_init_pygobject (void)
{
	init_pygobject_check (2, 11, 5); /* FIXME: get from config */
}

static void
gedit_init_pygtk (void)
{
	PyObject *gtk, *mdict, *version, *required_version;

	init_pygtk ();

	/* there isn't init_pygtk_check(), do the version
	 * check ourselves */
	gtk = PyImport_ImportModule("gtk");
	mdict = PyModule_GetDict(gtk);
	version = PyDict_GetItemString (mdict, "pygtk_version");
	if (!version)
	{
		PyErr_SetString (PyExc_ImportError,
				 "PyGObject version too old");
		return;
	}

	required_version = Py_BuildValue ("(iii)", 2, 4, 0); /* FIXME */

	if (PyObject_Compare (version, required_version) == -1)
	{
		PyErr_SetString (PyExc_ImportError,
				 "PyGObject version too old");
		Py_DECREF (required_version);
		return;
	}

	Py_DECREF (required_version);
}

static void
old_gtksourceview_init (void)
{
	PyErr_SetString(PyExc_ImportError,
			"gtksourceview module not allowed, use gtksourceview2");
}

static void
gedit_init_pygtksourceview (void)
{
	PyObject *gtksourceview, *mdict, *version, *required_version;

	gtksourceview = PyImport_ImportModule("gtksourceview2");
	if (gtksourceview == NULL)
	{
		PyErr_SetString (PyExc_ImportError,
				 "could not import gtksourceview");
		return;
	}

	mdict = PyModule_GetDict (gtksourceview);
	version = PyDict_GetItemString (mdict, "pygtksourceview2_version");
	if (!version)
	{
		PyErr_SetString (PyExc_ImportError,
				 "PyGtkSourceView version too old");
		return;
	}

	required_version = Py_BuildValue ("(iii)", 0, 8, 0); /* FIXME */

	if (PyObject_Compare (version, required_version) == -1)
	{
		PyErr_SetString (PyExc_ImportError,
				 "PyGtkSourceView version too old");
		Py_DECREF (required_version);
		return;
	}

	Py_DECREF (required_version);

	/* Create a dummy 'gtksourceview' module to prevent
	 * loading of the old 'gtksourceview' modules that
	 * has conflicting symbols with the gtksourceview2 module.
	 * Raise an exception when trying to import it.
	 */
	PyImport_AppendInittab ("gtksourceview", old_gtksourceview_init);
}

static gboolean
gedit_python_init (GeditPluginLoaderPython *loader)
{
	PyObject *mdict, *tuple;
	PyObject *gedit, *geditutils, *geditcommands;
	PyObject *gettext, *install, *gettext_args;
	struct sigaction old_sigint;
	gint res;
	char *argv[] = { "gedit", NULL };

	if (loader->priv->init_failed)
	{
		/* We already failed to initialized Python, don't need to
		 * retry again */
		return FALSE;
	}
	
	if (Py_IsInitialized ())
	{
		/* Python has already been successfully initialized */
		return TRUE;
	}

	/* We are trying to initialize Python for the first time,
	   set init_failed to FALSE only if the entire initialization process
	   ends with success */
	loader->priv->init_failed = TRUE;

	/* Hack to make python not overwrite SIGINT: this is needed to avoid
	 * the crash reported on bug #326191 */

	/* CHECK: can't we use Py_InitializeEx instead of Py_Initialize in order
          to avoid to manage signal handlers ? - Paolo (Dec. 31, 2006) */

	/* Save old handler */
	res = sigaction (SIGINT, NULL, &old_sigint);  
	if (res != 0)
	{
		g_warning ("Error initializing Python interpreter: cannot get "
		           "handler to SIGINT signal (%s)",
		           strerror (errno));

		return FALSE;
	}

	/* Python initialization */
	Py_Initialize ();

	/* Restore old handler */
	res = sigaction (SIGINT, &old_sigint, NULL);
	if (res != 0)
	{
		g_warning ("Error initializing Python interpreter: cannot restore "
		           "handler to SIGINT signal (%s).",
		           strerror (errno));

		goto python_init_error;
	}

	PySys_SetArgv (1, argv);

	if (!gedit_check_pygtk2 ())
	{
		/* Warning message already printed in check_pygtk2 */
		goto python_init_error;
	}

	/* import gobject */	
	gedit_init_pygobject ();
	if (PyErr_Occurred ())
	{
		g_warning ("Error initializing Python interpreter: could not import pygobject.");

		goto python_init_error;		
	}

	/* import gtk */
	gedit_init_pygtk ();
	if (PyErr_Occurred ())
	{
		g_warning ("Error initializing Python interpreter: could not import pygtk.");

		goto python_init_error;
	}
	
	/* import gtksourceview */
	gedit_init_pygtksourceview ();
	if (PyErr_Occurred ())
	{
		PyErr_Print ();

		g_warning ("Error initializing Python interpreter: could not import pygtksourceview.");

		goto python_init_error;
	}	
	
	/* import gedit */
	gedit = Py_InitModule ("gedit", pygedit_functions);
	mdict = PyModule_GetDict (gedit);

	pygedit_register_classes (mdict);
	pygedit_add_constants (gedit, "GEDIT_");

	/* gedit version */
	tuple = Py_BuildValue("(iii)", 
			      GEDIT_MAJOR_VERSION,
			      GEDIT_MINOR_VERSION,
			      GEDIT_MICRO_VERSION);
	PyDict_SetItemString(mdict, "version", tuple);
	Py_DECREF(tuple);
	
	/* Retrieve the Python type for gedit.Plugin */
	PyGeditPlugin_Type = (PyTypeObject *) PyDict_GetItemString (mdict, "Plugin"); 
	if (PyGeditPlugin_Type == NULL)
	{
		PyErr_Print ();

		goto python_init_error;
	}

	/* import gedit.utils */
	geditutils = Py_InitModule ("gedit.utils", pygeditutils_functions);
	PyDict_SetItemString (mdict, "utils", geditutils);

	/* import gedit.commands */
	geditcommands = Py_InitModule ("gedit.commands", pygeditcommands_functions);
	PyDict_SetItemString (mdict, "commands", geditcommands);

	mdict = PyModule_GetDict (geditutils);
	pygeditutils_register_classes (mdict);
	
	mdict = PyModule_GetDict (geditcommands);
	pygeditcommands_register_classes (mdict);

	/* i18n support */
	gettext = PyImport_ImportModule ("gettext");
	if (gettext == NULL)
	{
		g_warning ("Error initializing Python interpreter: could not import gettext.");

		goto python_init_error;
	}

	mdict = PyModule_GetDict (gettext);
	install = PyDict_GetItemString (mdict, "install");
	gettext_args = Py_BuildValue ("ss", GETTEXT_PACKAGE, GEDIT_LOCALEDIR);
	PyObject_CallObject (install, gettext_args);
	Py_DECREF (gettext_args);
	
	/* Python has been successfully initialized */
	loader->priv->init_failed = FALSE;
	
	return TRUE;
	
python_init_error:

	g_warning ("Please check the installation of all the Python related packages required "
	           "by gedit and try again.");

	PyErr_Clear ();

	gedit_python_shutdown (loader);

	return FALSE;
}

static void
gedit_plugin_loader_python_finalize (GObject *object)
{
	GeditPluginLoaderPython *pyloader = GEDIT_PLUGIN_LOADER_PYTHON (object);
	
	g_hash_table_destroy (pyloader->priv->loaded_plugins);
	gedit_python_shutdown (pyloader);

	G_OBJECT_CLASS (gedit_plugin_loader_python_parent_class)->finalize (object);
}

static void
gedit_plugin_loader_python_class_init (GeditPluginLoaderPythonClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	object_class->finalize = gedit_plugin_loader_python_finalize;

	g_type_class_add_private (object_class, sizeof (GeditPluginLoaderPythonPrivate));
}

static void
destroy_python_info (PythonInfo *info)
{
	PyGILState_STATE state = pyg_gil_state_ensure ();
	Py_DECREF (info->type);	
	pyg_gil_state_release (state);
	
	g_free (info->path);
	g_free (info);
}

static void
gedit_plugin_loader_python_init (GeditPluginLoaderPython *self)
{
	self->priv = GEDIT_PLUGIN_LOADER_PYTHON_GET_PRIVATE (self);
	
	/* initialize python interpreter */
	gedit_python_init (self);

	/* loaded_plugins maps GeditPluginInfo to a PythonInfo */
	self->priv->loaded_plugins = g_hash_table_new_full (g_direct_hash,
						            g_direct_equal,
						            NULL,
						            (GDestroyNotify)destroy_python_info);
}

GeditPluginLoaderPython *
gedit_plugin_loader_python_new ()
{
	GObject *loader = g_object_new (GEDIT_TYPE_PLUGIN_LOADER_PYTHON, NULL);

	return GEDIT_PLUGIN_LOADER_PYTHON (loader);
}

