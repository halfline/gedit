/* 
 * gedit-window.c
 * This file is part of gedit
 *
 * Copyright (C) 2005 - Paolo Maggi 
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
 
/*
 * Modified by the gedit Team, 2005. See the AUTHORS file for a 
 * list of people on the gedit Team.  
 * See the ChangeLog files for a list of changes. 
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <time.h>
#include <sys/types.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gtksourceview/gtksourcelanguage.h>
#include <gtksourceview/gtksourcelanguagesmanager.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "gedit-ui.h"
#include "gedit-window.h"
#include "gedit-window-private.h"
#include "gedit-app.h"
#include "gedit-statusbar.h"
#include "gedit-utils.h"
#include "gedit-commands.h"
#include "gedit-debug.h"
#include "gedit-languages-manager.h"
#include "gedit-prefs-manager-app.h"
#include "gedit-panel.h"
#include "gedit-recent.h"
#include "gedit-plugins-engine.h"

#include "recent-files/egg-recent-model.h"
#include "recent-files/egg-recent-view.h"
#include "recent-files/egg-recent-view-gtk.h"
#include "recent-files/egg-recent-view-uimanager.h"

#define GEDIT_WINDOW_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object),\
					 GEDIT_TYPE_WINDOW,                    \
					 GeditWindowPrivate))

static void process_tab_added   (GeditWindow *window,
			         GeditTab      *tab);
static void process_tab_removed (GeditWindow *window,
				 GeditTab      *tab);

/* Signals */

enum
{
	TAB_ADDED,
	TAB_REMOVED,
	TABS_REORDERED,
	ACTIVE_TAB_CHANGED,
	ACTIVE_TAB_STATE_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

enum
{
	PROP_0,
	PROP_STATE
};

enum
{
	TARGET_URI_LIST = 100
};

static const GtkTargetEntry drag_types[] =
{
	{ "text/uri-list", 0, TARGET_URI_LIST },
};

G_DEFINE_TYPE(GeditWindow, gedit_window, GTK_TYPE_WINDOW)

static void
gedit_window_get_property (GObject    *object,
			   guint       prop_id,
			   GValue     *value,
			   GParamSpec *pspec)
{
	GeditWindow *window = GEDIT_WINDOW (object);

	switch (prop_id)
	{
		case PROP_STATE:
			g_value_set_int (value,
					 gedit_window_get_state (window));
			break;			
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;			
	}
}

static void
gedit_window_finalize (GObject *object)
{
	GeditWindow *window = GEDIT_WINDOW (object); 

	g_object_unref (window->priv->manager);

	g_object_unref (window->priv->window_group);

	g_free (window->priv->default_path);

	G_OBJECT_CLASS (gedit_window_parent_class)->finalize (object);
}

static void
gedit_window_destroy (GtkObject *object)
{
	GeditWindow *window;

	window = GEDIT_WINDOW (object);

	if (gedit_prefs_manager_window_height_can_set ())
		gedit_prefs_manager_set_window_height (window->priv->height);

	if (gedit_prefs_manager_window_width_can_set ())
		gedit_prefs_manager_set_window_width (window->priv->width);

	if (gedit_prefs_manager_window_state_can_set ())
		gedit_prefs_manager_set_window_state (window->priv->window_state);

	if ((window->priv->side_panel_size > 0) &&
		gedit_prefs_manager_side_panel_size_can_set ())
			gedit_prefs_manager_set_side_panel_size	(
					window->priv->side_panel_size);

	if ((window->priv->bottom_panel_size > 0) && 
		gedit_prefs_manager_bottom_panel_size_can_set ())
			gedit_prefs_manager_set_bottom_panel_size (
					window->priv->bottom_panel_size);

	/* do it here, because recent_view_uim finalization
	 * requires that widgets are still there
	 */
	if (window->priv->recent_view_uim != NULL)
	{
		g_object_unref (window->priv->recent_view_uim);
		window->priv->recent_view_uim = NULL;
	}
	
	gedit_plugins_engine_garbage_collect();

	GTK_OBJECT_CLASS (gedit_window_parent_class)->destroy (object);
}

static gboolean
gedit_window_window_state_event (GtkWidget           *widget,
				 GdkEventWindowState *event)
{
	GeditWindow *window = GEDIT_WINDOW (widget);

	window->priv->window_state = event->new_window_state;

	if (event->changed_mask &
	    (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN))
	{
		gboolean show;

		show = !(event->new_window_state &
			(GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN));

		_gedit_statusbar_set_has_resize_grip (GEDIT_STATUSBAR (window->priv->statusbar),
						      show);
	}

	return FALSE;
}

static gboolean 
gedit_window_configure_event (GtkWidget         *widget,
			      GdkEventConfigure *event)
{
	GeditWindow *window = GEDIT_WINDOW (widget);

	window->priv->width = event->width;
	window->priv->height = event->height;

	return GTK_WIDGET_CLASS (gedit_window_parent_class)->configure_event (widget, event);
}

/*
 * GtkWindow catches keybindings for the menu items _before_ passing them to
 * the focused widget. This is unfortunate and means that pressing ctrl+V
 * in an entry on a panel ends up pasting text in the TextView.
 * Here we force events to be first  passed to the focused widget and then
 * we chain up the default handler... this is the opposite of Gtk default
 * behavior so we need to keep an eye open to see if anything breaks.
 */
static gboolean
gedit_window_key_press_event (GtkWidget   *widget,
			      GdkEventKey *event)
{
	GtkWidget *focused_widget;
	gboolean handled = FALSE;

	focused_widget = gtk_window_get_focus (GTK_WINDOW (widget));

	if (focused_widget)
	{
		handled = gtk_widget_event (focused_widget,
					    (GdkEvent*) event);
	}

	if (handled)
		return TRUE;
	else
		return GTK_WIDGET_CLASS (gedit_window_parent_class)->key_press_event (widget, event);
}

static void
gedit_window_tab_added (GeditWindow *window,
			GeditTab    *tab)
{
	process_tab_added (window, tab);
}

static void
gedit_window_tab_removed (GeditWindow *window,
			  GeditTab    *tab) 
{
	process_tab_removed (window, tab);
}

static void
gedit_window_class_init (GeditWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkObjectClass *gobject_class = GTK_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	klass->tab_added = gedit_window_tab_added;
	klass->tab_removed = gedit_window_tab_removed;

	object_class->finalize = gedit_window_finalize;
	object_class->get_property = gedit_window_get_property;

	gobject_class->destroy = gedit_window_destroy;

	widget_class->window_state_event = gedit_window_window_state_event;
	widget_class->configure_event = gedit_window_configure_event;
	widget_class->key_press_event = gedit_window_key_press_event;

	signals[TAB_ADDED] =
		g_signal_new ("tab_added",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditWindowClass, tab_added),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GEDIT_TYPE_TAB);
	signals[TAB_REMOVED] =
		g_signal_new ("tab_removed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditWindowClass, tab_removed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GEDIT_TYPE_TAB);
	signals[TABS_REORDERED] =
		g_signal_new ("tabs_reordered",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditWindowClass, tabs_reordered),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
	signals[ACTIVE_TAB_CHANGED] =
		g_signal_new ("active_tab_changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditWindowClass, active_tab_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GEDIT_TYPE_TAB);
	signals[ACTIVE_TAB_STATE_CHANGED] =
		g_signal_new ("active_tab_state_changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditWindowClass, active_tab_state_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);			      			      

	g_object_class_install_property (object_class,
					 PROP_STATE,
					 g_param_spec_int ("state",
							   "State",
							   "The window's state",
							   0, /* GEDIT_WINDOW_STATE_NORMAL */
							   G_MAXINT,
							   0, /* GEDIT_WINDOW_STATE_NORMAL */
							   G_PARAM_READABLE));
							   
	g_type_class_add_private (object_class, sizeof(GeditWindowPrivate));
}

static void
menu_item_select_cb (GtkMenuItem *proxy,
		     GeditWindow *window)
{
	GtkAction *action;
	char *message;

	action = g_object_get_data (G_OBJECT (proxy),  "gtk-action");
	g_return_if_fail (action != NULL);

	g_object_get (G_OBJECT (action), "tooltip", &message, NULL);
	if (message)
	{
		gtk_statusbar_push (GTK_STATUSBAR (window->priv->statusbar),
				    window->priv->tip_message_cid, message);
		g_free (message);
	}
}

static void
menu_item_deselect_cb (GtkMenuItem *proxy,
                       GeditWindow *window)
{
	gtk_statusbar_pop (GTK_STATUSBAR (window->priv->statusbar),
			   window->priv->tip_message_cid);
}

static void
connect_proxy_cb (GtkUIManager *manager,
                  GtkAction *action,
                  GtkWidget *proxy,
                  GeditWindow *window)
{
	if (GTK_IS_MENU_ITEM (proxy))
	{
		g_signal_connect (proxy, "select",
				  G_CALLBACK (menu_item_select_cb), window);
		g_signal_connect (proxy, "deselect",
				  G_CALLBACK (menu_item_deselect_cb), window);
	}
}

static void
disconnect_proxy_cb (GtkUIManager *manager,
                     GtkAction *action,
                     GtkWidget *proxy,
                     GeditWindow *window)
{
	if (GTK_IS_MENU_ITEM (proxy))
	{
		g_signal_handlers_disconnect_by_func
			(proxy, G_CALLBACK (menu_item_select_cb), window);
		g_signal_handlers_disconnect_by_func
			(proxy, G_CALLBACK (menu_item_deselect_cb), window);
	}
}

/* Returns TRUE if toolbar is visible */
static gboolean
set_toolbar_style (GeditWindow *window,
		   GeditWindow *origin)
{
	gboolean visible;
	GeditToolbarSetting style;
	GtkAction *action;
	
	if (origin == NULL)
		visible = gedit_prefs_manager_get_toolbar_visible ();
	else
		visible = GTK_WIDGET_VISIBLE (origin->priv->toolbar);
	
	/* Set visibility */
	if (visible)
		gtk_widget_show (window->priv->toolbar);
	else
		gtk_widget_hide (window->priv->toolbar);

	action = gtk_action_group_get_action (window->priv->always_sensitive_action_group,
					      "ViewToolbar");

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)) != visible)
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), visible);

	/* Set style */
	if (origin == NULL)
		style = gedit_prefs_manager_get_toolbar_buttons_style ();
	else
		style = origin->priv->toolbar_style;
	
	window->priv->toolbar_style = style;
		
	switch (style)
	{
		case GEDIT_TOOLBAR_SYSTEM:
			gedit_debug_message (DEBUG_WINDOW, "GEDIT: SYSTEM");
			gtk_toolbar_unset_style (
					GTK_TOOLBAR (window->priv->toolbar));
			break;

		case GEDIT_TOOLBAR_ICONS:
			gedit_debug_message (DEBUG_WINDOW, "GEDIT: ICONS");
			gtk_toolbar_set_style (
					GTK_TOOLBAR (window->priv->toolbar),
					GTK_TOOLBAR_ICONS);
			break;

		case GEDIT_TOOLBAR_ICONS_AND_TEXT:
			gedit_debug_message (DEBUG_WINDOW, "GEDIT: ICONS_AND_TEXT");
			gtk_toolbar_set_style (
					GTK_TOOLBAR (window->priv->toolbar),
					GTK_TOOLBAR_BOTH);			
			break;

		case GEDIT_TOOLBAR_ICONS_BOTH_HORIZ:
			gedit_debug_message (DEBUG_WINDOW, "GEDIT: ICONS_BOTH_HORIZ");
			gtk_toolbar_set_style (
					GTK_TOOLBAR (window->priv->toolbar),
					GTK_TOOLBAR_BOTH_HORIZ);	
			break;       
	}
	
	return visible;
}

