#include "gedit-message-bus.h"

#include <string.h>
#include <stdarg.h>
#include <gobject/gvaluecollector.h>

#define GEDIT_MESSAGE_BUS_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GEDIT_TYPE_MESSAGE_BUS, GeditMessageBusPrivate))

typedef struct
{
	gchar *domain;
	gchar *name;

	GList *listeners;
} Message;

typedef struct
{
	gchar *key;
	GType type;
	gboolean required;
} PolicyItem;

typedef struct
{
	gchar *domain;
	gchar *name;
	
	GList *policies; /* list of PolicyItem */
} Policy;

typedef struct
{
	guint id;
	gboolean blocked;

	GDestroyNotify destroy_data;
	GeditMessageCallback callback;
	gpointer userdata;
} Listener;

typedef struct
{
	Message *message;
	GList *listener;
} IdMap;

struct _GeditMessageBusPrivate
{
	GHashTable *messages;
	GHashTable *idmap;

	GList *message_queue;
	guint idle_id;

	guint next_id;
	
	GHashTable *policies;
};

/* signals */
enum
{
	DISPATCH,
	LAST_SIGNAL
};

static guint message_bus_signals[LAST_SIGNAL];

static void gedit_message_bus_dispatch_real (GeditMessageBus *bus,
				 	     GeditMessage    *message);

G_DEFINE_TYPE(GeditMessageBus, gedit_message_bus, G_TYPE_OBJECT)

static void
policy_item_free (PolicyItem *pitem)
{
	g_free (pitem->key);
	g_free (pitem);
}

static void
policy_free (Policy *policy)
{
	GList *item;
	
	g_free (policy->domain);
	g_free (policy->name);
	
	for (item = policy->policies; item; item = item->next)
		policy_item_free ((PolicyItem *)item->data);
	
	g_list_free (policy->policies);
	g_free (policy);
}

static void
listener_free (Listener *listener)
{
	if (listener->destroy_data)
		listener->destroy_data (listener->userdata);

	g_free (listener);
}

static void
message_free (Message *message)
{
	g_free (message->name);
	g_free (message->domain);
	
	g_list_foreach (message->listeners, (GFunc)listener_free, NULL);
	g_list_free (message->listeners);
	
	g_free (message);
}

static void
message_queue_free (GList *queue)
{
	g_list_foreach (queue, (GFunc)g_object_unref, NULL);
	g_list_free (queue);
}

static void
gedit_message_bus_finalize (GObject *object)
{
	GeditMessageBus *bus = GEDIT_MESSAGE_BUS (object);
	
	if (bus->priv->idle_id != 0)
		g_source_remove (bus->priv->idle_id);
	
	message_queue_free (bus->priv->message_queue);
	g_hash_table_destroy (bus->priv->messages);
	g_hash_table_destroy (bus->priv->idmap);
	
	G_OBJECT_CLASS (gedit_message_bus_parent_class)->finalize (object);
}

static void
gedit_message_bus_class_init (GeditMessageBusClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	object_class->finalize = gedit_message_bus_finalize;
	
	klass->dispatch = gedit_message_bus_dispatch_real;

	message_bus_signals[DISPATCH] =
   		g_signal_new ("dispatch",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GeditMessageBusClass, dispatch),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GEDIT_TYPE_MESSAGE);

	g_type_class_add_private (object_class, sizeof(GeditMessageBusPrivate));
}

inline static gchar *
msg_identifier (const gchar *domain,
		const gchar *name)
{
	return g_strconcat (domain, "::", name, NULL);
}

static Message *
message_new (GeditMessageBus *bus,
	     const gchar     *domain,
	     const gchar     *name)
{
	Message *message = g_new (Message, 1);
	
	message->domain = g_strdup (domain);
	message->name = g_strdup (name);
	message->listeners = NULL;

	g_hash_table_insert (bus->priv->messages, 
			     msg_identifier (domain, name),
			     message);
	return message;
}

