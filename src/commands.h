/* vi:set ts=8 sts=0 sw=8:
 *
 * gEdit
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef MAX_RECENT
#define MAX_RECENT 4
#endif

/* File Ops */
extern void file_quit_cb(GtkWidget *widget, gpointer cbdata);
extern void file_new_cb(GtkWidget *widget, gpointer cbdata);
/* extern void window_new_cb(GtkWidget *widget, gpointer cbdata); -- FOO! */
extern void file_open_cb(GtkWidget *widget, gpointer cbdata);
extern void file_open_in_new_win_cb(GtkWidget *widget, gpointer data);
extern gint file_save_cb(GtkWidget *widget, gpointer cbdata);
extern void file_save_as_cb(GtkWidget *widget, gpointer cbdata);
extern void file_close_cb(GtkWidget *widget, gpointer cbdata);
/*FIXME extern void file_close_all_cb(GtkWidget *widget, gpointer cbdata);
extern void window_close_cb(GtkWidget *widget, gpointer cbdata);*/


/* Edit functions */
extern void edit_cut_cb(GtkWidget *widget, gpointer cbdata);
extern void edit_copy_cb(GtkWidget *widget, gpointer cbdata);
extern void edit_paste_cb(GtkWidget *widget, gpointer cbdata);
extern void edit_selall_cb(GtkWidget *widget, gpointer cbdata);

extern void doc_changed_cb(GtkWidget *widget, gpointer);

/* Tab positioning */
extern void tab_top_cb(GtkWidget *widget, gpointer cbwindow);
extern void tab_bot_cb(GtkWidget *widget, gpointer cbwindow);
extern void tab_lef_cb(GtkWidget *widget, gpointer cbwindow);
extern void tab_rgt_cb(GtkWidget *widget, gpointer cbwindow);
extern void tab_toggle_cb(GtkWidget *widget, gpointer cbwindow);

/* Auto indent */
extern gint auto_indent_cb(GtkWidget *text, char *insertion_text, int length, int *pos);
extern void auto_indent_toggle_cb(GtkWidget *w, gpointer cbdata);
extern gint gE_event_button_press(GtkWidget *w, GdkEventButton *, gE_window *);

/* DND */
extern void filenames_dropped (GtkWidget * widget,
	GdkDragContext   *context,
	gint              x,
	gint              y,
	GtkSelectionData *selection_data,
	guint             info,
	guint             time);

/* Recent documents */
extern void recent_add (char *filename);
extern void recent_update (gE_window *window);

/* Insert/delete text callbacks for undo/redo and split screening */

extern void doc_insert_text_cb(GtkWidget *editable,
	char *insertion_text, int length, int *pos, gE_document *doc);
extern void doc_delete_text_cb (GtkWidget *editable,
	int start_pos, int end_pos, gE_document *doc);

extern void options_toggle_split_screen_cb (GtkWidget *widget, gpointer data);
extern void options_toggle_status_bar_cb (GtkWidget *widget, gpointer data);
extern void options_toggle_word_wrap_cb (GtkWidget *widget, gpointer data);
extern void options_toggle_line_wrap_cb (GtkWidget *widget, gpointer data);
extern void options_toggle_read_only_cb (GtkWidget *widget, gpointer data);

/* Functions needed to be made external for the plugins api */
extern void popup_close_verify (gE_document *doc, gE_data *data);
extern void close_doc_execute(gE_document *opt_doc, gpointer cbdata);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __COMMANDS_H__ */