static void
set_sensitivity_according_to_tab (GeditWindow *window,
				  GeditTab    *tab)
{
	GeditDocument *doc;
	GeditView     *view;
	GtkAction     *action;
	gboolean       b;
	gboolean       state_normal;
	GeditTabState  state;

	g_return_if_fail (GEDIT_TAB (tab));

	gedit_debug (DEBUG_WINDOW);

	state = gedit_tab_get_state (tab);
	state_normal = (state == GEDIT_TAB_STATE_NORMAL);

	view = gedit_tab_get_view (tab);
	doc = GEDIT_DOCUMENT (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

	action = gtk_action_group_get_action (window->priv->action_group,
					      "FileSave");
	gtk_action_set_sensitive (action,
				  (state_normal ||
				   (state == GEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW)) &&
				  !gedit_document_get_readonly (doc));

	action = gtk_action_group_get_action (window->priv->action_group,
					      "FileSaveAs");
	gtk_action_set_sensitive (action,
				  state_normal ||
				  (state == GEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW));
				  	
	action = gtk_action_group_get_action (window->priv->action_group,
					      "FileRevert");
	gtk_action_set_sensitive (action, 
				  !gedit_document_is_untitled (doc) &&
				  state_normal);

	action = gtk_action_group_get_action (window->priv->action_group,
					      "FilePrintPreview");
	gtk_action_set_sensitive (action,
				  state_normal);

	action = gtk_action_group_get_action (window->priv->action_group,
					      "FilePrint");
	gtk_action_set_sensitive (action,
				  state_normal ||
				  (state == GEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW));
				  
	action = gtk_action_group_get_action (window->priv->action_group,
					      "FileClose");

	gtk_action_set_sensitive (action,
	                          (state != GEDIT_TAB_STATE_CLOSING) &&
				  (state != GEDIT_TAB_STATE_SAVING) &&
				  (state != GEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW) &&
				  (state != GEDIT_TAB_STATE_SAVING_ERROR));

	action = gtk_action_group_get_action (window->priv->action_group,
					      "EditUndo");
	gtk_action_set_sensitive (action, 
				  state_normal &&
				  gtk_source_buffer_can_undo (GTK_SOURCE_BUFFER (doc)));

	action = gtk_action_group_get_action (window->priv->action_group,
					      "EditRedo");
	gtk_action_set_sensitive (action, 
				  state_normal &&
				  gtk_source_buffer_can_redo (GTK_SOURCE_BUFFER (doc)));

	action = gtk_action_group_get_action (window->priv->action_group,
					      "EditCut");
	gtk_action_set_sensitive (action,
				  state_normal &&
				  _gedit_document_get_has_selection (doc));

	action = gtk_action_group_get_action (window->priv->action_group,
					      "EditCopy");
	gtk_action_set_sensitive (action,
				  state_normal &&
				  _gedit_document_get_has_selection (doc));
				  
	action = gtk_action_group_get_action (window->priv->action_group,
					      "EditPaste");
	gtk_action_set_sensitive (action,
				  state_normal);

	action = gtk_action_group_get_action (window->priv->action_group,
					      "EditDelete");
	gtk_action_set_sensitive (action,
				  state_normal &&
				  _gedit_document_get_has_selection (doc));

	action = gtk_action_group_get_action (window->priv->action_group,
					      "SearchFind");
	gtk_action_set_sensitive (action,
				  state_normal);

	action = gtk_action_group_get_action (window->priv->action_group,
					      "SearchReplace");
	gtk_action_set_sensitive (action,
				  state_normal);

	b = gedit_document_get_can_search_again (doc);
	action = gtk_action_group_get_action (window->priv->action_group,
					      "SearchFindNext");
	gtk_action_set_sensitive (action, state_normal && b);

	action = gtk_action_group_get_action (window->priv->action_group,
					      "SearchFindPrevious");
	gtk_action_set_sensitive (action, state_normal && b);

	action = gtk_action_group_get_action (window->priv->action_group,
					      "SearchClearHighlight");
	gtk_action_set_sensitive (action, state_normal && b);


	action = gtk_action_group_get_action (window->priv->action_group,
					      "SearchGoToLine");
	gtk_action_set_sensitive (action, state_normal);
	
	action = gtk_action_group_get_action (window->priv->action_group,
					      "ViewHighlightMode");
	gtk_action_set_sensitive (action, 
				  (state != GEDIT_TAB_STATE_CLOSING) &&
				  gedit_prefs_manager_get_enable_syntax_highlighting ());

	gedit_plugins_engine_update_plugins_ui (window, FALSE);
}

static void
language_toggled (GtkToggleAction *action,
		  GeditWindow     *window)
{
	GeditDocument *doc;
	const GSList *languages;
	const GtkSourceLanguage *lang;
	gint n;

	if (gtk_toggle_action_get_active (action) == FALSE)
		return;

	doc = gedit_window_get_active_document (window);
	if (doc == NULL)
		return;

	n = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (action));

	if (n < 0)
	{
		/* Normal (no highlighting) */
		lang = NULL;
	}
	else
	{
		languages = gedit_languages_manager_get_available_languages_sorted (
						gedit_get_languages_manager ());

		lang = GTK_SOURCE_LANGUAGE (g_slist_nth_data ((GSList *) languages, n));
	}

	gedit_document_set_language (doc, (GtkSourceLanguage *) lang);
}

static gchar *
escape_section_name (gchar *name)
{
	gchar *tmp;
	gchar *ret;
	gssize len;

	len = strlen (name);

	/* escape slashes doesn't change the string lenght */
	tmp = gedit_utils_escape_slashes (name, len);
	ret = g_markup_escape_text (tmp, len);

	g_free (tmp);

	return ret;
}

static void
create_language_menu_item (GtkSourceLanguage *lang,
			   gint               index,
			   guint              ui_id,
			   GeditWindow       *window)
{
	GtkAction *section_action;
	GtkRadioAction *action;
	GtkAction *normal_action;
	GSList *group;
	gchar *section;
	gchar *escaped_section;
	gchar *lang_name;
	gchar *escaped_lang_name;
	gchar *tip;
	gchar *path;

	section = gtk_source_language_get_section (lang);
	escaped_section = escape_section_name (section);

	/* check if the section submenu exists or create it */
	section_action = gtk_action_group_get_action (window->priv->languages_action_group,
						      escaped_section);

	if (section_action == NULL)
	{
		section_action = gtk_action_new (escaped_section,
						 section,
						 NULL,
						 NULL);

		gtk_action_group_add_action (window->priv->languages_action_group,
					     section_action);
		g_object_unref (section_action);

		gtk_ui_manager_add_ui (window->priv->manager,
				       ui_id,
				       "/MenuBar/ViewMenu/ViewHighlightModeMenu/LanguagesMenuPlaceholder",
				       escaped_section, escaped_section,
				       GTK_UI_MANAGER_MENU,
				       FALSE);
	}

	/* now add the language item to the section */
	lang_name = gtk_source_language_get_name (lang);

	escaped_lang_name = g_markup_escape_text (lang_name, -1);

	tip = g_strdup_printf (_("Use %s highlight mode"), lang_name);
	path = g_strdup_printf ("/MenuBar/ViewMenu/ViewHighlightModeMenu/LanguagesMenuPlaceholder/%s",
				escaped_section);

	action = gtk_radio_action_new (escaped_lang_name,
				       lang_name,
				       tip,
				       NULL,
				       index);

	gtk_action_group_add_action (window->priv->languages_action_group,
				     GTK_ACTION (action));
	g_object_unref (action);

	/* add the action to the same radio group of the "Normal" action */
	normal_action = gtk_action_group_get_action (window->priv->languages_action_group,
						     "LangNone");
	group = gtk_radio_action_get_group (GTK_RADIO_ACTION (normal_action));
	gtk_radio_action_set_group (action, group);

	g_signal_connect (action,
			  "activate",
			  G_CALLBACK (language_toggled),
			  window);

	gtk_ui_manager_add_ui (window->priv->manager,
			       ui_id,
			       path,
			       escaped_lang_name, escaped_lang_name,
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);

	g_free (path);
	g_free (tip);
	g_free (lang_name);
	g_free (escaped_lang_name);
	g_free (section);
	g_free (escaped_section);
}