static Message *
lookup_message (GeditMessageBus *bus,
	       const gchar      *domain,
	       const gchar      *name,
	       gboolean          create)
{
	gchar *identifier;
	Message *message;
	
	identifier = msg_identifier (domain, name);
	message = (Message *)g_hash_table_lookup (bus->priv->messages, identifier);
	g_free (identifier);

	if (!message && !create)
		return NULL;
	
	if (!message)
		message = message_new (bus, domain, name);
	
	return message;
}

static guint
add_listener (GeditMessageBus      *bus,
	      Message		   *message,
	      GeditMessageCallback  callback,
	      gpointer		    userdata,
	      GDestroyNotify        destroy_data)
{
	Listener *listener;
	IdMap *idmap;
	
	listener = g_new (Listener, 1);
	listener->id = ++bus->priv->next_id;
	listener->callback = callback;
	listener->userdata = userdata;
	listener->blocked = FALSE;
	listener->destroy_data = destroy_data;

	message->listeners = g_list_append (message->listeners, listener);
	
	idmap = g_new (IdMap, 1);
	idmap->message = message;
	idmap->listener = g_list_last (message->listeners);

	g_hash_table_insert (bus->priv->idmap, GINT_TO_POINTER (listener->id), idmap);	
	return listener->id;
}

static void
remove_listener (GeditMessageBus *bus,
		 Message         *message,
		 GList		 *listener)
{
	Listener *lst;
	
	lst = (Listener *)listener->data;
	
	/* remove from idmap */
	g_hash_table_remove (bus->priv->idmap, GINT_TO_POINTER (lst->id));
	listener_free (lst);

	/* remove from list of listeners */
	message->listeners = g_list_delete_link (message->listeners, listener);
	
	if (!message->listeners)
	{
		/* remove message because it does not have any listeners */
		g_hash_table_remove (bus->priv->messages, message);
	}
}

static void
block_listener (GeditMessageBus *bus,
		Message		*message,
		GList		*listener)
{
	Listener *lst;
	
	lst = (Listener *)listener->data;
	lst->blocked = TRUE;
}

static void
unblock_listener (GeditMessageBus *bus,
		  Message	  *message,
		  GList		  *listener)
{
	Listener *lst;
	
	lst = (Listener *)listener->data;
	lst->blocked = FALSE;
}

static void
dispatch_message_real (GeditMessageBus *bus,
		       Message         *msg,
		       GeditMessage    *message)
{
	GList *item;
	
	for (item = msg->listeners; item; item = item->next)
	{
		Listener *listener = (Listener *)item->data;
		
		if (!listener->blocked)
			listener->callback (bus, message, listener->userdata);
	}
}

static void
gedit_message_bus_dispatch_real (GeditMessageBus *bus,
				 GeditMessage    *message)
{
	const gchar *domain;
	const gchar *name;
	Message *msg;
	
	domain = gedit_message_get_domain (message);
	name = gedit_message_get_name (message);

	msg = lookup_message (bus, domain, name, FALSE);
	
	if (msg)
		dispatch_message_real (bus, msg, message);
}

static void
dispatch_message (GeditMessageBus *bus,
		  GeditMessage    *message)
{
	g_signal_emit (bus, message_bus_signals[DISPATCH], 0, message);	
}

static gboolean
idle_dispatch (GeditMessageBus *bus)
{
	GList *list;
	GList *item;
	
	/* make sure to set idle_id to 0 first so that any new async messages
	   will be queued properly */
	bus->priv->idle_id = 0;

	/* reverse queue to get correct delivery order */
	list = g_list_reverse (bus->priv->message_queue);
	bus->priv->message_queue = NULL;
	
	for (item = list; item; item = item->next)
	{
		GeditMessage *msg = GEDIT_MESSAGE (item->data);
		
		dispatch_message (bus, msg);
	}
	
	message_queue_free (list);
	return FALSE;
}

typedef void (*MatchCallback) (GeditMessageBus *, Message *, GList *);

