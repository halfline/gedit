#include <gnome.h>
#include <libgnorba/gnorba.h>
#include "desktop-editor.h"

void
corba_exception (CORBA_Environment* ev)
{
        switch (ev->_major) {
        case CORBA_SYSTEM_EXCEPTION:
                g_log ("GEdit CORBA", G_LOG_LEVEL_DEBUG,
                       "CORBA system exception %s.\n",
                       CORBA_exception_id (ev));
                break;
        case CORBA_USER_EXCEPTION:
                g_log ("GEdit CORBA", G_LOG_LEVEL_DEBUG,
                       "CORBA user exception: %s.\n",
                       CORBA_exception_id (ev));
                break;
        default:
                break;
        }
}

int
main (int argc, char **argv)
{
	Desktop_EditorFactory factory;
	Desktop_Editor editor, anothereditor;
	CORBA_Environment ev;
	CORBA_Object name_service;
	CORBA_ORB orb;
	CORBA_char *file;
	Desktop_Range range;
	CORBA_long pos;
	GNOME_stringlist *strings;
	gint i;

	CORBA_exception_init (&ev);
	orb = gnome_CORBA_init("test-gedit", "1", &argc, argv, 0, &ev);
	
	factory = (Desktop_EditorFactory) goad_server_activate_with_repo_id (
			NULL,
			"IDL:Desktop/EditorFactory:1.0",
			0,
			NULL);
/*	editor = Desktop_EditorFactory_open(factory,	
			"/home/martijn/binding.txt", &ev);
	corba_exception (&ev);*/

	if (!factory) return -1;
	anothereditor = Desktop_EditorFactory_open_existing(factory,
			"./editor.c", &ev);
	corba_exception (&ev);

	getchar();

	Desktop_Editor_replace (anothereditor, "include", "changedthis!",
			Desktop_SEEK_END, 0, &ev);
	corba_exception (&ev);

	getchar();

	range.begin = 1;
	range.end = 100;
	strings = Desktop_Editor_get_text (anothereditor, &range, &ev);
	corba_exception (&ev);

	for (i = 0; i < strings->_length; i++) {
		printf ("--> %s\n", strings->_buffer[i]);
	}
/*	CORBA_free (strings);*/

	getchar();
	Desktop_Editor_close(anothereditor, &ev);
	corba_exception (&ev);

	return 0;
}