static void
create_languages_menu (GeditWindow *window)
{
	GtkRadioAction *action_none;
	const GSList *languages;
	const GSList *l;
	guint id;
	gint i;

	gedit_debug (DEBUG_WINDOW);

	/* add the "None" item before all the others */
	
	/* Translators: "None" means that no highlight mode is selected in the 
	 * "View->Highlight Mode" submenu and so syntax highlighting is disabled */
	action_none = gtk_radio_action_new ("LangNone", _("None"),
					    _("Disable syntax highlighting"),
					    NULL,
					    -1);

	gtk_action_group_add_action (window->priv->languages_action_group,
				     GTK_ACTION (action_none));
	g_object_unref (action_none);

	g_signal_connect (action_none,
			  "activate",
			  G_CALLBACK (language_toggled),
			  window);

	id = gtk_ui_manager_new_merge_id (window->priv->manager);

	gtk_ui_manager_add_ui (window->priv->manager,
			       id,
			       "/MenuBar/ViewMenu/ViewHighlightModeMenu/LanguagesMenuPlaceholder",
			       "LangNone", "LangNone",
			       GTK_UI_MANAGER_MENUITEM,
			       TRUE);

	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action_none), TRUE);

	/* now add all the known languages */
	languages = gedit_languages_manager_get_available_languages_sorted (
						gedit_get_languages_manager ());

	for (l = languages, i = 0; l != NULL; l = l->next, ++i)
		create_language_menu_item (GTK_SOURCE_LANGUAGE (l->data),
					    i,
					    id,
					    window);
}

static void
update_languages_menu (GeditWindow *window)
{
	GeditDocument *doc;
	GList *actions;
	GList *l;
	GtkAction *action;
	GtkSourceLanguage *lang;
	gchar *lang_name;
	gchar *escaped_lang_name;

	doc = gedit_window_get_active_document (window);
	if (doc == NULL)
		return;

	lang = gedit_document_get_language (doc);
	if (lang != NULL)
		lang_name = gtk_source_language_get_name (lang);
	else
		lang_name = g_strdup ("LangNone");

	escaped_lang_name = g_markup_escape_text (lang_name, -1);

	actions = gtk_action_group_list_actions (window->priv->languages_action_group);

	/* prevent recursion */
	for (l = actions; l != NULL; l = l->next)
	{
		g_signal_handlers_block_by_func (GTK_ACTION (l->data),
						 G_CALLBACK (language_toggled),
						 window);
	}

	action = gtk_action_group_get_action (window->priv->languages_action_group,
					      escaped_lang_name);

	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

	for (l = actions; l != NULL; l = l->next)
	{
		g_signal_handlers_unblock_by_func (GTK_ACTION (l->data),
						   G_CALLBACK (language_toggled),
						   window);
	}

	g_list_free (actions);
	g_free (lang_name);
	g_free (escaped_lang_name);
}

static void
open_recent_gtk (EggRecentViewGtk *view, 
		 EggRecentItem    *item,
		 GeditWindow      *window)
{
	gedit_cmd_file_open_recent (item, window);
}

static void
open_recent_uim (GtkAction   *action, 
		 GeditWindow *window)
{
	EggRecentItem *item;

	item = egg_recent_view_uimanager_get_item (window->priv->recent_view_uim,
						   action);
	g_return_if_fail (item != NULL);

	gedit_cmd_file_open_recent (item, window);
}

#define TIP_MAX_URI_LEN 100

static void
recent_tooltip_func_gtk (GtkTooltips   *tooltips,
			 GtkWidget     *menu,
			 EggRecentItem *item,
			 gpointer       user_data)
{
	gchar *tip;
	gchar *uri_for_display;
	gchar *trunc_uri;

	uri_for_display = gedit_utils_format_uri_for_display (egg_recent_item_peek_uri (item));
	g_return_if_fail (uri_for_display != NULL);

	trunc_uri = gedit_utils_str_middle_truncate (uri_for_display,
						     TIP_MAX_URI_LEN);
	g_free (uri_for_display);

	/* Translators: %s is a URI */
	tip = g_strdup_printf (_("Open '%s'"), trunc_uri);

	g_free (trunc_uri);

	gtk_tooltips_set_tip (tooltips, GTK_WIDGET (menu), tip, NULL);

	g_free (tip);
}

static char *
recent_tooltip_func_uim (EggRecentItem *item,
			 gpointer       user_data)
{
	gchar *tip;
	gchar *uri_for_display;
	gchar *trunc_uri;

	uri_for_display = gedit_utils_format_uri_for_display (egg_recent_item_peek_uri (item));
	g_return_val_if_fail (uri_for_display != NULL, NULL);

	trunc_uri = gedit_utils_str_middle_truncate (uri_for_display,
						     TIP_MAX_URI_LEN);
	g_free (uri_for_display);

	/* Translators: %s is a URI */
	tip = g_strdup_printf (_("Open '%s'"), trunc_uri);

	g_free (trunc_uri);

	return tip;
}

#undef TIP_MAX_URI_LEN

static void
build_recent_tool_menu (GtkMenuToolButton *button,
			GeditWindow       *window)
{
	EggRecentViewGtk *view;
	EggRecentModel *model;

	model = gedit_recent_get_model ();
	view = egg_recent_view_gtk_new (window->priv->toolbar_recent_menu,
					NULL);

	egg_recent_view_gtk_show_icons (view, TRUE);
	egg_recent_view_gtk_show_numbers (view, FALSE);

	/* elipsize if if gets too large */
	egg_recent_view_gtk_set_label_width (view, 50);

	egg_recent_view_gtk_set_tooltip_func (view, recent_tooltip_func_gtk, NULL);

	egg_recent_view_set_model (EGG_RECENT_VIEW (view), model);

	g_signal_connect (view,
			  "activate",
			  G_CALLBACK (open_recent_gtk),
			  window);

	gtk_widget_show (window->priv->toolbar_recent_menu);

	/* this callback must run just once for lazy initialization:
	 * we can now disconnect it
	 */
	g_signal_handlers_disconnect_by_func (button,
					      G_CALLBACK (build_recent_tool_menu),
					      window);
}

static void
set_non_homogeneus (GtkWidget *widget, gpointer data)
{
	gtk_tool_item_set_homogeneous (GTK_TOOL_ITEM (widget), FALSE);
}

static void
toolbar_visibility_changed (GtkWidget   *toolbar,
			    GeditWindow *window)
{
	gboolean visible;
	GtkAction *action;

	visible = GTK_WIDGET_VISIBLE (toolbar);

	if (gedit_prefs_manager_toolbar_visible_can_set ())
		gedit_prefs_manager_set_toolbar_visible (visible);

	action = gtk_action_group_get_action (window->priv->always_sensitive_action_group,
					      "ViewToolbar");

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)) != visible)
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), visible);
}

