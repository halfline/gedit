#include <gnome.h>
#include <argz.h>
#include "main.h"
#include "gE_document.h"
#include "gE_files.h"
#include "gE_mdi.h"
#include "commands.h"
#include "desktop-editor.h"
#include "corba-glue.h"
#include "search.h"


static Desktop_Editor
impl_Desktop_Editor__create(PortableServer_POA  poa,
                            CORBA_Environment  *ev,
                            gE_document        *doc);
/*** App-specific servant structures ***/

typedef struct {
   POA_Desktop_EditorFactory servant;
   PortableServer_POA poa;
} impl_POA_Desktop_EditorFactory;

/*** Implementation stub prototypes ***/

static void impl_Desktop_EditorFactory__destroy(impl_POA_Desktop_EditorFactory * servant,
						CORBA_Environment * ev);
static CORBA_boolean
 impl_Desktop_EditorFactory_supports(impl_POA_Desktop_EditorFactory * servant,
				     CORBA_char * obj_goad_id,
				     CORBA_Environment * ev);
static CORBA_Object
 impl_Desktop_EditorFactory_create_object(impl_POA_Desktop_EditorFactory * servant,
					  CORBA_char * goad_id,
					  GNOME_stringlist * params,
					  CORBA_Environment * ev);
static CORBA_Object
 impl_Desktop_EditorFactory_open(impl_POA_Desktop_EditorFactory * servant,
				 CORBA_char * path,
				 CORBA_Environment * ev);
static CORBA_Object
 impl_Desktop_EditorFactory_open_existing(impl_POA_Desktop_EditorFactory * servant,
					  CORBA_char * path,
					  CORBA_Environment * ev);

static void
 impl_Desktop_Editor_insert(impl_POA_Desktop_Editor * servant,
			    CORBA_unsigned_long pos,
			    CORBA_char * new,
			    CORBA_Environment * ev);

static void
 impl_Desktop_Editor_delete(impl_POA_Desktop_Editor * servant,
			    CORBA_unsigned_long pos,
			    CORBA_unsigned_long charcount,
			    CORBA_Environment * ev);

static CORBA_short
 impl_Desktop_Editor_replace(impl_POA_Desktop_Editor * servant,
			     CORBA_char * regexp,
			     CORBA_char * newstring,
			     Desktop_SEEKFROM whence,
			     Desktop_SearchOption flags,
			     CORBA_Environment * ev);

static void
 impl_Desktop_Editor_ref(impl_POA_Desktop_Editor * servant,
			 CORBA_Environment * ev);
static void
 impl_Desktop_Editor_unref(impl_POA_Desktop_Editor * servant,
			   CORBA_Environment * ev);
static CORBA_Object
 impl_Desktop_Editor_query_interface(impl_POA_Desktop_Editor * servant,
				     CORBA_char * repoid,
				     CORBA_Environment * ev);
static CORBA_char *
 impl_Desktop_Editor__get_path(impl_POA_Desktop_Editor * servant,
			       CORBA_Environment * ev);
static CORBA_unsigned_long
 impl_Desktop_Editor__get_position(impl_POA_Desktop_Editor * servant,
				   CORBA_Environment * ev);
static Desktop_Range
 impl_Desktop_Editor__get_selection(impl_POA_Desktop_Editor * servant,
				    CORBA_Environment * ev);
static void
 impl_Desktop_Editor__set_selection(impl_POA_Desktop_Editor * servant,
				    Desktop_Range * value,
				    CORBA_Environment * ev);
static void
 impl_Desktop_Editor_scroll_pos(impl_POA_Desktop_Editor * servant,
				CORBA_long offset,
				Desktop_SEEKFROM whence,
				CORBA_Environment * ev);
static void
 impl_Desktop_Editor_scroll_line(impl_POA_Desktop_Editor * servant,
				 CORBA_unsigned_long line,
				 CORBA_Environment * ev);
static CORBA_long
 impl_Desktop_Editor_search(impl_POA_Desktop_Editor * servant,
			    CORBA_char * regexp,
			    Desktop_SEEKFROM whence,
			    Desktop_SearchOption flags,
			    CORBA_Environment * ev);
static GNOME_stringlist *
 impl_Desktop_Editor_get_text(impl_POA_Desktop_Editor * servant,
			      Desktop_Range * what,
			      CORBA_Environment * ev);
