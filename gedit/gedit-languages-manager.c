/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gedit-languages-manager.c
 * This file is part of gedit
 *
 * Copyright (C) 2003-2006 - Paolo Maggi 
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
 * Modified by the gedit Team, 2003-2006. See the AUTHORS file for a 
 * list of people on the gedit Team.  
 * See the ChangeLog files for a list of changes. 
 *
 * $Id$
 */

#include <string.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include "gedit-languages-manager.h"
#include "gedit-prefs-manager.h"
#include "gedit-utils.h"
#include "gedit-debug.h"

static GtkSourceLanguagesManager *language_manager = NULL;
static GConfClient *gconf_client = NULL;
static const GSList *languages_list = NULL;
static GSList *languages_list_sorted = NULL;


GtkSourceLanguagesManager *
gedit_get_languages_manager (void)
{
	if (language_manager == NULL)
	{
		language_manager = gtk_source_languages_manager_new ();	

		gconf_client = gconf_client_get_default ();
		g_return_val_if_fail (gconf_client != NULL, language_manager);
	}

	return language_manager;
}

// NUKE THIS WRAPPER NO LONGER NEEDED
GtkSourceLanguage *
gedit_languages_manager_get_language_from_id (GtkSourceLanguagesManager *lm,
					      const gchar               *lang_id)
{
	return gtk_source_languages_manager_get_language_by_id (lm, lang_id);
}

static GSList *initialized_languages = NULL;

void 
gedit_language_init_tag_styles (GtkSourceLanguage *language)
{
	initialized_languages =	g_slist_prepend (initialized_languages, language);
}

static gint
language_compare (gconstpointer a, gconstpointer b)
{
	GtkSourceLanguage *lang_a = GTK_SOURCE_LANGUAGE (a);
	GtkSourceLanguage *lang_b = GTK_SOURCE_LANGUAGE (b);
	gchar *name_a = gtk_source_language_get_name (lang_a);
	gchar *name_b = gtk_source_language_get_name (lang_b);
	gint res;
	
	res = g_utf8_collate (name_a, name_b);
		
	g_free (name_a);
	g_free (name_b);	
	
	return res;
}

/* Move the sorting in sourceview as a flag to get_available_langs? */
const GSList*
gedit_languages_manager_get_available_languages_sorted (GtkSourceLanguagesManager *lm)
{
	const GSList *languages;

	languages = gtk_source_languages_manager_get_available_languages (lm);

	if ((languages_list_sorted == NULL) || (languages != languages_list))
	{
		languages_list = languages;
		languages_list_sorted = g_slist_copy ((GSList*)languages);
		languages_list_sorted = g_slist_sort (languages_list_sorted, (GCompareFunc)language_compare);
	}

	return languages_list_sorted;
}

// We need to figure out what needs to go into gsv itself

static GSList *
language_get_mime_types (GtkSourceLanguage *lang)
{
	const gchar *mimetypes;
	gchar **mtl;
	gint i;
	GSList *mime_types = NULL;

	mimetypes = gtk_source_language_get_property (lang, "mimetypes");
	if (mimetypes == NULL)
		return NULL;

	mtl = g_strsplit (mimetypes, ";", 0);

	for (i = 0; mtl[i] != NULL; i++)
	{
		/* steal the strings from the array */
		mime_types = g_slist_prepend (mime_types, mtl[i]);
	}

	g_free (mtl);

	return mime_types;
}

/* Returns a hash table that is used as a cache of already matched mime-types */
static GHashTable *
get_languages_cache (GtkSourceLanguagesManager *lm)
{
	static GQuark id = 0;
	GHashTable *res;
	
	if (id == 0)
		id = g_quark_from_static_string ("gedit_languages_manager_cache");

	res = (GHashTable *)g_object_get_qdata (G_OBJECT (lm), id);
	if (res == NULL)
	{
		res = g_hash_table_new_full (g_str_hash,
					     g_str_equal,
					     g_free,
					     NULL);

		g_object_set_qdata_full (G_OBJECT (lm), 
					 id,
					 res,
					 (GDestroyNotify)g_hash_table_unref);
	}
	
	return res;
}

static GtkSourceLanguage *
get_language_from_cache (GtkSourceLanguagesManager *lm,
			 const gchar               *mime_type)
{
	GHashTable *cache;
	
	cache = get_languages_cache (lm);
	
	return g_hash_table_lookup (cache, mime_type);
}

static void
add_language_to_cache (GtkSourceLanguagesManager *lm,
		       const gchar               *mime_type,
		       GtkSourceLanguage         *lang)
{
	GHashTable *cache;
	
	cache = get_languages_cache (lm);
	g_hash_table_replace (cache, g_strdup (mime_type), lang);
}

/*
 * gedit_languages_manager_get_language_from_mime_type works like
 * gtk_source_languages_manager_get_language_from_mime_type but it uses
 * gnome_vfs_mime_type_get_equivalence instead of strcmp to compare mime-types.
 * In this way it takes care of mime-types inheritance (fixes bug #324191)
 * It also uses a cache of already matched mime-type in order to improve
 * performance for frequently used mime-types.
 */
GtkSourceLanguage *
gedit_languages_manager_get_language_from_mime_type (GtkSourceLanguagesManager 	*lm,
						     const gchar                *mime_type)
{
	const GSList *languages;
	GtkSourceLanguage *lang;
	GtkSourceLanguage *parent = NULL;
	
	g_return_val_if_fail (mime_type != NULL, NULL);

	languages = gtk_source_languages_manager_get_available_languages (lm);
	
	lang = get_language_from_cache (lm, mime_type);
	if (lang != NULL)
	{
		gedit_debug_message (DEBUG_DOCUMENT,
				     "Cache hit for %s", mime_type);
		return lang;
	}
	gedit_debug_message (DEBUG_DOCUMENT,
			     "Cache miss for %s", mime_type);

	while (languages != NULL)
	{
		GSList *mime_types, *tmp;

		lang = GTK_SOURCE_LANGUAGE (languages->data);

		tmp = mime_types = language_get_mime_types (lang);

		while (tmp != NULL)
		{
			GnomeVFSMimeEquivalence res;
			res = gnome_vfs_mime_type_get_equivalence (mime_type, 
								   (const gchar*)tmp->data);
									
			if (res == GNOME_VFS_MIME_IDENTICAL)
			{	
				/* If the mime-type of lang is identical to "mime-type" then
				   return lang */
				gedit_debug_message (DEBUG_DOCUMENT,
						     "%s is indentical to %s\n", (const gchar*)tmp->data, mime_type);
				   
				break;
			}
			else if ((res == GNOME_VFS_MIME_PARENT) && (parent == NULL))
			{
				/* If the mime-type of lang is a parent of "mime-type" then
				   remember it. We will return it if we don't find
				   an exact match. The first "parent" wins */
				parent = lang;
				
				gedit_debug_message (DEBUG_DOCUMENT,
						     "%s is a parent of %s\n", (const gchar*)tmp->data, mime_type);
			}

			tmp = g_slist_next (tmp);
		}

		g_slist_foreach (mime_types, (GFunc) g_free, NULL);
		g_slist_free (mime_types);
		
		if (tmp != NULL)
		{
			add_language_to_cache (lm, mime_type, lang);
			return lang;
		}
		
		languages = g_slist_next (languages);
	}

	if (parent != NULL)
		add_language_to_cache (lm, mime_type, parent);
	
	return parent;
}

