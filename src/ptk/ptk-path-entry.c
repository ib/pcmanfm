/*
*  C Implementation: ptk-path-entry
*
* Description: A custom entry widget with auto-completion
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "ptk-path-entry.h"
#include <gdk/gdkkeysyms.h>
#include "vfs-file-info.h"  /* for vfs_file_resolve_path */
#include <string.h>

enum
{
    COL_NAME,
    COL_PATH,
    N_COLS
};

/*
static GQuark use_hand_cursor = (GQuark)"hand_cursor";
#define is_hand_cursor_used( entry )    (g_object_get_qdata(entry, use_hand_cursor))
*/

static char*
get_cwd( GtkEntry* entry )
{
    char *real_path = vfs_file_resolve_path( NULL, gtk_entry_get_text(entry) );
    if( G_LIKELY( g_path_is_absolute( real_path ) ) )
    {
        char* ret = g_path_get_dirname( real_path );
        g_free( real_path );
        return ret;
    }
    return NULL;
}

static gboolean match_func( GtkEntryCompletion *completion,
                                                     const gchar *key,
                                                     GtkTreeIter *it,
                                                     gpointer user_data)
{
    char* name = NULL;
    GtkTreeModel* model = gtk_entry_completion_get_model(completion);

    key = (const char*)g_object_get_data( G_OBJECT(completion), "fn" );
    gtk_tree_model_get( model, it, COL_NAME, &name, -1 );

    if( G_LIKELY(name) )
    {
        if( *key == 0 || 0 == g_ascii_strncasecmp( name, key, strlen(key) ) )
        {
            g_free( name );
            return TRUE;
        }
        g_free( name );
    }
    return FALSE;
}

static void update_completion( GtkEntry* entry,
                               GtkEntryCompletion* completion )
{
    char* new_dir, *fn;
    const char* old_dir;
    GtkListStore* list;
    const char *sep;

    sep = strrchr( gtk_entry_get_text(entry), '/' );
    if( sep )
        fn = (char*)sep + 1;
    else
        fn = (char*)gtk_entry_get_text(entry);
    g_object_set_data_full( G_OBJECT(completion), "fn", g_strdup(fn), (GDestroyNotify)g_free );

    new_dir = get_cwd( entry );
    old_dir = (const char*)g_object_get_data( (GObject*)completion, "cwd" );
    if( old_dir && new_dir && 0 == g_ascii_strcasecmp( old_dir, new_dir ) )
    {
        g_free( new_dir );
        return;
    }
    g_object_set_data_full( (GObject*)completion, "cwd",
                             new_dir, g_free );
    list = (GtkListStore*)gtk_entry_completion_get_model( completion );
    gtk_list_store_clear( list );
    if( new_dir )
    {
        GDir* dir;
        if( (dir = g_dir_open( new_dir, 0, NULL )) )
        {
            const char* name;
            while( (name = g_dir_read_name( dir )) )
            {
                char* full_path = g_build_filename( new_dir, name, NULL );
                if( g_file_test( full_path, G_FILE_TEST_IS_DIR ) )
                {
                    GtkTreeIter it;
                    char* disp_name = g_filename_display_basename( full_path );
                    gtk_list_store_append( list, &it );
                    gtk_list_store_set( list, &it, COL_NAME, disp_name, COL_PATH, full_path, -1 );
                    g_free( disp_name );
                }
                g_free( full_path );
            }
            g_dir_close( dir );

            gtk_entry_completion_set_match_func( completion, match_func, new_dir, NULL );
        }
        else
            gtk_entry_completion_set_match_func( completion, NULL, NULL, NULL );
    }
}

static void
on_changed( GtkEntry* entry, gpointer user_data )
{
    GtkEntryCompletion* completion;
    completion = gtk_entry_get_completion( entry );
    update_completion( entry, completion );
}

static gboolean
on_key_press( GtkWidget *entry, GdkEventKey* evt, gpointer user_data )
{
    if( evt->keyval == GDK_Tab && ! (evt->state & GDK_CONTROL_MASK) )
    {
#if GTK_CHECK_VERSION(2, 8, 0)
        /* This API exists since gtk+ 2.6, but gtk+ 2.6.x seems to have bugs
           related to this API which cause crash.
           Reported in bug #1570063 by Orlando Fiol <fiolorlando@gmail.com>
        */
        gtk_entry_completion_insert_prefix( gtk_entry_get_completion(GTK_ENTRY(entry)) );
#endif
        gtk_editable_set_position( (GtkEditable*)entry, -1 );
        return TRUE;
    }
    return FALSE;
}