static void
 impl_Desktop_Editor_save(impl_POA_Desktop_Editor * servant,
			  CORBA_Environment * ev);
static void
 impl_Desktop_Editor_save_as(impl_POA_Desktop_Editor * servant,
			     CORBA_char * path,
			     CORBA_Environment * ev);
static void
 impl_Desktop_Editor_close(impl_POA_Desktop_Editor * servant,
			   CORBA_Environment * ev);

/*** epv structures ***/

static PortableServer_ServantBase__epv impl_Desktop_EditorFactory_base_epv =
{
   NULL,			/* _private data */
   NULL,			/* finalize routine */
   NULL,			/* default_POA routine */
};
static POA_Desktop_EditorFactory__epv impl_Desktop_EditorFactory_epv =
{
   NULL,			/* _private */
};
static POA_GNOME_GenericFactory__epv impl_Desktop_EditorFactory_GNOME_GenericFactory_epv =
{
   NULL,			/* _private */
   (gpointer) & impl_Desktop_EditorFactory_supports,
   (gpointer) & impl_Desktop_EditorFactory_create_object,
};
static POA_Desktop_TextViewerFactory__epv impl_Desktop_EditorFactory_Desktop_TextViewerFactory_epv =
{
   NULL,			/* _private */
   (gpointer) & impl_Desktop_EditorFactory_open,
   (gpointer) & impl_Desktop_EditorFactory_open_existing,
};
static PortableServer_ServantBase__epv impl_Desktop_Editor_base_epv =
{
   NULL,			/* _private data */
   NULL,			/* finalize routine */
   NULL,			/* default_POA routine */
};
static POA_Desktop_Editor__epv impl_Desktop_Editor_epv =
{
   NULL,			/* _private */
   (gpointer) & impl_Desktop_Editor_insert,

   (gpointer) & impl_Desktop_Editor_delete,

   (gpointer) & impl_Desktop_Editor_replace,

};
static POA_GNOME_object__epv impl_Desktop_Editor_GNOME_object_epv =
{
   NULL,			/* _private */
   (gpointer) & impl_Desktop_Editor_ref,
   (gpointer) & impl_Desktop_Editor_unref,
   (gpointer) & impl_Desktop_Editor_query_interface,
};
static POA_Desktop_TextViewer__epv impl_Desktop_Editor_Desktop_TextViewer_epv =
{
   NULL,			/* _private */
   (gpointer) & impl_Desktop_Editor__get_path,
   (gpointer) & impl_Desktop_Editor__get_position,
   (gpointer) & impl_Desktop_Editor__get_selection,
   (gpointer) & impl_Desktop_Editor__set_selection,
   (gpointer) & impl_Desktop_Editor_scroll_pos,
   (gpointer) & impl_Desktop_Editor_scroll_line,
   (gpointer) & impl_Desktop_Editor_search,
   (gpointer) & impl_Desktop_Editor_get_text,
   (gpointer) & impl_Desktop_Editor_save,
   (gpointer) & impl_Desktop_Editor_save_as,
   (gpointer) & impl_Desktop_Editor_close,
};

/*** vepv structures ***/

static POA_Desktop_EditorFactory__vepv impl_Desktop_EditorFactory_vepv =
{
   &impl_Desktop_EditorFactory_base_epv,
   &impl_Desktop_EditorFactory_GNOME_GenericFactory_epv,
   &impl_Desktop_EditorFactory_Desktop_TextViewerFactory_epv,
   &impl_Desktop_EditorFactory_epv,
};
static POA_Desktop_Editor__vepv impl_Desktop_Editor_vepv =
{
   &impl_Desktop_Editor_base_epv,
   &impl_Desktop_Editor_GNOME_object_epv,
   &impl_Desktop_Editor_Desktop_TextViewer_epv,
   &impl_Desktop_Editor_epv,
};

/*** Stub implementations ***/

Desktop_EditorFactory 
impl_Desktop_EditorFactory__create(PortableServer_POA poa, CORBA_Environment * ev)
{
   Desktop_EditorFactory retval;
   impl_POA_Desktop_EditorFactory *newservant;
   PortableServer_ObjectId *objid;

   newservant = g_new0(impl_POA_Desktop_EditorFactory, 1);
   newservant->servant.vepv = &impl_Desktop_EditorFactory_vepv;
   newservant->poa = poa;
   POA_Desktop_EditorFactory__init((PortableServer_Servant) newservant, ev);
   objid = PortableServer_POA_activate_object(poa, newservant, ev);
   CORBA_free(objid);
   retval = PortableServer_POA_servant_to_reference(poa, newservant, ev);

   return retval;
}

