#ifndef __GEDIT_MESSAGE_BUS_H__
#define __GEDIT_MESSAGE_BUS_H__

#include <glib-object.h>
#include <gedit/gedit-message.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_MESSAGE_BUS			(gedit_message_bus_get_type ())
#define GEDIT_MESSAGE_BUS(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_MESSAGE_BUS, GeditMessageBus))
#define GEDIT_MESSAGE_BUS_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_MESSAGE_BUS, GeditMessageBus const))
#define GEDIT_MESSAGE_BUS_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), GEDIT_TYPE_MESSAGE_BUS, GeditMessageBusClass))
#define GEDIT_IS_MESSAGE_BUS(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEDIT_TYPE_MESSAGE_BUS))
#define GEDIT_IS_MESSAGE_BUS_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_MESSAGE_BUS))
#define GEDIT_MESSAGE_BUS_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GEDIT_TYPE_MESSAGE_BUS, GeditMessageBusClass))

typedef struct _GeditMessageBus		GeditMessageBus;
typedef struct _GeditMessageBusClass	GeditMessageBusClass;
typedef struct _GeditMessageBusPrivate	GeditMessageBusPrivate;

struct _GeditMessageBus {
	GObject parent;
	
	GeditMessageBusPrivate *priv;
};

struct _GeditMessageBusClass {
	GObjectClass parent_class;
};

typedef void (* GeditMessageCallback) 	(GeditMessageBus	*bus,
					 GeditMessage		*message,
					 gpointer		 userdata);

GType gedit_message_bus_get_type (void) G_GNUC_CONST;

GeditMessageBus *gedit_message_bus_get 	  (void);
guint gedit_message_bus_connect	 	  (GeditMessageBus	*bus, 
					   const gchar		*domain,
					   const gchar		*name,
					   GeditMessageCallback	 callback,
					   gpointer		 userdata,
					   GDestroyNotify        destroy_data);
			     
void gedit_message_bus_disconnect	  (GeditMessageBus	*bus,
					   guint		 id);

void gedit_message_bus_disconnect_by_func (GeditMessageBus	*bus,
					   const gchar		*domain,
					   const gchar		*name,
					   GeditMessageCallback	 callback,
					   gpointer		 userdata);

void gedit_message_bus_block		  (GeditMessageBus	*bus,
					   guint		 id);
void gedit_message_bus_block_by_func	  (GeditMessageBus	*bus,
					   const gchar		*domain,
					   const gchar		*name,
					   GeditMessageCallback	 callback,
					   gpointer		 userdata);

void gedit_message_bus_unblock		  (GeditMessageBus	*bus,
					   guint		 id);
void gedit_message_bus_unblock_by_func	  (GeditMessageBus	*bus,
					   const gchar		*domain,
					   const gchar		*name,
					   GeditMessageCallback	 callback,
					   gpointer		 userdata);

void gedit_message_bus_send_message	  (GeditMessageBus	*bus,
					   GeditMessage		*message);

void gedit_message_bus_send_message_sync  (GeditMessageBus	*bus,
					   GeditMessage		*message);
					  
void gedit_message_bus_send		  (GeditMessageBus	*bus,
					   const gchar		*domain,
					   const gchar		*name,
					   ...) G_GNUC_NULL_TERMINATED;
GeditMessage *gedit_message_bus_send_sync (GeditMessageBus	*bus,
					   const gchar		*domain,
					   const gchar		*name,
					   ...) G_GNUC_NULL_TERMINATED;
G_END_DECLS

#endif /* __GEDIT_MESSAGE_BUS_H__ */