static void
process_by_id (GeditMessageBus  *bus,
	       guint	         id,
	       MatchCallback     processor)
{
	IdMap *idmap;
	
	idmap = (IdMap *)g_hash_table_lookup (bus->priv->idmap, GINT_TO_POINTER (id));
	
	if (idmap == NULL)
	{
		g_warning ("No handler registered with id `%d'", id);
		return;
	}
		
	processor (bus, idmap->message, idmap->listener);
}

static void
process_by_match (GeditMessageBus      *bus,
	          const gchar          *domain,
	          const gchar          *name,
	          GeditMessageCallback  callback,
	          gpointer              userdata,
	          MatchCallback         processor)
{
	Message *message;
	GList *item;
	
	message = lookup_message (bus, domain, name, FALSE);
	
	if (!message)
	{
		g_warning ("No such handler registered for %s::%s", domain, name);
		return;
	}
	
	for (item = message->listeners; item; item = item->next)
	{
		Listener *listener = (Listener *)item->data;
		
		if (listener->callback == callback && 
		    listener->userdata == userdata)
		{
			processor (bus, message, item);
			return;
		}
	}
	
	g_warning ("No such handler registered for %s::%s", domain, name);
}

static void
gedit_message_bus_init (GeditMessageBus *self)
{
	self->priv = GEDIT_MESSAGE_BUS_GET_PRIVATE (self);
	
	self->priv->messages = g_hash_table_new_full (g_str_hash,
						      g_str_equal,
						      (GDestroyNotify)g_free,
						      (GDestroyNotify)message_free);

	self->priv->idmap = g_hash_table_new_full (g_direct_hash,
	 					   g_direct_equal,
	 					   NULL,
	 					   (GDestroyNotify)g_free);
	 					   
	self->priv->policies = g_hash_table_new_full (g_str_hash,
						      g_str_equal,
						      (GDestroyNotify)g_free,
						      (GDestroyNotify)policy_free);
}

GeditMessageBus *
gedit_message_bus_get_default (void)
{
	static GeditMessageBus *default_bus = NULL;
	
	if (G_UNLIKELY (default_bus == NULL))
	{
		default_bus = g_object_new (GEDIT_TYPE_MESSAGE_BUS, NULL);
		g_object_add_weak_pointer (G_OBJECT (default_bus),
				           (gpointer) &default_bus);
	}
	
	return default_bus;
}

void
gedit_message_bus_register (GeditMessageBus *bus,
			    const gchar     *domain,
			    const gchar	    *name,
			    guint	     num_optional,
			    ...)
{
	gchar *identifier;
	gpointer data;
	Policy *policy;
	va_list var_args;
	const gchar *key;
	GList *item;
	
	g_return_if_fail (GEDIT_IS_MESSAGE_BUS (bus));
	
	identifier = msg_identifier (domain, name);
	data = g_hash_table_lookup (bus->priv->policies, identifier);
	
	if (data != NULL)
	{
		/* policy is already registered */
		g_free (identifier);
		return;
	}
	
	policy = g_new(Policy, 1);
	policy->domain = g_strdup (domain);
	policy->name = g_strdup (name);
	policy->policies = NULL;
	
	/* construct policy items */
	va_start (var_args, num_optional);

	/* read in required items */
	while ((key = va_arg (var_args, const gchar *)) != NULL)
	{
		GType gtype;
		PolicyItem *pitem;
		
		gtype = va_arg (var_args, GType);
		
		if (!_gedit_message_gtype_supported (gtype))
		{
			g_warning ("Type `%s' is not supported on the message bus",
				   g_type_name (gtype));
			continue;
		}
		
		pitem = g_new(PolicyItem, 1);
		pitem->key = g_strdup (key);
		pitem->type = gtype;

		policy->policies = g_list_prepend (policy->policies, pitem);
	}
	
	for (item = policy->policies; num_optional-- > 0 && item; item = item->next)
		((PolicyItem *)item->data)->required = FALSE;
	
	policy->policies = g_list_reverse (policy->policies);
	g_hash_table_insert (bus->priv->policies, identifier, policy);
}