static gboolean
on_focus_in( GtkWidget *entry, GdkEventFocus* evt, gpointer user_data )
{
    GtkEntryCompletion* completion = gtk_entry_completion_new();
    GtkListStore* list = gtk_list_store_new( N_COLS, G_TYPE_STRING, G_TYPE_STRING );
    GtkCellRenderer* render;

    gtk_entry_completion_set_minimum_key_length( completion, 1 );
    gtk_entry_completion_set_model( completion, GTK_TREE_MODEL(list) );
    g_object_unref( list );

    /* gtk_entry_completion_set_text_column( completion, COL_PATH ); */
    g_object_set( completion, "text-column", COL_PATH, NULL );
    render = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start( (GtkCellLayout*)completion, render, TRUE );
    gtk_cell_layout_add_attribute( (GtkCellLayout*)completion, render, "text", COL_NAME );

    gtk_entry_completion_set_inline_completion( completion, TRUE );
#if GTK_CHECK_VERSION( 2, 8, 0)
    /* gtk+ prior to 2.8.0 doesn't have this API */
    gtk_entry_completion_set_popup_set_width( completion, TRUE );
#endif
    gtk_entry_set_completion( GTK_ENTRY(entry), completion );
    g_signal_connect( G_OBJECT(entry), "changed", G_CALLBACK(on_changed), NULL );
    g_object_unref( completion );

    return FALSE;
}

static gboolean
on_focus_out( GtkWidget *entry, GdkEventFocus* evt, gpointer user_data )
{
    g_signal_handlers_disconnect_by_func( entry, on_changed, NULL );
    gtk_entry_set_completion( GTK_ENTRY(entry), NULL );
    return FALSE;
}

#if 0
/* Weird!  We cannot change the cursor of GtkEntry... */

static gboolean on_mouse_move(GtkWidget      *entry,
                                                             GdkEventMotion *evt,
                                                             gpointer        user_data)
{
    if( evt->state == GDK_CONTROL_MASK )
    {
        if( ! is_hand_cursor_used( entry ) )
        {
            GdkCursor* hand = gdk_cursor_new_for_display( gtk_widget_get_display(entry), GDK_HAND2 );
            gdk_window_set_cursor( entry->window, hand );
            gdk_cursor_unref( hand );
            g_object_set_qdata( entry, use_hand_cursor, (gpointer)TRUE );
            g_debug( "SET" );
        }
        return TRUE;
    }
    else
    {
        if( is_hand_cursor_used( entry ) )
        {
            gdk_window_set_cursor( entry->window, NULL );
            g_object_set_qdata( entry, use_hand_cursor, (gpointer)FALSE );
            g_debug( "UNSET" );
        }
    }
    return FALSE;
}

static gboolean on_button_press(GtkWidget      *entry,
                                                                 GdkEventButton *evt,
                                                                 gpointer        user_data)
{
    return FALSE;
}
#endif

static gboolean on_button_release(GtkEntry      *entry,
                                                                    GdkEventButton *evt,
                                                                    gpointer        user_data)
{
    if( GDK_BUTTON_RELEASE == evt->type
        && (evt->state & GDK_CONTROL_MASK)
        && 1 == evt->button )
    {
        int pos;
        const char *text, *sep;
        char *path;

        pos = gtk_editable_get_position( GTK_EDITABLE(entry) );
        text = gtk_entry_get_text( entry );
        if( G_LIKELY( text && *text ) )
        {
            sep = g_utf8_offset_to_pointer( text, pos );
            if( G_LIKELY( sep ) )
            {
                while( *sep && *sep != '/' )
                    sep = g_utf8_next_char(sep);
                if( G_UNLIKELY( sep == text ) )
                {
                    if( '/' == *sep )
                        ++sep;
                    else
                        return FALSE;
                }
                path = g_strndup( text, (sep - text) );
                gtk_entry_set_text( entry, path );
                g_free( path );

                gtk_widget_activate( (GtkWidget*)entry );
            }
        }
    }
    return FALSE;
}

GtkWidget* ptk_path_entry_new()
{
    GtkWidget* entry = gtk_entry_new();

    g_signal_connect( entry, "focus-in-event", G_CALLBACK(on_focus_in), NULL );
    g_signal_connect( entry, "focus-out-event", G_CALLBACK(on_focus_out), NULL );

    /* used to eat the tab key */
    g_signal_connect( entry, "key-press-event", G_CALLBACK(on_key_press), NULL );

/*
    g_signal_connect( entry, "motion-notify-event", G_CALLBACK(on_mouse_move), NULL );
    g_signal_connect( entry, "button-press-event", G_CALLBACK(on_button_press), NULL );
*/
    g_signal_connect( entry, "button-release-event", G_CALLBACK(on_button_release), NULL );

    return entry;
}