static void
impl_Desktop_EditorFactory__destroy(impl_POA_Desktop_EditorFactory * servant, CORBA_Environment * ev)
{
   PortableServer_ObjectId *objid;

   objid = PortableServer_POA_servant_to_id(servant->poa, servant, ev);
   PortableServer_POA_deactivate_object(servant->poa, objid, ev);
   CORBA_free(objid);

   POA_Desktop_EditorFactory__fini((PortableServer_Servant) servant, ev);
   g_free(servant);
}

static CORBA_boolean
impl_Desktop_EditorFactory_supports(impl_POA_Desktop_EditorFactory * servant,
				    CORBA_char * obj_goad_id,
				    CORBA_Environment * ev)
{
   CORBA_boolean retval;

   return retval;
}

static CORBA_Object
impl_Desktop_EditorFactory_create_object(impl_POA_Desktop_EditorFactory * servant,
					 CORBA_char * goad_id,
					 GNOME_stringlist * params,
					 CORBA_Environment * ev)
{
   CORBA_Object retval;
   gE_document *doc;

   if (mdi->windows == NULL) {
      gnome_mdi_open_toplevel (mdi);
   }    
   doc = gE_document_new();
   retval = impl_Desktop_Editor__create(servant->poa, ev, doc);
   return retval;
}

static CORBA_Object
impl_Desktop_EditorFactory_open(impl_POA_Desktop_EditorFactory * servant,
				CORBA_char * path,
				CORBA_Environment * ev)
{
   CORBA_Object retval;
   gE_document *doc;

   if (mdi->windows == NULL) {
      gnome_mdi_open_toplevel (mdi);
   }
   doc = gE_document_new_with_file (g_strdup(path));
   retval = impl_Desktop_Editor__create(servant->poa, ev, doc);
   return retval;
}

static CORBA_Object
impl_Desktop_EditorFactory_open_existing(impl_POA_Desktop_EditorFactory * servant,
					 CORBA_char * path,
					 CORBA_Environment * ev)
{
   CORBA_Object retval;
   gE_document *doc = NULL;
   GList *list;

   if (mdi->windows == NULL) {
      gnome_mdi_open_toplevel (mdi);
   }
   list = g_list_first(gE_documents);
   while (list) {
      doc = list->data;
      if (doc->filename && !strcmp(doc->filename, path)) {
         break;
      }
      doc = NULL;
      list = list->next;
   }
   if (!doc) {
      doc = gE_document_new_with_file (g_strdup(path));
   }
   retval = impl_Desktop_Editor__create(servant->poa, ev, doc);
   return retval;
}

static Desktop_Editor 
impl_Desktop_Editor__create(PortableServer_POA poa, CORBA_Environment * ev,
	gE_document *doc)
{
   Desktop_Editor retval;
	Desktop_Editor saved;
   impl_POA_Desktop_Editor *newservant;
   PortableServer_ObjectId *objid;

   newservant = g_new0(impl_POA_Desktop_Editor, 1);
   newservant->servant.vepv = &impl_Desktop_Editor_vepv;
   newservant->poa = poa;
	newservant->document = doc;
	newservant->search_pos = -1;
   POA_Desktop_Editor__init((PortableServer_Servant) newservant, ev);
   objid = PortableServer_POA_activate_object(poa, newservant, ev);
   CORBA_free(objid);
   saved = PortableServer_POA_servant_to_reference(poa, newservant, ev);
	doc->ed_obj_list = g_list_append (doc->ed_obj_list, newservant);
	retval = CORBA_Object_duplicate (saved, ev);

   return retval;
}

void
impl_Desktop_Editor__destroy(impl_POA_Desktop_Editor * servant, CORBA_Environment * ev)
{
   PortableServer_ObjectId *objid;

   objid = PortableServer_POA_servant_to_id(servant->poa, servant, ev);
   PortableServer_POA_deactivate_object(servant->poa, objid, ev);
   CORBA_free(objid);

   POA_Desktop_Editor__fini((PortableServer_Servant) servant, ev);
   g_free(servant);
}

