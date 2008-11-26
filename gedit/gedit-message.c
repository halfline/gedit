#include "gedit-message.h"

#include <string.h>
#include <gobject/gvaluecollector.h>

#define GEDIT_MESSAGE_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GEDIT_TYPE_MESSAGE, GeditMessagePrivate))

enum {
	PROP_0,

	PROP_DOMAIN,
	PROP_NAME
};

struct _GeditMessagePrivate
{
	gchar *domain;
	gchar *name;
	
	GHashTable *values;
};

G_DEFINE_TYPE (GeditMessage, gedit_message, G_TYPE_OBJECT)

static void
gedit_message_finalize (GObject *object)
{
	GeditMessage *message = GEDIT_MESSAGE (object);
	
	g_free (message->priv->domain);
	g_free (message->priv->name);
	
	g_hash_table_destroy (message->priv->values);

	G_OBJECT_CLASS (gedit_message_parent_class)->finalize (object);
}

static void
gedit_message_get_property (GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
	GeditMessage *msg = GEDIT_MESSAGE (object);

	switch (prop_id)
	{
		case PROP_DOMAIN:
			g_value_set_string (value, msg->priv->domain);
			break;
		case PROP_NAME:
			g_value_set_string (value, msg->priv->name);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_message_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
	GeditMessage *msg = GEDIT_MESSAGE (object);

	switch (prop_id)
	{
		case PROP_DOMAIN:
			msg->priv->domain = g_strdup (g_value_get_string (value));
			break;
		case PROP_NAME:
			msg->priv->name = g_strdup (g_value_get_string (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_message_class_init (GeditMessageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	
	object_class->finalize = gedit_message_finalize;
	object_class->get_property = gedit_message_get_property;
	object_class->set_property = gedit_message_set_property;
	
	g_object_class_install_property (object_class, PROP_DOMAIN,
					 g_param_spec_string ("domain",
							      "DOMAIN",
							      "The message domain",
							      NULL,
							      G_PARAM_READWRITE |
							      G_PARAM_STATIC_STRINGS |
							      G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class, PROP_NAME,
					 g_param_spec_string ("name",
							      "NAME",
							      "The message name",
							      NULL,
							      G_PARAM_READWRITE |
							      G_PARAM_STATIC_STRINGS |
							      G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof(GeditMessagePrivate));
}

static void
destroy_value (GValue *value)
{
	g_value_unset (value);
	g_free (value);
}

static void
gedit_message_init (GeditMessage *self)
{
	self->priv = GEDIT_MESSAGE_GET_PRIVATE (self);

	self->priv->values = g_hash_table_new_full (g_str_hash,
						    g_str_equal,
						    (GDestroyNotify)g_free,
						    (GDestroyNotify)destroy_value);
}

gboolean
_gedit_message_gtype_supported (GType type)
{
	gint i = 0;
  
	static const GType type_list[] =
	{
		G_TYPE_BOOLEAN,
		G_TYPE_CHAR,
		G_TYPE_UCHAR,
		G_TYPE_INT,
		G_TYPE_UINT,
		G_TYPE_LONG,
		G_TYPE_ULONG,
		G_TYPE_INT64,
		G_TYPE_UINT64,
		G_TYPE_ENUM,
		G_TYPE_FLAGS,
		G_TYPE_FLOAT,
		G_TYPE_DOUBLE,
		G_TYPE_STRING,
		G_TYPE_POINTER,
		G_TYPE_BOXED,
		G_TYPE_OBJECT,
		G_TYPE_INVALID
	};

	if (!G_TYPE_IS_VALUE_TYPE (type))
		return FALSE;

	while (type_list[i] != G_TYPE_INVALID)
	{
		if (g_type_is_a (type, type_list[i]))
			return TRUE;
		i++;
	}

	return FALSE;
}

static void
add_value (GeditMessage *message,
	   const gchar 	*key,
	   GType	 gtype)
{
	GValue *value;
	
	value = g_new0 (GValue, 1);
	g_value_init (value, gtype);
	g_value_reset (value);

	g_hash_table_insert (message->priv->values, g_strdup (key), value);
}

static gboolean
set_value_real (GValue 	     *to, 
		const GValue *from)
{
	GType from_type;
	GType to_type;
	
	from_type = G_VALUE_TYPE (from);
	to_type = G_VALUE_TYPE (to);

	if (!g_type_is_a (from_type, to_type))
	{		
		if (!g_value_transform (from, to))
		{
			g_warning ("%s: Unable to make conversion from %s to %s",
				   G_STRLOC,
				   g_type_name (from_type),
				   g_type_name (to_type));
			return FALSE;
		}
		
		return TRUE;
	}
	
	g_value_copy (from, to);
	return TRUE;
}

inline static GValue *
value_lookup (GeditMessage *message,
	      const gchar  *key)
{
	return (GValue *)g_hash_table_lookup (message->priv->values, key);
}

GeditMessage*
gedit_message_new (const gchar *domain,
		   const gchar *name,
		   ...)
{
	va_list var_args;
	GeditMessage *message;
	
	va_start (var_args, name);
	message = gedit_message_new_valist (domain, name, var_args);
	va_end (var_args);
	
	return message;
}

GeditMessage *
gedit_message_new_valist (const gchar *domain,
			  const gchar *name,
			  va_list      var_args)
{
	GeditMessage *message;
	const gchar *key;
	GArray *keys;
	GArray *types;

	message = g_object_new (GEDIT_TYPE_MESSAGE, "domain", domain, "name", name, NULL);
	
	keys = g_array_new (FALSE, FALSE, sizeof (const gchar *));
	types = g_array_new (FALSE, FALSE, sizeof (GType));

	while ((key = va_arg (var_args, const gchar *)) != NULL)
	{
		/* get corresponding GType */
		GType gtype = va_arg (var_args, GType);
		
		g_array_append_val (keys, key);
		g_array_append_val (types, gtype);
	}
	
	gedit_message_set_types (message, 
				 (const gchar **)keys->data,
				 (GType *)types->data,
				 keys->len);
	
	g_array_free (keys, TRUE);
	g_array_free (types, TRUE);
	
	return message;
}

GeditMessage *
gedit_message_new_hash (const gchar *domain,
			const gchar *name,
			GHashTable  *values)
{
	GeditMessage *message;
	const gchar *key;
	GList *keys;
	GList *item;
	GValue *value;
	
	message = g_object_new (GEDIT_TYPE_MESSAGE, "domain", domain, "name", name, NULL);	
	keys = g_hash_table_get_keys (values);
	
	for (item = keys; item; item = item->next)
	{
		key = (const gchar *)(item->data);
		value = g_hash_table_lookup (values, key);
		
		if (value == NULL)
			continue;
		
		if (!_gedit_message_gtype_supported (G_VALUE_TYPE (value)))
		{
			g_warning ("GType %s is not supported, ignoring key %s", 
				   g_type_name (G_VALUE_TYPE (value)), 
				   key);
			continue;
		}
		
		add_value (message, key, G_VALUE_TYPE (value));
		gedit_message_set_value (message, key, value); 
	}
	
	g_list_free (keys);
	
	return message;
}

void
gedit_message_set_types (GeditMessage  *message,
			 const gchar  **keys,
			 GType	       *types,
			 gint		n_types)
{
	gint i;

	g_return_if_fail (GEDIT_IS_MESSAGE (message));
	
	g_hash_table_ref (message->priv->values);
	g_hash_table_destroy (message->priv->values);
	
	for (i = 0; i < n_types; i++)
	{
		if (!_gedit_message_gtype_supported (types[i]))
		{
			g_warning ("GType %s is not supported, ignoring key %s", g_type_name (types[i]), keys[i]);
			continue;
		}
		
		add_value (message, keys[i], types[i]);
	}	
}

const gchar *
gedit_message_get_name (GeditMessage *message)
{
	g_return_val_if_fail (GEDIT_IS_MESSAGE (message), NULL);
	
	return message->priv->name;
}

const gchar *
gedit_message_get_domain (GeditMessage *message)
{
	g_return_val_if_fail (GEDIT_IS_MESSAGE (message), NULL);
	
	return message->priv->domain;
}

void
gedit_message_set (GeditMessage *message,
		   ...)
{
	va_list ap;

	g_return_if_fail (GEDIT_IS_MESSAGE (message));

	va_start (ap, message);
	gedit_message_set_valist (message, ap);
	va_end (ap);
}

void
gedit_message_set_valist (GeditMessage *message,
			  va_list	var_args)
{
	const gchar *key;

	g_return_if_fail (GEDIT_IS_MESSAGE (message));

	while ((key = va_arg (var_args, const gchar *)) != NULL)
	{
		/* lookup the key */
		GValue *container = value_lookup (message, key);
		GValue value = {0,};
		gchar *error = NULL;
		
		if (!container)
		{
			g_warning ("%s: Cannot set value for %s, does not exist", 
				   G_STRLOC,
				   key);
			
			/* skip value */
			va_arg (var_args, gpointer);
			continue;
		}
		
		g_value_init (&value, G_VALUE_TYPE (container));
		G_VALUE_COLLECT (&value, var_args, 0, &error);
		
		if (error)
		{
			g_warning ("%s: %s", G_STRLOC, error);
			continue;
		}

		set_value_real (container, &value);
		g_value_unset (&value);
	}
}

void
gedit_message_set_value (GeditMessage *message,
			 const gchar  *key,
			 GValue	      *value)
{
	GValue *container;
	g_return_if_fail (GEDIT_IS_MESSAGE (message));
	
	container = value_lookup (message, key);
	
	if (!container)
	{
		g_warning ("%s: Cannot set value for %s, does not exist", 
			   G_STRLOC, 
			   key);
		return;
	}
	
	set_value_real (container, value);
}

void
gedit_message_set_valuesv (GeditMessage	 *message,
			   const gchar	**keys,
			   GValue        *values,
			   gint		  n_values)
{
	gint i;
	
	g_return_if_fail (GEDIT_IS_MESSAGE (message));
	
	for (i = 0; i < n_values; i++)
	{
		gedit_message_set_value (message, keys[i], &values[i]);
	}
}

void 
gedit_message_get (GeditMessage	*message,
		   ...)
{
	va_list ap;

	g_return_if_fail (GEDIT_IS_MESSAGE (message));
	
	va_start (ap, message);
	gedit_message_get_valist (message, ap);
	va_end (ap);
}

void
gedit_message_get_valist (GeditMessage *message,
			  va_list 	var_args)
{
	const gchar *key;

	g_return_if_fail (GEDIT_IS_MESSAGE (message));
	
	while ((key = va_arg (var_args, const gchar *)) != NULL)
	{
		GValue *container;
		GValue copy = {0,};
		gchar *error = NULL;

		container = value_lookup (message, key);
	
		if (!container)
		{		
			/* skip value */
			va_arg (var_args, gpointer);
			continue;
		}
		
		/* copy the value here, to be sure it isn't tainted */
		g_value_init (&copy, G_VALUE_TYPE (container));
		g_value_copy (container, &copy);
		
		G_VALUE_LCOPY (&copy, var_args, 0, &error);
		
		if (error)
		{
			g_warning ("%s: %s", G_STRLOC, error);
			g_free (error);
			
			/* purposely leak the value here, because it might
			   be in a bad state */
			continue;
		}
		
		g_value_unset (&copy);
	}
}

void 
gedit_message_get_value (GeditMessage *message,
			 const gchar  *key,
			 GValue	      *value)
{
	GValue *container;
	
	g_return_if_fail (GEDIT_IS_MESSAGE (message));
	
	container = value_lookup (message, key);
	
	if (!container)
	{
		g_warning ("%s: Invalid key `%s'",
			   G_STRLOC,
			   key);
		return;
	}
	
	g_value_init (value, G_VALUE_TYPE (container));
	set_value_real (value, container);
}

GType 
gedit_message_get_key_type (GeditMessage    *message,
			    const gchar	    *key)
{
	GValue *container;

	g_return_val_if_fail (GEDIT_IS_MESSAGE (message), 0);
	
	container = value_lookup (message, key);
	
	if (!container)
	{
		g_warning ("%s: Invalid key `%s'",
			   G_STRLOC,
			   key);
		return 0;
	}
	
	return G_VALUE_TYPE (container);
}

GStrv
gedit_message_get_keys (GeditMessage *message)
{
	GList *keys;
	GList *item;
	GStrv result;
	gint i = 0;
	
	g_return_val_if_fail (GEDIT_IS_MESSAGE (message), NULL);
	
	result = g_new0 (gchar *, g_hash_table_size (message->priv->values) + 1);
	
	keys = g_hash_table_get_keys (message->priv->values);
	
	for (item = keys; item; item = item->next)
		result[i++] = g_strdup ((gchar *)(item->data));
	
	g_list_free (keys);
	return result;
}

GHashTable *
gedit_message_get_hash (GeditMessage *message)
{
	g_return_val_if_fail (GEDIT_IS_MESSAGE (message), NULL);
	return message->priv->values;
}

gboolean
gedit_message_has_key (GeditMessage *message,
		       const gchar  *key)
{
	GValue *container;
	
	g_return_val_if_fail (GEDIT_IS_MESSAGE (message), FALSE);
	
	container = value_lookup (message, key);
	
	return container != NULL;
}