static void
create_menu_bar_and_toolbar (GeditWindow *window, 
			     GtkWidget   *main_box)
{
	GtkActionGroup *action_group;
	GtkAction *action;
	GtkUIManager *manager;
	GtkWidget *menubar;
	EggRecentModel *recent_model;
	EggRecentViewUIManager *recent_view;
	GtkToolItem *open_button;
	GError *error = NULL;

	gedit_debug (DEBUG_WINDOW);

	manager = gtk_ui_manager_new ();
	window->priv->manager = manager;

	gtk_window_add_accel_group (GTK_WINDOW (window),
				    gtk_ui_manager_get_accel_group (manager));

	action_group = gtk_action_group_new ("GeditWindowAlwaysSensitiveActions");
	gtk_action_group_set_translation_domain (action_group, NULL);
	gtk_action_group_add_actions (action_group,
				      gedit_always_sensitive_menu_entries,
				      G_N_ELEMENTS (gedit_always_sensitive_menu_entries),
				      window);
	gtk_action_group_add_toggle_actions (action_group,
					     gedit_always_sensitive_toggle_menu_entries,
					     G_N_ELEMENTS (gedit_always_sensitive_toggle_menu_entries),
					     window);

	gtk_ui_manager_insert_action_group (manager, action_group, 0);
	g_object_unref (action_group);
	window->priv->always_sensitive_action_group = action_group;

	action_group = gtk_action_group_new ("GeditWindowActions");
	gtk_action_group_set_translation_domain (action_group, NULL);
	gtk_action_group_add_actions (action_group,
				      gedit_menu_entries,
				      G_N_ELEMENTS (gedit_menu_entries),
				      window);
	gtk_action_group_add_toggle_actions (action_group,
					     gedit_toggle_menu_entries,
					     G_N_ELEMENTS (gedit_toggle_menu_entries),
					     window);

	gtk_ui_manager_insert_action_group (manager, action_group, 0);
	g_object_unref (action_group);
	window->priv->action_group = action_group;

	/* set short labels to use in the toolbar */
	action = gtk_action_group_get_action (action_group, "FileSave");
	g_object_set (action, "short_label", _("Save"), NULL);
	action = gtk_action_group_get_action (action_group, "SearchFind");
	g_object_set (action, "short_label", _("Find"), NULL);
	action = gtk_action_group_get_action (action_group, "SearchReplace");
	g_object_set (action, "short_label", _("Replace"), NULL);

	/* set which actions should have priority on the toolbar */
	action = gtk_action_group_get_action (action_group, "FileSave");
	g_object_set (action, "is_important", TRUE, NULL);
	action = gtk_action_group_get_action (action_group, "EditUndo");
	g_object_set (action, "is_important", TRUE, NULL);

	/* now load the UI definition */
	gtk_ui_manager_add_ui_from_file (manager, GEDIT_UI_DIR "gedit-ui.xml", &error);
	if (error != NULL)
	{
		g_warning ("Could not merge gedit-ui.xml: %s", error->message);
		g_error_free (error);
	}

	/* show tooltips in the statusbar */
	g_signal_connect (manager,
			  "connect_proxy",
			  G_CALLBACK (connect_proxy_cb),
			  window);
	g_signal_connect (manager,
			  "disconnect_proxy",
			  G_CALLBACK (disconnect_proxy_cb),
			  window);

	/* recent files menu */
	recent_model = gedit_recent_get_model ();
	recent_view = egg_recent_view_uimanager_new (manager,
						     "/MenuBar/FileMenu/FileRecentsPlaceholder",
						     G_CALLBACK (open_recent_uim),
						     window);
	window->priv->recent_view_uim = recent_view;
	egg_recent_view_uimanager_show_icons (recent_view, FALSE);
	egg_recent_view_uimanager_set_tooltip_func (recent_view,
						    recent_tooltip_func_uim,
						    window);
	egg_recent_view_set_model (EGG_RECENT_VIEW (recent_view), recent_model);

	/* languages menu */
	action_group = gtk_action_group_new ("LanguagesActions");
	gtk_action_group_set_translation_domain (action_group, NULL);
	window->priv->languages_action_group = action_group;
	gtk_ui_manager_insert_action_group (manager, action_group, 0);
	g_object_unref (action_group);
	create_languages_menu (window);

	menubar = gtk_ui_manager_get_widget (manager, "/MenuBar");
	gtk_box_pack_start (GTK_BOX (main_box), 
			    menubar, 
			    FALSE, 
			    FALSE, 
			    0);

	window->priv->toolbar = gtk_ui_manager_get_widget (manager, "/ToolBar");
	gtk_box_pack_start (GTK_BOX (main_box),
			    window->priv->toolbar,
			    FALSE,
			    FALSE,
			    0);

	/* add the custom Open button to the toolbar */
	open_button = gtk_menu_tool_button_new_from_stock (GTK_STOCK_OPEN);

	/* the popup menu is actually built the first time it's showed */
	window->priv->toolbar_recent_menu = gtk_menu_new ();
	gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (open_button),
				       window->priv->toolbar_recent_menu);
	g_signal_connect (open_button,
			  "show-menu",
			  G_CALLBACK (build_recent_tool_menu),
			  window);

	/* not very nice the way we access the tooltops object
	 * but I can't see a better way and I don't want a differen GtkTooltip
	 * just for this tool button.
	 */
	gtk_tool_item_set_tooltip (open_button,
				   GTK_TOOLBAR (window->priv->toolbar)->tooltips,
				   _("Open a file"),
				   NULL);
	gtk_menu_tool_button_set_arrow_tooltip (GTK_MENU_TOOL_BUTTON (open_button),
						GTK_TOOLBAR (window->priv->toolbar)->tooltips,
						_("Open a recently used file"),
						NULL);

	action = gtk_action_group_get_action (window->priv->always_sensitive_action_group,
					      "FileOpen");
	g_object_set (action,
		      "is_important", TRUE,
		      "short_label", _("Open"),
		      NULL);
	gtk_action_connect_proxy (action, GTK_WIDGET (open_button));

	gtk_toolbar_insert (GTK_TOOLBAR (window->priv->toolbar), open_button, 1);
	
	set_toolbar_style (window, NULL);

	gtk_container_foreach (GTK_CONTAINER (window->priv->toolbar),
			       (GtkCallback)set_non_homogeneus,
			       NULL);

	g_signal_connect_after (G_OBJECT (window->priv->toolbar),
				"show",
				G_CALLBACK (toolbar_visibility_changed),
				window);
	g_signal_connect_after (G_OBJECT (window->priv->toolbar),
				"hide",
				G_CALLBACK (toolbar_visibility_changed),
				window);
}

/* Returns TRUE if status bar is visible */
static gboolean
set_statusbar_style (GeditWindow *window,
		     GeditWindow *origin)
{
	GtkAction *action;
	
	gboolean visible;

	if (origin == NULL)
		visible = gedit_prefs_manager_get_statusbar_visible ();
	else
		visible = GTK_WIDGET_VISIBLE (origin->priv->statusbar);

	if (visible)
		gtk_widget_show (window->priv->statusbar);
	else
		gtk_widget_hide (window->priv->statusbar);

	action = gtk_action_group_get_action (window->priv->always_sensitive_action_group,
					      "ViewStatusbar");
					      
	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)) != visible)
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), visible);
		
	return visible;
}

static void
statusbar_visibility_changed (GtkWidget   *statusbar,
			      GeditWindow *window)
{
	gboolean visible;
	GtkAction *action;

	visible = GTK_WIDGET_VISIBLE (statusbar);

	if (gedit_prefs_manager_statusbar_visible_can_set ())
		gedit_prefs_manager_set_statusbar_visible (visible);

	action = gtk_action_group_get_action (window->priv->always_sensitive_action_group,
					      "ViewStatusbar");

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)) != visible)
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), visible);
}

static void
create_statusbar (GeditWindow *window, 
		  GtkWidget   *main_box)
{
	gedit_debug (DEBUG_WINDOW);

	window->priv->statusbar = gedit_statusbar_new ();

	window->priv->generic_message_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR (window->priv->statusbar), "generic_message");
	window->priv->tip_message_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR (window->priv->statusbar), "tip_message");

	gtk_box_pack_end (GTK_BOX (main_box),
			  window->priv->statusbar,
			  FALSE, 
			  TRUE, 
			  0);

	g_signal_connect_after (G_OBJECT (window->priv->statusbar),
				"show",
				G_CALLBACK (statusbar_visibility_changed),
				window);
	g_signal_connect_after (G_OBJECT (window->priv->statusbar),
				"hide",
				G_CALLBACK (statusbar_visibility_changed),
				window);

	set_statusbar_style (window, NULL);
}

static void
update_cursor_position_statusbar (GtkTextBuffer *buffer, 
				  GeditWindow   *window)
{
	gint row, col;
	GtkTextIter iter;
	GtkTextIter start;
	guint tab_size;
	GeditView *view;

	gedit_debug (DEBUG_WINDOW);
  
 	if (buffer != GTK_TEXT_BUFFER (gedit_window_get_active_document (window)))
 		return;
 		
 	view = gedit_window_get_active_view (window);
 	
	gtk_text_buffer_get_iter_at_mark (buffer,
					  &iter,
					  gtk_text_buffer_get_insert (buffer));
	
	row = gtk_text_iter_get_line (&iter);
	
	start = iter;
	gtk_text_iter_set_line_offset (&start, 0);
	col = 0;

	tab_size = gtk_source_view_get_tabs_width (GTK_SOURCE_VIEW (view));

	while (!gtk_text_iter_equal (&start, &iter))
	{
		/* FIXME: Are we Unicode compliant here? */
		if (gtk_text_iter_get_char (&start) == '\t')
					
			col += (tab_size - (col  % tab_size));
		else
			++col;

		gtk_text_iter_forward_char (&start);
	}
	
	gedit_statusbar_set_cursor_position (
				GEDIT_STATUSBAR (window->priv->statusbar),
				row + 1,
				col + 1);
}

static void
update_overwrite_mode_statusbar (GtkTextView *view, 
				 GeditWindow *window)
{
	if (view != GTK_TEXT_VIEW (gedit_window_get_active_view (window)))
		return;
		
	/* Note that we have to use !gtk_text_view_get_overwrite since we
	   are in the in the signal handler of "toggle overwrite" that is
	   G_SIGNAL_RUN_LAST
	*/
	gedit_statusbar_set_overwrite (
			GEDIT_STATUSBAR (window->priv->statusbar),
			!gtk_text_view_get_overwrite (view));
}

#define MAX_TITLE_LENGTH 100

static void 
set_title (GeditWindow *window)
{
	GeditDocument *doc = NULL;
	gchar *name;
	gchar *dirname = NULL;
	gchar *title = NULL;
	gint len;

	if (window->priv->active_tab == NULL)
	{
		gtk_window_set_title (GTK_WINDOW (window), "gedit");
		return;
	}

	doc = gedit_tab_get_document (window->priv->active_tab);
	g_return_if_fail (doc != NULL);

	name = gedit_document_get_short_name_for_display (doc);

	len = g_utf8_strlen (name, -1);

	/* if the name is awfully long, truncate it and be done with it,
	 * otherwise also show the directory (ellipsized if needed)
	 */
	if (len > MAX_TITLE_LENGTH)
	{
		gchar *tmp;

		tmp = gedit_utils_str_middle_truncate (name,
						       MAX_TITLE_LENGTH);
		g_free (name);
		name = tmp;
	}
	else
	{
		gchar *uri;
		gchar *str;

		uri = gedit_document_get_uri_for_display (doc);
		str = gedit_utils_uri_get_dirname (uri);
		g_free (uri);

		if (str != NULL)
		{
			/* use the remaining space for the dir, but use a min of 20 chars
			 * so that we do not end up with a dirname like "(a...b)".
			 * This means that in the worst case when the filename is long 99
			 * we have a title long 99 + 20, but I think it's a rare enough
			 * case to be acceptable. It's justa darn title afterall :)
			 */
			dirname = gedit_utils_str_middle_truncate (str, 
								   MAX (20, MAX_TITLE_LENGTH - len));
			g_free (str);
		}
	}

	if (gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (doc)))
	{
		gchar *tmp_name;
		
		tmp_name = g_strdup_printf ("*%s", name);
		g_free (name);
		
		name = tmp_name;
	}		

	if (gedit_document_get_readonly (doc)) 
	{
		if (dirname != NULL)
			title = g_strdup_printf ("%s [%s] (%s) - gedit", 
						 name, 
						 _("Read Only"), 
						 dirname);
		else
			title = g_strdup_printf ("%s [%s] - gedit", 
						 name, 
						 _("Read Only"));
	} 
	else 
	{
		if (dirname != NULL)
			title = g_strdup_printf ("%s (%s) - gedit", 
						 name, 
						 dirname);
		else
			title = g_strdup_printf ("%s - gedit", 
						 name);
	}

	gtk_window_set_title (GTK_WINDOW (window), title);

	g_free (dirname);
	g_free (name);
	g_free (title);
}