static void
impl_Desktop_Editor_insert(impl_POA_Desktop_Editor * servant,
			   CORBA_unsigned_long pos,
			   CORBA_char * new,
			   CORBA_Environment * ev)
{
   GtkEditable *edit;

   edit = GTK_EDITABLE (servant->document->text);
   gtk_editable_insert_text (edit, new, strlen (new), &pos);
}

static void
impl_Desktop_Editor_delete(impl_POA_Desktop_Editor * servant,
			   CORBA_unsigned_long pos,
			   CORBA_unsigned_long charcount,
			   CORBA_Environment * ev)
{
   GtkEditable *edit;

   edit = GTK_EDITABLE (servant->document->text);
   gtk_editable_delete_text (edit, pos, pos + charcount);
}

static CORBA_short
impl_Desktop_Editor_replace(impl_POA_Desktop_Editor * servant,
			    CORBA_char * regexp,
			    CORBA_char * newstring,
			    Desktop_SEEKFROM whence,
			    Desktop_SearchOption flags,
			    CORBA_Environment * ev)
{
   CORBA_short retval = 0;
   GtkEditable *edit;
   gulong options = 0;
   gint pos = -1;

   if (flags & Desktop_REG_REGEX) {
      /* exception */
      return -1;
   }
   if (flags & Desktop_REG_ICASE) {
      options |= SEARCH_NOCASE;
   }

   edit = GTK_EDITABLE (servant->document->text);
   gtk_text_freeze (GTK_TEXT (edit));
   switch (whence) {
      case Desktop_SEEK_START:
         pos = 0;
      break;
      case Desktop_SEEK_CURRENT:
         if (servant->search_pos == -1) {
            pos = gtk_editable_get_position (edit);
         } else {
            pos = servant->search_pos;
         }
      break;
      case Desktop_SEEK_END:
         pos = gtk_text_get_length (GTK_TEXT (edit));
         options |= SEARCH_BACKWARDS;
      break;
   }
   do {
      pos = gE_search_search (servant->document, regexp,
            pos, options);
      if (pos == -1) break;
      gE_search_replace (servant->document, pos, strlen(regexp), newstring);
      if (whence != Desktop_SEEK_END) {
         pos += strlen (newstring);
      }
		retval++;

   /* FIXME: need new flag here */
   } while ((flags & Desktop_REG_NOSUB));

   /* remember pos for repeated replace if applicable */
   servant->search_pos = pos;
   gtk_text_thaw (GTK_TEXT (edit));
   return retval;
}

static void
impl_Desktop_Editor_ref(impl_POA_Desktop_Editor * servant,
			CORBA_Environment * ev)
{
}

static void
impl_Desktop_Editor_unref(impl_POA_Desktop_Editor * servant,
			  CORBA_Environment * ev)
{
}

static CORBA_Object
impl_Desktop_Editor_query_interface(impl_POA_Desktop_Editor * servant,
				    CORBA_char * repoid,
				    CORBA_Environment * ev)
{
   CORBA_Object retval;

   return retval;
}

static CORBA_char *
impl_Desktop_Editor__get_path(impl_POA_Desktop_Editor * servant,
			      CORBA_Environment * ev)
{
   CORBA_char *retval;
   retval = CORBA_string_dup (servant->document->filename);
   g_warning ("%s\nstring sent: %s", servant->document->filename, retval);
   return retval;
}

static CORBA_unsigned_long
impl_Desktop_Editor__get_position(impl_POA_Desktop_Editor * servant,
				  CORBA_Environment * ev)
{
   CORBA_unsigned_long retval;
   GtkEditable *edit;

   edit = GTK_EDITABLE (servant->document->text);
   retval = gtk_editable_get_position (edit);
   return retval;
}

static Desktop_Range
impl_Desktop_Editor__get_selection(impl_POA_Desktop_Editor * servant,
				   CORBA_Environment * ev)
{
   Desktop_Range retval;
   GtkEditable *edit;

   edit = GTK_EDITABLE (servant->document->text);
   retval.begin = edit->selection_start_pos;
   retval.end = edit->selection_end_pos;
   return retval;
}

static void
impl_Desktop_Editor__set_selection(impl_POA_Desktop_Editor * servant,
				   Desktop_Range * value,
				   CORBA_Environment * ev)
{
   GtkEditable *edit;

   edit = GTK_EDITABLE (servant->document->text);
   gtk_editable_select_region (edit, value->begin, value->end);
}