void
gedit_message_bus_unregister (GeditMessageBus *bus,
			      const gchar     *domain,
			      const gchar     *name)
{
	gchar *identifier;
	
	g_return_if_fail (GEDIT_IS_MESSAGE_BUS (bus));

	identifier = msg_identifier (domain, name);
	g_hash_table_remove (bus->priv->policies, name);
	g_free (identifier);
}

void
gedit_message_bus_unregister_all (GeditMessageBus *bus,
			          const gchar     *domain)
{
	GList *pols;
	GList *item;
	g_return_if_fail (GEDIT_IS_MESSAGE_BUS (bus));

	pols = g_hash_table_get_values (bus->priv->policies);
	
	for (item = pols; item; item = item->next)
	{
		Policy *policy = (Policy *)item->data;
		
		if (strcmp (policy->domain, domain) == 0)
		{
			gchar *identifier = msg_identifier (policy->domain, policy->name);
			g_hash_table_remove (bus->priv->policies, identifier);
			g_free (identifier);
		}
	}
	
	g_list_free (pols);
}

guint
gedit_message_bus_connect (GeditMessageBus	*bus, 
		           const gchar		*domain,
		           const gchar		*name,
		           GeditMessageCallback  callback,
		           gpointer		 userdata,
		           GDestroyNotify	 destroy_data)
{
	Message *message;

	g_return_val_if_fail (GEDIT_IS_MESSAGE_BUS (bus), 0);
	g_return_val_if_fail (domain != NULL, 0);
	g_return_val_if_fail (name != NULL, 0);
	g_return_val_if_fail (callback != NULL, 0);
	
	/* lookup the message and create if it does not exist yet */
	message = lookup_message (bus, domain, name, TRUE);
	
	return add_listener (bus, message, callback, userdata, destroy_data);
}

void
gedit_message_bus_disconnect (GeditMessageBus *bus,
			      guint            id)
{
	g_return_if_fail (GEDIT_IS_MESSAGE_BUS (bus));
	
	process_by_id (bus, id, remove_listener);
}

void
gedit_message_bus_disconnect_by_func (GeditMessageBus      *bus,
				      const gchar	   *domain,
				      const gchar	   *name,
				      GeditMessageCallback  callback,
				      gpointer		    userdata)
{
	g_return_if_fail (GEDIT_IS_MESSAGE_BUS (bus));
	
	process_by_match (bus, domain, name, callback, userdata, remove_listener);
}

void
gedit_message_bus_block (GeditMessageBus *bus,
			 guint		  id)
{
	g_return_if_fail (GEDIT_IS_MESSAGE_BUS (bus));
	
	process_by_id (bus, id, block_listener);
}

void
gedit_message_bus_block_by_func (GeditMessageBus      *bus,
				 const gchar	      *domain,
				 const gchar	      *name,
				 GeditMessageCallback  callback,
				 gpointer	       userdata)
{
	g_return_if_fail (GEDIT_IS_MESSAGE_BUS (bus));
	
	process_by_match (bus, domain, name, callback, userdata, block_listener);
}

void
gedit_message_bus_unblock (GeditMessageBus *bus,
			   guint	    id)
{
	g_return_if_fail (GEDIT_IS_MESSAGE_BUS (bus));
	
	process_by_id (bus, id, unblock_listener);
}

void
gedit_message_bus_unblock_by_func (GeditMessageBus      *bus,
				   const gchar	        *domain,
				   const gchar	        *name,
				   GeditMessageCallback  callback,
				   gpointer	         userdata)
{
	g_return_if_fail (GEDIT_IS_MESSAGE_BUS (bus));
	
	process_by_match (bus, domain, name, callback, userdata, unblock_listener);
}