#undef MAX_TITLE_LENGTH

static void
set_sensitivity_according_to_window_state (GeditWindow *window)
{
	GtkAction        *action;
	GeditWindowClass *klass;
	
	// GtkWidget *recent_file_menu;
	
	/* We disable File->Quit/SaveAll/CloseAll while printing to avoid to have two
	   operations (save and print/print preview) that uses the message area at
	   the same time (may be we can remove this limitation in the future) */
	/* We disable File->Quit/CloseAll if state is saving since saving cannot be
	   cancelled (may be we can remove this limitation in the future) */
	action = gtk_action_group_get_action (window->priv->always_sensitive_action_group,
					      "FileNew");
	gtk_action_set_sensitive (action, 
				  !(window->priv->state & GEDIT_WINDOW_STATE_SAVING_SESSION));
				  
	action = gtk_action_group_get_action (window->priv->always_sensitive_action_group,
					      "FileOpen");
	gtk_action_set_sensitive (action, 
				  !(window->priv->state & GEDIT_WINDOW_STATE_SAVING_SESSION));

	action = gtk_action_group_get_action (window->priv->always_sensitive_action_group,
					      "FileOpenURI");
	gtk_action_set_sensitive (action, 
				  !(window->priv->state & GEDIT_WINDOW_STATE_SAVING_SESSION));
		
	/* FIXME		
	recent_file_menu = gtk_ui_manager_get_widget (window->priv->manager,
						      "/MenuBar/FileMenu/FileRecentsPlaceholder");
	gtk_widget_set_sensitive (recent_file_menu, 
				  !(window->priv->state & GEDIT_WINDOW_STATE_SAVING_SESSION));
	*/
			  
	if ((window->priv->state & GEDIT_WINDOW_STATE_SAVING_SESSION) != 0)
	{
		/* TODO: If we really care, Find could be active
		 * when in SAVING_SESSION state */

		if (gtk_action_group_get_sensitive (window->priv->action_group))
			gtk_action_group_set_sensitive (window->priv->action_group,
							FALSE);
	}
	else
	{
		if (!gtk_action_group_get_sensitive (window->priv->action_group))
			gtk_action_group_set_sensitive (window->priv->action_group,
							window->priv->tabs != NULL);
	}
	
	klass = GEDIT_WINDOW_GET_CLASS (window);
	
	/* Chain up */
	if (klass->update_sensitivity)
		klass->update_sensitivity (window);
}

static void
analyze_tab_state (GeditWindow *window,
		   GeditTab    *tab)
{
	GeditTabState ts;
	
	ts = gedit_tab_get_state (tab);
	
	switch (ts)
	{
		case GEDIT_TAB_STATE_LOADING:
		case GEDIT_TAB_STATE_REVERTING:
			window->priv->state |= GEDIT_WINDOW_STATE_LOADING;
			break;
		
		case GEDIT_TAB_STATE_SAVING:
			window->priv->state |= GEDIT_WINDOW_STATE_SAVING;
			break;
			
		case GEDIT_TAB_STATE_PRINTING:
		case GEDIT_TAB_STATE_PRINT_PREVIEWING:
			window->priv->state |= GEDIT_WINDOW_STATE_PRINTING;
			break;
	
		case GEDIT_TAB_STATE_LOADING_ERROR:
		case GEDIT_TAB_STATE_REVERTING_ERROR:
		case GEDIT_TAB_STATE_SAVING_ERROR:
		case GEDIT_TAB_STATE_GENERIC_ERROR:
			window->priv->state |= GEDIT_WINDOW_STATE_ERROR;
			++window->priv->num_tabs_with_error;
		default:
			/* NOP */
			break;		
	}
}

static void
update_window_state (GeditWindow *window)
{
	GeditWindowState old_ws;
	GList *item;
	gint old_num_of_errors;
	
	gedit_debug_message (DEBUG_WINDOW, "Old state: %x", window->priv->state);
	
	old_ws = window->priv->state;
	old_num_of_errors = window->priv->num_tabs_with_error;
	
	window->priv->state = old_ws & GEDIT_WINDOW_STATE_SAVING_SESSION;
	
	window->priv->num_tabs_with_error = 0;

	for (item = window->priv->tabs; item; item = item->next)
		analyze_tab_state (window, GEDIT_TAB (item->data));
		
	gedit_debug_message (DEBUG_WINDOW, "New state: %x", window->priv->state);		
		
	if (old_ws != window->priv->state)
	{
		set_sensitivity_according_to_window_state (window);

		gedit_statusbar_set_window_state (GEDIT_STATUSBAR (window->priv->statusbar),
						  window->priv->state,
						  window->priv->num_tabs_with_error);
						  
		g_object_notify (G_OBJECT (window), "state");
	}
	else if (old_num_of_errors != window->priv->num_tabs_with_error)
	{
		gedit_statusbar_set_window_state (GEDIT_STATUSBAR (window->priv->statusbar),
						  window->priv->state,
						  window->priv->num_tabs_with_error);	
	}
	
}

static void
sync_state (GeditTab *tab, GParamSpec *pspec, GeditWindow *window)
{
	gedit_debug (DEBUG_WINDOW);
	
	update_window_state (window);
	
	if (tab != window->priv->active_tab)
		return;

	set_sensitivity_according_to_tab (window, tab);

	g_signal_emit (G_OBJECT (window), signals[ACTIVE_TAB_STATE_CHANGED], 0);		
}

static void
sync_name (GeditTab *tab, GParamSpec *pspec, GeditWindow *window)
{
	GtkAction *action;
	GeditDocument *doc;

	if (tab != window->priv->active_tab)
		return;

	set_title (window);

	doc = gedit_tab_get_document (tab);
	action = gtk_action_group_get_action (window->priv->action_group,
					      "FileRevert");
	gtk_action_set_sensitive (action,
				  !gedit_document_is_untitled (doc));

	gedit_plugins_engine_update_plugins_ui (window, FALSE);
}

static void
drag_data_received_cb (GtkWidget        *widget,
		       GdkDragContext   *context,
		       gint              x,
		       gint              y,
		       GtkSelectionData *selection_data,
		       guint             info,
		       guint             time,
		       gpointer          data)
{
	GtkWidget *target_window;
	gchar **uris;
	GSList *uri_list = NULL;
	gint i;

	if (info != TARGET_URI_LIST)
		return;

	g_return_if_fail (widget != NULL);

	target_window = gtk_widget_get_toplevel (widget);
	g_return_if_fail (GEDIT_IS_WINDOW (target_window));
	
	if ((GEDIT_WINDOW(target_window)->priv->state & GEDIT_WINDOW_STATE_SAVING_SESSION) != 0)
		return;

	uris = g_uri_list_extract_uris ((gchar *) selection_data->data);

	for (i = 0; uris[i] != NULL; i++)
	{
		gchar *uri;
		
		uri = gedit_utils_make_canonical_uri_from_shell_arg (uris[i]);
		
		/* Silently ignore malformed URI/filename */
		if (uri != NULL)
			uri_list = g_slist_prepend (uri_list, uri);	
	}

	g_strfreev (uris);

	if (uri_list == NULL)
		return;

	uri_list = g_slist_reverse (uri_list);

	gedit_cmd_load_files (GEDIT_WINDOW (target_window),
			      uri_list,
			      NULL);

	g_slist_foreach (uri_list, (GFunc) g_free, NULL);	
	g_slist_free (uri_list);
}

/*
 * Override the gtk_text_view_drag_motion and drag_drop 
 * functions to get URIs
 *
 * If the mime type is text/uri-list, then we will accept
 * the potential drop, or request the data (depending on the
 * function).
 *
 * If the drag context has any other mime type, then pass the
 * information onto the GtkTextView's standard handlers.
 * (widget_class->function_name).
 *
 * See bug #89881 for details
 */

static gboolean
drag_motion_cb (GtkWidget      *widget,
		GdkDragContext *context,
		gint            x,
		gint            y,
		guint           time)
{
	GtkTargetList *tl;
	GtkWidgetClass *widget_class;
	gboolean result;

	tl = gtk_target_list_new (drag_types,
				  G_N_ELEMENTS (drag_types));

	/* If this is a URL, deal with it here, or pass to the text view */
	if (gtk_drag_dest_find_target (widget, context, tl) != GDK_NONE) 
	{
		gdk_drag_status (context, context->suggested_action, time);
		result = TRUE;
	}
	else
	{
		widget_class = GTK_WIDGET_GET_CLASS (widget);
		result = (*widget_class->drag_motion) (widget, context, x, y, time);
	}

	gtk_target_list_unref (tl);

	return result;
}

