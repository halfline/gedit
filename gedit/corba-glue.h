typedef struct {
   POA_Desktop_Editor servant;
   PortableServer_POA poa;

   CORBA_char *attr_path;
   CORBA_unsigned_long attr_position;
   Desktop_Range attr_selection;

        gE_document *document;
        glong search_pos;
} impl_POA_Desktop_Editor;

CORBA_Environment *global_ev;


Desktop_EditorFactory impl_Desktop_EditorFactory__create(
		PortableServer_POA poa,
		CORBA_Environment *ev);

void impl_Desktop_Editor__destroy(impl_POA_Desktop_Editor *servant,
		CORBA_Environment *ev);