static void
impl_Desktop_Editor_scroll_pos(impl_POA_Desktop_Editor * servant,
			       CORBA_long offset,
			       Desktop_SEEKFROM whence,
			       CORBA_Environment * ev)
{
   GtkEditable *edit;
   gint pos = -1, line, lc;

   edit = GTK_EDITABLE (servant->document->text);
   gtk_text_freeze (GTK_TEXT (edit));
   switch (whence) {
      case Desktop_SEEK_START:
         pos = offset;
      break;
      case Desktop_SEEK_CURRENT:
         pos = gtk_editable_get_position (edit) + offset;
      break;
      case Desktop_SEEK_END:
         pos = gtk_text_get_length (GTK_TEXT (edit)) + offset;
      break;
   }
   line = pos_to_line (servant->document, pos, &lc);
   seek_to_line (servant->document, line, lc);
   gtk_text_set_point (GTK_TEXT (edit), pos);
   gtk_text_thaw (GTK_TEXT (edit));
}

static void
impl_Desktop_Editor_scroll_line(impl_POA_Desktop_Editor * servant,
				CORBA_unsigned_long line,
				CORBA_Environment * ev)
{
   seek_to_line (servant->document, line, -1);
}

static CORBA_long
impl_Desktop_Editor_search(impl_POA_Desktop_Editor * servant,
			   CORBA_char * regexp,
			   Desktop_SEEKFROM whence,
			   Desktop_SearchOption flags,
			   CORBA_Environment * ev)
{
   CORBA_long retval;
   GtkEditable *edit;
   gulong options = 0;
   glong pos = -1;

   if (flags & Desktop_REG_REGEX) {
      /* exception */
   }
   if (flags & Desktop_REG_ICASE) {
      options |= SEARCH_NOCASE;
   }

   edit = GTK_EDITABLE (servant->document->text);
   gtk_text_freeze (GTK_TEXT (edit));
   switch (whence) {
      case Desktop_SEEK_START:
         pos = 0;
      break;
      case Desktop_SEEK_CURRENT:
         if (servant->search_pos == -1) {
            pos = gtk_editable_get_position (edit);
         } else {
            pos = servant->search_pos;
         }
      break;
      case Desktop_SEEK_END:
         pos = gtk_text_get_length (GTK_TEXT (edit));
         options |= SEARCH_BACKWARDS;
      break;
   }
   retval = gE_search_search (servant->document, regexp, pos, options);
   /* remember pos for repeated search if applicable */
   if (retval == -1) {
      servant->search_pos = -1;
   } else if (whence == Desktop_SEEK_END) {
      servant->search_pos = retval + strlen (regexp);
   }

   gtk_text_thaw (GTK_TEXT (edit));
   return retval;
}

static GNOME_stringlist *
impl_Desktop_Editor_get_text(impl_POA_Desktop_Editor * servant,
			     Desktop_Range * what,
			     CORBA_Environment * ev)
{
   GNOME_stringlist *retval;
	gchar *line, **lines;
	gchar *text;
	CORBA_unsigned_long size;

	text = gtk_editable_get_chars (GTK_EDITABLE (servant->document->text),
		what->begin, what->end);
	line = g_strdup (text);
	lines = g_new0 (gchar *, 1);
	size = 1;
	lines[0] = line;
	while (*line != '\0') {
		if (*line == '\n') {
			*line = '\0';
			lines = g_renew (gchar *, lines, size + 1);
			lines[size] = line + 1;
			size++;
		}
		line++;
	}

	retval = GNOME_stringlist__alloc();
	retval->_maximum = size;
	retval->_length = size;
	retval->_buffer = lines;
	retval->_release = FALSE;

   return retval;
}

static void
impl_Desktop_Editor_save(impl_POA_Desktop_Editor * servant,
			 CORBA_Environment * ev)
{
   gE_file_save (servant->document, servant->document->filename);
}

static void
impl_Desktop_Editor_save_as(impl_POA_Desktop_Editor * servant,
			    CORBA_char * path,
			    CORBA_Environment * ev)
{
   gE_file_save (servant->document, path);
}

static void
impl_Desktop_Editor_close(impl_POA_Desktop_Editor * servant,
			  CORBA_Environment * ev)
{
	servant->document->changed = FALSE;
   gnome_mdi_remove_child (mdi, GNOME_MDI_CHILD (servant->document), FALSE);

}