static gboolean
drag_drop_cb (GtkWidget      *widget,
	      GdkDragContext *context,
	      gint            x,
	      gint            y,
	      guint           time)
{
	GtkTargetList *tl;
	GtkWidgetClass *widget_class;
	gboolean result;
	GdkAtom target;

	tl = gtk_target_list_new (drag_types,
				  G_N_ELEMENTS (drag_types));

	/* If this is a URL, just get the drag data */
	target = gtk_drag_dest_find_target (widget, context, tl);
	if (target != GDK_NONE)
	{
		gtk_drag_get_data (widget, context, target, time);
		result = TRUE;
	}
	else
	{
		widget_class = GTK_WIDGET_GET_CLASS (widget);
		result = (*widget_class->drag_drop) (widget, context, x, y, time);
	}

	gtk_target_list_unref (tl);

	return result;
}

static void
can_search_again (GeditDocument *doc,
		  GParamSpec    *pspec,
		  GeditWindow   *window)
{
	gboolean sensitive;
	GtkAction *action;

	if (doc != gedit_window_get_active_document (window))
		return;

	sensitive = gedit_document_get_can_search_again (doc);

	action = gtk_action_group_get_action (window->priv->action_group,
					      "SearchFindNext");
	gtk_action_set_sensitive (action, sensitive);

	action = gtk_action_group_get_action (window->priv->action_group,
					      "SearchFindPrevious");
	gtk_action_set_sensitive (action, sensitive);

	action = gtk_action_group_get_action (window->priv->action_group,
					      "SearchClearHighlight");
	gtk_action_set_sensitive (action, sensitive);
}

static void
can_undo (GeditDocument *doc,
          gboolean       can,
          GeditWindow   *window)
{
	GtkAction *action;
	
	if (doc != gedit_window_get_active_document (window))
		return;
		
	action = gtk_action_group_get_action (window->priv->action_group,
					     "EditUndo");
	gtk_action_set_sensitive (action, can);
}

static void
can_redo (GeditDocument *doc,
          gboolean       can,
          GeditWindow   *window)
{
	GtkAction *action;

	if (doc != gedit_window_get_active_document (window))
		return;
	
	action = gtk_action_group_get_action (window->priv->action_group,
					     "EditRedo");
	gtk_action_set_sensitive (action, can);
}

static void
selection_changed (GeditDocument *doc,
		   GParamSpec    *pspec,
		   GeditWindow   *window)
{
	GeditTab *tab;
	GtkAction *action;
	GeditTabState state;
	gboolean state_normal;

	gedit_debug (DEBUG_WINDOW);

	if (doc != gedit_window_get_active_document (window))
		return;

	tab = gedit_tab_get_from_document (doc);
	state = gedit_tab_get_state (tab);
	state_normal = (state == GEDIT_TAB_STATE_NORMAL);

	action = gtk_action_group_get_action (window->priv->action_group,
					      "EditCut");
	gtk_action_set_sensitive (action,
				  state_normal &&
				  _gedit_document_get_has_selection (doc));

	action = gtk_action_group_get_action (window->priv->action_group,
					      "EditCopy");
	gtk_action_set_sensitive (action,
				  state_normal &&
				  _gedit_document_get_has_selection (doc));

	action = gtk_action_group_get_action (window->priv->action_group,
					      "EditDelete");
	gtk_action_set_sensitive (action,
				  state_normal &&
				  _gedit_document_get_has_selection (doc));
}

static void
sync_languages_menu (GeditDocument *doc,
		     GParamSpec    *pspec,
		     GeditWindow   *window)
{
	update_languages_menu (window);
}

static void
update_default_path (GeditWindow   *window,
		     GeditDocument *doc)
{
	gchar *uri;

	uri = gedit_document_get_uri (doc);
	// CHECK: what does it happens when loading from stdin? - Paolo
	g_return_if_fail (uri != NULL);

	if (gedit_utils_uri_has_file_scheme (uri))
	{
		gchar *default_path;

		// CHECK: does it work with uri chaining? - Paolo
		default_path = g_path_get_dirname (uri);

		g_return_if_fail (strlen (default_path) >= 5 /* strlen ("file:") */);
		if (strcmp (default_path, "file:") == 0)
		{
			g_free (default_path);
		
			default_path = g_strdup ("file:///");
		}

		g_free (window->priv->default_path);

		window->priv->default_path = default_path;

		gedit_debug_message (DEBUG_WINDOW, 
				     "New default path: %s", default_path);
	}

	g_free (uri);
}

static void
doc_loaded (GeditDocument *doc,
	    const GError  *error,
	    GeditWindow   *window)
{
	if (error != NULL)
	{
		gedit_debug_message (DEBUG_WINDOW, "Error");
		return;
	}
	
	update_default_path (window, doc);
}

static void
doc_saved (GeditDocument *doc,
	   const GError  *error,
	   GeditWindow   *window)
{
	if (error != NULL)
	{
		gedit_debug_message (DEBUG_WINDOW, "Error");
		return;
	}
	
	if (!_gedit_document_is_saving_as (doc))
	{
		gedit_debug_message (DEBUG_WINDOW, "Not saving as");
		return;
	}

	update_default_path (window, doc);
}

static void
process_tab_added (GeditWindow *window,
		    GeditTab      *tab)
{
	GeditView *view;
	GeditDocument *doc;
	GtkTargetList *tl;

	gedit_debug (DEBUG_WINDOW);
	g_return_if_fail ((window->priv->state & GEDIT_WINDOW_STATE_SAVING_SESSION) == 0);
	
	window->priv->tabs = g_list_append(window->priv->tabs, tab);

	/* Set sensitivity */
	if (!gtk_action_group_get_sensitive (window->priv->action_group))
		gtk_action_group_set_sensitive (window->priv->action_group,
						TRUE);

	view = gedit_tab_get_view (tab);
	doc = gedit_tab_get_document (tab);

	/* IMPORTANT: remember to disconnect the signal in process_tab_removed
	 * if a new signal is connected here */

	g_signal_connect (tab, 
			 "notify::name",
			  G_CALLBACK (sync_name), 
			  window);
	g_signal_connect (tab, 
			 "notify::state",
			  G_CALLBACK (sync_state), 
			  window);

	g_signal_connect (doc,
			  "cursor-moved",
			  G_CALLBACK (update_cursor_position_statusbar),
			  window);
	g_signal_connect (doc,
			  "notify::can-search-again",
			  G_CALLBACK (can_search_again),
			  window);
	g_signal_connect (doc,
			  "can-undo",
			  G_CALLBACK (can_undo),
			  window);
	g_signal_connect (doc,
			  "can-redo",
			  G_CALLBACK (can_redo),
			  window);
	g_signal_connect (doc,
			  "notify::has-selection",
			  G_CALLBACK (selection_changed),
			  window);
	g_signal_connect (doc,
			  "notify::language",
			  G_CALLBACK (sync_languages_menu),
			  window);
	g_signal_connect (doc,
			  "loaded",
			  G_CALLBACK (doc_loaded),
			  window);			  			  
	g_signal_connect (doc,
			  "saved",
			  G_CALLBACK (doc_saved),
			  window);
	g_signal_connect (view,
			  "toggle_overwrite",
			  G_CALLBACK (update_overwrite_mode_statusbar),
			  window);

	/* CHECK: it seems to me this does not work when tab are moved between
	   windows */
	/* Drag and drop support */
	tl = gtk_drag_dest_get_target_list (GTK_WIDGET (view));
	g_return_if_fail (tl != NULL);

	gtk_target_list_add_table (tl, drag_types, G_N_ELEMENTS (drag_types));

	g_signal_connect (view,
			  "drag_data_received",
			  G_CALLBACK (drag_data_received_cb), 
			  NULL);

	/* Get signals before the standard text view functions to deal 
	 * with uris for text files.
	 */
	g_signal_connect (view,
			  "drag_motion",
			  G_CALLBACK (drag_motion_cb), 
			  NULL);
	g_signal_connect (view,
			  "drag_drop",
			  G_CALLBACK (drag_drop_cb), 
			  NULL);

	update_window_state (window);
}

static void
process_tab_removed (GeditWindow *window,
		      GeditTab      *tab)
{
	GeditView     *view;
	GeditDocument *doc;
	GtkAction     *action;

	gedit_debug (DEBUG_WINDOW);
	g_return_if_fail ((window->priv->state & GEDIT_WINDOW_STATE_SAVING_SESSION) == 0);

	window->priv->tabs = g_list_remove (window->priv->tabs, tab);
	
	view = gedit_tab_get_view (tab);
	doc = gedit_tab_get_document (tab);
	
	g_signal_handlers_disconnect_by_func (tab,
					      G_CALLBACK (sync_name), 
					      window);
	g_signal_handlers_disconnect_by_func (tab,
					      G_CALLBACK (sync_state), 
					      window);
	g_signal_handlers_disconnect_by_func (doc,
					      G_CALLBACK (update_cursor_position_statusbar), 
					      window);
	g_signal_handlers_disconnect_by_func (doc, 
					      G_CALLBACK (can_search_again),
					      window);
	g_signal_handlers_disconnect_by_func (doc, 
					      G_CALLBACK (can_undo),
					      window);
	g_signal_handlers_disconnect_by_func (doc, 
					      G_CALLBACK (can_redo),
					      window);
	g_signal_handlers_disconnect_by_func (doc,
					      G_CALLBACK (selection_changed),
					      window);
	g_signal_handlers_disconnect_by_func (doc,
					      G_CALLBACK (sync_languages_menu),
					      window);
	g_signal_handlers_disconnect_by_func (doc, 
					      G_CALLBACK (doc_loaded),
					      window);
	g_signal_handlers_disconnect_by_func (doc, 
					      G_CALLBACK (doc_saved),
					      window);					      				      
	g_signal_handlers_disconnect_by_func (view, 
					      G_CALLBACK (update_overwrite_mode_statusbar),
					      window);
	g_signal_handlers_disconnect_by_func (view, 
					      G_CALLBACK (drag_data_received_cb),
					      NULL);
	g_signal_handlers_disconnect_by_func (view, 
					      G_CALLBACK (drag_motion_cb),
					      NULL);
	g_signal_handlers_disconnect_by_func (view, 
					      G_CALLBACK (drag_drop_cb),
					      NULL);

	if (window->priv->tabs == NULL)
	{
		// CHECK: maybe do set_active_tab here
		window->priv->active_tab = NULL;
			       
		set_title (window);
			
		/* Remove line and col info */
		gedit_statusbar_set_cursor_position (
				GEDIT_STATUSBAR (window->priv->statusbar),
				-1,
				-1);
				
		gedit_statusbar_clear_overwrite (
				GEDIT_STATUSBAR (window->priv->statusbar));								

		if (gtk_action_group_get_sensitive (window->priv->action_group))
			gtk_action_group_set_sensitive (window->priv->action_group,
							FALSE);

		action = gtk_action_group_get_action (window->priv->action_group,
						      "ViewHighlightMode");
		gtk_action_set_sensitive (action, FALSE);

		gedit_plugins_engine_update_plugins_ui (window, FALSE);
	}

	update_window_state (window);
	gedit_plugins_engine_garbage_collect();
}