static gboolean
check_message_policy (GeditMessageBus *bus,
		      GeditMessage    *message)
{
	const gchar *domain = gedit_message_get_domain (message);
	const gchar *name = gedit_message_get_name (message);
	
	gchar *identifier = msg_identifier (domain, name);
	Policy *policy = (Policy *)g_hash_table_lookup (bus->priv->policies, identifier);
	PolicyItem *pitem;
	GList *item;
	
	g_free (identifier);

	if (policy == NULL)
	{
		g_warning ("Policy for '%s::%s' could not be found", domain, name);
		return FALSE;
	}
	
	/* check all required and optional arguments */
	for (item = policy->policies; item; item = item->next)
	{
		GType gtype;
		gboolean haskey;
		
		pitem = (PolicyItem *)item->data;
		
		/* check if key exists */
		haskey = gedit_message_has_key (message, pitem->key);
		
		if (pitem->required && !haskey)
		{
			g_warning ("Required key %s not found for %s::%s", pitem->key, domain, name);
			return FALSE;
		}
		else if (!haskey && !pitem->required)
		{
			continue;
		}
		
		gtype = gedit_message_get_key_type (message, pitem->key);
		
		if (!g_type_is_a (pitem->type, gtype) && 
		    !g_value_type_transformable (pitem->type, gtype))
		{
			g_warning ("Invalid key type for %s::%s, expected %s, got %s", domain, name, g_type_name (pitem->type), g_type_name (gtype));
			return FALSE;
		}
	}
	
	return TRUE;
}

void
gedit_message_bus_send_message (GeditMessageBus *bus,
			        GeditMessage    *message)
{
	g_return_if_fail (GEDIT_IS_MESSAGE_BUS (bus));
	g_return_if_fail (GEDIT_IS_MESSAGE (message));
	
	if (!check_message_policy (bus, message))
		return;
	
	bus->priv->message_queue = g_list_prepend (bus->priv->message_queue, 
						   g_object_ref (message));

	if (bus->priv->idle_id == 0)
		bus->priv->idle_id = g_idle_add_full (G_PRIORITY_HIGH,
						      (GSourceFunc)idle_dispatch,
						      bus,
						      NULL);
}

void
gedit_message_bus_send_message_sync (GeditMessageBus *bus,
			             GeditMessage    *message)
{
	g_return_if_fail (GEDIT_IS_MESSAGE_BUS (bus));
	g_return_if_fail (GEDIT_IS_MESSAGE (message));
	
	if (!check_message_policy (bus, message))
		return;

	dispatch_message (bus, message);
}

static GType
policy_find_type (Policy      *policy, 
		  const gchar *key)
{
	GList *item;
	
	for (item = policy->policies; item; item = item->next)
	{
		PolicyItem *pitem = (PolicyItem *)item->data;
		
		if (strcmp (pitem->key, key) == 0)
			return pitem->type;
	}
	
	return 0;
}

static GeditMessage *
create_message (GeditMessageBus *bus,
		const gchar     *domain,
		const gchar     *name,
		va_list          var_args)
{
	const gchar **keys = NULL;
	GType *types = NULL;
	GValue *values = NULL;
	gint num = 0;
	const gchar *key;
	gchar *identifier;
	GeditMessage *message;
	Policy *policy;
	gint i;
	
	identifier = msg_identifier (domain, name);
	policy = g_hash_table_lookup (bus->priv->policies, identifier);
	g_free (identifier);
	
	if (policy == NULL)
	{
		g_warning ("Policy for %s::%s not found", domain, name);
		return NULL;
	}
	
	while ((key = va_arg (var_args, const gchar *)) != NULL)
	{
		gchar *error = NULL;
		GType gtype;
		GValue value = {0,};
		
		gtype = policy_find_type (policy, key);
		
		if (!gtype)
		{
			g_warning ("Key %s not registered in policy for %s::%s", key, domain, name);
			return NULL;
		}
		
		g_value_init (&value, gtype);
		G_VALUE_COLLECT (&value, var_args, 0, &error);
		
		if (error)
		{
			g_warning ("%s: %s", G_STRLOC, error);
			g_free (error);
			
			/* leak value, might have gone bad */
			continue;
		}
		
		num++;
		
		keys = g_realloc (keys, sizeof(gchar *) * num);
		types = g_realloc (types, sizeof(GType) * num);
		values = g_realloc (values, sizeof(GValue) * num);
		
		keys[num - 1] = key;
		types[num - 1] = gtype;
		
		memset (&values[num - 1], 0, sizeof(GValue));
		g_value_init (&values[num - 1], types[num - 1]);
		g_value_copy (&value, &values[num - 1]);
		
		g_value_unset (&value);
	}
		
	message = gedit_message_new (domain, name, NULL);
	gedit_message_set_types (message, keys, types, num);
	gedit_message_set_valuesv (message, keys, values, num);
	
	g_free (keys);
	g_free (types);
	
	for (i = 0; i < num; i++)
		g_value_unset (&values[i]);

	g_free (values);
	
	return message;
}

