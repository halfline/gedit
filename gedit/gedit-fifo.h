#ifndef __GEDIT_FIFO_H__
#define __GEDIT_FIFO_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_FIFO				(gedit_fifo_get_type ())
#define GEDIT_FIFO(obj)				(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_FIFO, GeditFifo))
#define GEDIT_FIFO_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_FIFO, GeditFifo const))
#define GEDIT_FIFO_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), GEDIT_TYPE_FIFO, GeditFifoClass))
#define GEDIT_IS_FIFO(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEDIT_TYPE_FIFO))
#define GEDIT_IS_FIFO_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_FIFO))
#define GEDIT_FIFO_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GEDIT_TYPE_FIFO, GeditFifoClass))

typedef struct _GeditFifo			GeditFifo;
typedef struct _GeditFifoClass		GeditFifoClass;
typedef struct _GeditFifoPrivate	GeditFifoPrivate;

struct _GeditFifo {
	GObject parent;
	
	GeditFifoPrivate *priv;
};

struct _GeditFifoClass {
	GObjectClass parent_class;
};

GType gedit_fifo_get_type (void) G_GNUC_CONST;

GeditFifo *gedit_fifo_new (GFile *file);
GFile *gedit_fifo_get_file (GeditFifo *fifo);

void gedit_fifo_open_read_async (GeditFifo           *fifo,
                                 gint                 io_priority,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data);

void gedit_fifo_open_write_async (GeditFifo           *fifo,
                                  gint                 io_priority,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data);

GOutputStream *gedit_fifo_open_write_finish (GeditFifo     *fifo,
                                             GAsyncResult  *result,
                                             GError       **error);

GInputStream *gedit_fifo_open_read_finish (GeditFifo     *fifo,
                                           GAsyncResult  *result,
                                           GError       **error);

G_END_DECLS

#endif /* __GEDIT_FIFO_H__ */