static void
hpaned_restore_position (GtkWidget   *widget,
			 GeditWindow *window)
{
	gint pos;

	pos = MAX (100, gedit_prefs_manager_get_side_panel_size ());
	gtk_paned_set_position (GTK_PANED (window->priv->hpaned), pos);

	/* run this only once */
	g_signal_handlers_disconnect_by_func (widget, hpaned_restore_position, window);
}

static void
vpaned_restore_position (GtkWidget   *widget,
			 GeditWindow *window)
{
	gint pos;

	pos = widget->allocation.height -
	      MAX (50, gedit_prefs_manager_get_bottom_panel_size ());
	gtk_paned_set_position (GTK_PANED (window->priv->vpaned), pos);

	/* run this only once */
	g_signal_handlers_disconnect_by_func (widget, vpaned_restore_position, window);
}

static void
side_panel_visibility_changed (GtkWidget   *side_panel,
			       GeditWindow *window)
{
	gboolean visible;
	GtkAction *action;

	visible = GTK_WIDGET_VISIBLE (side_panel);

	if (gedit_prefs_manager_side_pane_visible_can_set ())
		gedit_prefs_manager_set_side_pane_visible (visible);

	action = gtk_action_group_get_action (window->priv->always_sensitive_action_group,
	                                      "ViewSidePane");

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)) != visible)
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), visible);

	/* focus the document */
	if (!visible && window->priv->active_tab != NULL)
		gtk_widget_grab_focus (GTK_WIDGET (
				gedit_tab_get_view (GEDIT_TAB (window->priv->active_tab))));
}

static void
side_panel_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation,
			  GeditWindow   *window)
{
	window->priv->side_panel_size = allocation->width;
}

static void
create_side_panel (GeditWindow *window)
{
	gedit_debug (DEBUG_WINDOW);

	window->priv->side_panel = gedit_panel_new (GTK_ORIENTATION_VERTICAL);

	gtk_paned_pack1 (GTK_PANED (window->priv->hpaned), 
			 window->priv->side_panel, 
			 FALSE, 
			 FALSE);
	gtk_widget_set_size_request (window->priv->side_panel, 100, -1);  			 

	g_signal_connect (window->priv->side_panel,
			  "size-allocate",
			  G_CALLBACK (side_panel_size_allocate),
			  window);

	g_signal_connect_after (window->priv->side_panel,
				"show",
				G_CALLBACK (side_panel_visibility_changed),
				window);
	g_signal_connect_after (window->priv->side_panel,
				"hide",
				G_CALLBACK (side_panel_visibility_changed),
				window);

	gtk_paned_set_position (GTK_PANED (window->priv->hpaned),
				MAX (100, gedit_prefs_manager_get_side_panel_size ()));
}

static void
bottom_panel_visibility_changed (GeditPanel  *bottom_panel,
				 GeditWindow *window)
{
	gboolean visible;
	GtkAction *action;

	visible = GTK_WIDGET_VISIBLE (bottom_panel);

	if (gedit_prefs_manager_bottom_panel_visible_can_set ())
		gedit_prefs_manager_set_bottom_panel_visible (visible);

	action = gtk_action_group_get_action (window->priv->action_group,
					      "ViewBottomPane");

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)) != visible)
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), visible);

	/* focus the document */
	if (!visible && window->priv->active_tab != NULL)
		gtk_widget_grab_focus (GTK_WIDGET (
				gedit_tab_get_view (GEDIT_TAB (window->priv->active_tab))));
}

static void
bottom_panel_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation,
			    GeditWindow   *window)
{
	window->priv->bottom_panel_size = allocation->height;
}

static void
bottom_panel_item_removed (GeditPanel  *panel,
			   GtkWidget   *item,
			   GeditWindow *window)
{
	if (gedit_panel_get_n_items (panel) == 0)
	{
		GtkAction *action;

		gtk_widget_hide (GTK_WIDGET (panel));

		action = gtk_action_group_get_action (window->priv->action_group,
						      "ViewBottomPane");
		gtk_action_set_sensitive (action, FALSE);
	}
}

static void
bottom_panel_item_added (GeditPanel  *panel,
			 GtkWidget   *item,
			 GeditWindow *window)
{
	/* if it's the first item added, set the menu item
	 * sensitive and if needed show the panel */
	if (gedit_panel_get_n_items (panel) == 1)
	{
		GtkAction *action;
		gboolean show;

		action = gtk_action_group_get_action (window->priv->action_group,
						      "ViewBottomPane");
		gtk_action_set_sensitive (action, TRUE);

		show = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
		if (show)
			gtk_widget_show (GTK_WIDGET (panel));
	}
}

static void
create_bottom_panel (GeditWindow *window) 
{
	gedit_debug (DEBUG_WINDOW);

	window->priv->bottom_panel = gedit_panel_new (GTK_ORIENTATION_HORIZONTAL);

	gtk_paned_pack2 (GTK_PANED (window->priv->vpaned), 
			 window->priv->bottom_panel, 
			 FALSE,
			 FALSE);

	g_signal_connect (window->priv->bottom_panel,
			  "size-allocate",
			  G_CALLBACK (bottom_panel_size_allocate),
			  window);

	g_signal_connect_after (window->priv->bottom_panel,
				"show",
				G_CALLBACK (bottom_panel_visibility_changed),
				window);
	g_signal_connect_after (window->priv->bottom_panel,
				"hide",
				G_CALLBACK (bottom_panel_visibility_changed),
				window);

	gtk_paned_set_position (GTK_PANED (window->priv->vpaned),
				MAX (50, gedit_prefs_manager_get_bottom_panel_size ()));
}

static void
init_panels_visibility (GeditWindow *window)
{
	gedit_debug (DEBUG_WINDOW);

	/* side pane */
	if (gedit_prefs_manager_get_side_pane_visible ())
		gtk_widget_show (window->priv->side_panel);

	/* bottom pane, it can be empty */
	if (gedit_panel_get_n_items (GEDIT_PANEL (window->priv->bottom_panel)) > 0)
	{
		if (gedit_prefs_manager_get_bottom_panel_visible ())
  			gtk_widget_show (window->priv->bottom_panel);
	}
	else
	{
		GtkAction *action;
		action = gtk_action_group_get_action (window->priv->action_group,
						      "ViewBottomPane");
		gtk_action_set_sensitive (action, FALSE);
	}

	/* start track sensitivity after the initial state is set */
  	g_signal_connect (window->priv->bottom_panel,
			  "item_removed",
			  G_CALLBACK (bottom_panel_item_removed),
			  window);
  	g_signal_connect (window->priv->bottom_panel,
			  "item_added",
			  G_CALLBACK (bottom_panel_item_added),
			  window);
}

static void
gedit_window_init (GeditWindow *window)
{
	static gboolean is_first = TRUE;
	GtkWidget *main_box;

	gedit_debug (DEBUG_WINDOW);

	window->priv = GEDIT_WINDOW_GET_PRIVATE (window);
	window->priv->active_tab = NULL;
	window->priv->tabs = NULL;
	window->priv->state = GEDIT_WINDOW_STATE_NORMAL;

	window->priv->window_group = gtk_window_group_new ();
	gtk_window_group_add_window (window->priv->window_group, GTK_WINDOW (window));

	main_box = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), main_box);
	gtk_widget_show (main_box);

	/* Add menu bar and toolbar bar */
	create_menu_bar_and_toolbar (window, main_box);

	/* Add status bar */
	create_statusbar (window, main_box);

	/* Add the main area */
	gedit_debug_message (DEBUG_WINDOW, "Add main area");		
	window->priv->hpaned = gtk_hpaned_new ();
  	gtk_box_pack_start (GTK_BOX (main_box), 
  			    window->priv->hpaned, 
  			    TRUE, 
  			    TRUE, 
  			    0);

	window->priv->vpaned = gtk_vpaned_new ();
  	gtk_paned_pack2 (GTK_PANED (window->priv->hpaned), 
  			 window->priv->vpaned, 
  			 TRUE, 
  			 FALSE);		 

	/* side and bottom panels */
  	create_side_panel (window);
	create_bottom_panel (window);

	/* restore paned positions as soon as we know the panes allocation.
	 * This needs to be done only for the first window, the successive
	 * windows are created by cloning the first one */
	if (is_first)
	{
		is_first = FALSE;

		window->priv->side_panel_size = gedit_prefs_manager_get_side_panel_size ();
		window->priv->bottom_panel_size = gedit_prefs_manager_get_bottom_panel_size ();
		g_signal_connect_after (window->priv->hpaned,
					"map",
					G_CALLBACK (hpaned_restore_position),
					window);
		g_signal_connect_after (window->priv->vpaned,
					"map",
					G_CALLBACK (vpaned_restore_position),
					window);
	}

	gtk_widget_show (window->priv->hpaned);
	gtk_widget_show (window->priv->vpaned);

	/* Drag and drop support */
	gtk_drag_dest_set (GTK_WIDGET (window),
			   GTK_DEST_DEFAULT_MOTION |
			   GTK_DEST_DEFAULT_HIGHLIGHT |
			   GTK_DEST_DEFAULT_DROP,
			   drag_types,
			   G_N_ELEMENTS (drag_types),
			   GDK_ACTION_COPY);

	/* connect instead of override, so that we can
	 * share the cb code with the view */
	g_signal_connect (window,
			  "drag_data_received",
	                  G_CALLBACK (drag_data_received_cb), 
	                  NULL);

	gedit_debug_message (DEBUG_WINDOW, "Update plugins ui");
	gedit_plugins_engine_update_plugins_ui (window, TRUE);

	/* set visibility of panes.
	 * This needs to be done after plugins activatation */
	init_panels_visibility (window);

	gedit_debug_message (DEBUG_WINDOW, "END");
}