void
gedit_message_bus_send (GeditMessageBus *bus,
			const gchar     *domain,
			const gchar     *name,
			...)
{
	va_list var_args;
	GeditMessage *message;
	
	va_start (var_args, name);

	message = create_message (bus, domain, name, var_args);
	
	if (message)
	{
		gedit_message_bus_send_message (bus, message);
		g_object_unref (message);
	}

	va_end (var_args);
}

GeditMessage *
gedit_message_bus_send_sync (GeditMessageBus *bus,
			     const gchar     *domain,
			     const gchar     *name,
			     ...)
{
	va_list var_args;
	GeditMessage *message;
	
	va_start (var_args, name);
	message = create_message (bus, domain, name, var_args);
	
	if (message)
		gedit_message_bus_send_message_sync (bus, message);

	va_end (var_args);
	
	return message;
}

typedef struct
{
	gchar *domain;
	gchar *name;
	GeditMessageBus *bus;
} PropertyProxy;

static void
property_proxy_message (GObject         *object,
			GParamSpec      *spec,
			PropertyProxy   *proxy)
{
	GeditMessage *message;
	GValue value = {0,};
	
	message = gedit_message_new (proxy->domain,
				     proxy->name,
				     spec->name, spec->value_type,
				     NULL);

	g_value_init (&value, spec->value_type);
	g_object_get_property (object, spec->name, &value);
	gedit_message_set_value (message, spec->name, &value);
	    
	gedit_message_bus_send_message (proxy->bus, message);

	g_object_unref (message);
	g_value_unset (&value);
}

static PropertyProxy *
property_proxy_new (GeditMessageBus *bus,
		    const gchar     *domain,
		    const gchar     *name)
{
	PropertyProxy *proxy;
	
	proxy = g_new (PropertyProxy, 1);
	proxy->domain = g_strdup (domain);
	proxy->name = g_strdup (name);
	proxy->bus = bus;
	
	return proxy;
}

static void
property_proxy_free (PropertyProxy *proxy,
		     GClosure      *closure)
{
	g_free (proxy->domain);
	g_free (proxy->name);
	g_free (proxy);
}

void 
gedit_message_bus_proxy_property (GeditMessageBus *bus,
				  const gchar     *domain,
				  const gchar     *name,
				  GObject	  *object,
				  const gchar 	  *property)
{
	GParamSpec *spec;
	gchar *detailed;
	
	spec = g_object_class_find_property (G_OBJECT_GET_CLASS (object), 
					     property);

	if (!spec)
	{
		g_warning ("Could not connect proxy because property `%s' does not exist for %s", 
			   property,
			   g_type_name (G_OBJECT_TYPE (object)));
		return;
	}
	
	if (!_gedit_message_gtype_supported (spec->value_type))
	{
		g_warning ("Type of property `%s' is not supported on the message bus",
			   g_type_name (spec->value_type));
		return;
	}
	
	detailed = g_strconcat ("notify::", property, NULL);
	g_signal_connect_data (object, 
			       detailed, 
			       G_CALLBACK (property_proxy_message), 
			       property_proxy_new (bus, domain, name),
			       (GClosureNotify)property_proxy_free,
			       0);
	g_free (detailed);
}