GeditView *
gedit_window_get_active_view (GeditWindow *window)
{
	GeditView *view;

	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	if (window->priv->active_tab == NULL)
		return NULL;

	view = gedit_tab_get_view (GEDIT_TAB (window->priv->active_tab));

	return view;
}

GeditDocument *
gedit_window_get_active_document (GeditWindow *window)
{
	GeditView *view;

	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	view = gedit_window_get_active_view (window);
	if (view == NULL)
		return NULL;

	return GEDIT_DOCUMENT (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
}

GeditTab *
gedit_window_create_tab (GeditWindow *window,
			 gboolean     jump_to)
{
	GeditWindowClass *klass;

	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);
	
	klass = GEDIT_WINDOW_GET_CLASS(window);
	
	if (klass->create_tab != NULL)
		return klass->create_tab(window, jump_to);

	return NULL;
}

GeditTab *
gedit_window_create_tab_from_uri (GeditWindow         *window,
				  const gchar         *uri,
				  const GeditEncoding *encoding,
				  gint                 line_pos,
				  gboolean             create,
				  gboolean             jump_to)
{
	GeditWindowClass *klass;

	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);
	
	klass = GEDIT_WINDOW_GET_CLASS(window);
	
	if (klass->create_tab_from_uri != NULL)
		return klass->create_tab_from_uri (window, 
						   uri,
						   encoding,
						   line_pos,
						   create,
						   jump_to);

	return NULL;
}				  

GeditTab *
gedit_window_get_active_tab (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);
	
	return (window->priv->active_tab == NULL) ? 
				NULL : GEDIT_TAB (window->priv->active_tab);
}

/* Returns a newly allocated list with all the tabs in the window */
GList *
gedit_window_get_tabs (GeditWindow *window)
{
	return g_list_copy(window->priv->tabs);
}

/* Returns a newly allocated list with all the documents in the window */
GList *
gedit_window_get_documents (GeditWindow *window)
{
	GList *res = NULL;
	GList *item;
	GeditDocument *document;
	
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);
	
	for (item = window->priv->tabs; item; item = item->next) {
		document = gedit_tab_get_document (GEDIT_TAB (item->data));
		res = g_list_prepend (res, document);
	}
			       
	res = g_list_reverse (res);
	
	return res;
}

/* Returns a newly allocated list with all the views in the window */
GList *
gedit_window_get_views (GeditWindow *window)
{
	GList *res = NULL;
	GList *item;
	GeditView *view;
	
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);
	
	for (item = window->priv->tabs; item; item = item->next) {
		view = gedit_tab_get_view (GEDIT_TAB (item->data));
		res = g_list_prepend (res, view);
	}
			       
	res = g_list_reverse (res);;
	
	return res;
}

void
gedit_window_close_tab (GeditWindow *window,
			GeditTab    *tab)
{
	GeditWindowClass *klass;
	
	g_return_if_fail (GEDIT_IS_WINDOW (window));
	g_return_if_fail (GEDIT_IS_TAB (tab));
	g_return_if_fail ((gedit_tab_get_state (tab) != GEDIT_TAB_STATE_SAVING) &&
			  (gedit_tab_get_state (tab) != GEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW));
	
	klass = GEDIT_WINDOW_GET_CLASS(window);
	
	if (klass->close_tab != NULL)
		klass->close_tab (window, tab);
}

void
gedit_window_close_all_tabs (GeditWindow *window)
{
	GList *tabs;
	
	g_return_if_fail (GEDIT_IS_WINDOW (window));
	g_return_if_fail (!(window->priv->state & GEDIT_WINDOW_STATE_SAVING) &&
			  !(window->priv->state & GEDIT_WINDOW_STATE_SAVING_SESSION));

	if (window->priv->tabs == NULL)
		return;

	tabs = g_list_copy(window->priv->tabs);
	gedit_window_close_tabs (window, tabs);
	g_list_free(tabs);
}

void
gedit_window_close_tabs (GeditWindow *window,
			 const GList *tabs)
{
	g_return_if_fail (GEDIT_IS_WINDOW (window));
	g_return_if_fail (!(window->priv->state & GEDIT_WINDOW_STATE_SAVING) &&
			  !(window->priv->state & GEDIT_WINDOW_STATE_SAVING_SESSION));

	if (tabs == NULL)
		return;

	window->priv->is_removing_tabs = TRUE;
	
	while (tabs != NULL)
	{
		if (tabs->next == NULL)
			window->priv->is_removing_tabs = FALSE;
		
		gedit_window_close_tab (window, GEDIT_TAB (tabs->data));
		tabs = g_list_next (tabs);
	}
}		      

void
gedit_window_set_active_tab (GeditWindow *window,
			     GeditTab    *tab)
{
	GeditWindowClass *klass;
	GeditView        *view;

	g_return_if_fail (GEDIT_IS_WINDOW (window));
	g_return_if_fail (GEDIT_IS_TAB (tab));

	if (tab == window->priv->active_tab)
		return;
		
	window->priv->active_tab = tab;

	set_title (window);
	set_sensitivity_according_to_tab (window, tab);

	/* update the syntax menu */
	update_languages_menu (window);

	view = gedit_tab_get_view (tab);

	/* sync the statusbar */
	update_cursor_position_statusbar (GTK_TEXT_BUFFER (gedit_tab_get_document (tab)),
					  window);
	gedit_statusbar_set_overwrite (GEDIT_STATUSBAR (window->priv->statusbar),
				       gtk_text_view_get_overwrite (GTK_TEXT_VIEW (view)));
	
	g_signal_emit (G_OBJECT (window), 
		       signals[ACTIVE_TAB_CHANGED], 
		       0, 
		       window->priv->active_tab);				       

	klass = GEDIT_WINDOW_GET_CLASS (window);

	if (klass->set_active_tab != NULL)
		klass->set_active_tab (window, tab);	
}

GtkWindowGroup *
gedit_window_get_group (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);
	
	return window->priv->window_group;
}

GtkUIManager *
gedit_window_get_ui_manager (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	return window->priv->manager;
}

GeditPanel *
gedit_window_get_side_panel (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	return GEDIT_PANEL (window->priv->side_panel);
}

GeditPanel *
gedit_window_get_bottom_panel (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	return GEDIT_PANEL (window->priv->bottom_panel);
}

GtkWidget *
gedit_window_get_statusbar (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), 0);

	return window->priv->statusbar;
}

GeditWindowState
gedit_window_get_state (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), GEDIT_WINDOW_STATE_NORMAL);

	return window->priv->state;
}

G_CONST_RETURN gchar *
_gedit_window_get_default_path (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);
	
	return window->priv->default_path;
}

/* Returns the documents that need to be saved before closing the window */
GList *
gedit_window_get_unsaved_documents (GeditWindow *window)
{
	GList *unsaved_docs = NULL;
	GList *docs;
	GList *l;

	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);
	
	docs = gedit_window_get_documents (window);

	for (l = docs; l != NULL; l = l->next)
	{
		GeditTab *tab;
		GeditDocument *doc;

		doc = GEDIT_DOCUMENT (l->data);
		tab = gedit_tab_get_from_document (doc);

		if (!_gedit_tab_can_close (tab))
			unsaved_docs = g_list_prepend (unsaved_docs, doc);
	}

	g_list_free (docs);

	return g_list_reverse (unsaved_docs);
}

void 
_gedit_window_set_saving_session_state (GeditWindow *window,
					gboolean     saving_session)
{
	GeditWindowState old_state;

	g_return_if_fail (GEDIT_IS_WINDOW (window));
	
	old_state = window->priv->state;

	if (saving_session)
		window->priv->state |= GEDIT_WINDOW_STATE_SAVING_SESSION;
	else
		window->priv->state &= ~GEDIT_WINDOW_STATE_SAVING_SESSION;

	if (old_state != window->priv->state)
	{
		set_sensitivity_according_to_window_state (window);

		g_object_notify (G_OBJECT (window), "state");
	}
}

void
_gedit_window_set_widget (GeditWindow *window,
			  GtkWidget   *widget)
{
	gedit_debug_message (DEBUG_WINDOW, "Set widget");
  	gtk_paned_pack1 (GTK_PANED (window->priv->vpaned), 
  			 widget,
  			 TRUE, 
  			 TRUE);
  	gtk_widget_show (widget); 
}

gboolean
_gedit_window_is_removing_tabs (GeditWindow *window)
{
	return window->priv->is_removing_tabs;
}
