/*
*  C Implementation: ptk-file-misc
*
* Description: Miscellaneous GUI-realated functions for files
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include "ptk-file-misc.h"

#include <glib.h>
#include "glib-mem.h"

#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#include <glib/gi18n.h>
#include "ptk-utils.h"
#include "ptk-file-task.h"
#include "vfs-file-task.h"
#include "ptk-file-properties.h"
#include "ptk-input-dialog.h"
#include "ptk-file-browser.h"
#include "vfs-app-desktop.h"
#include "vfs-execute.h"
#include "ptk-app-chooser.h"

/* FIXME: This is a bad hack and break the independence of ptk */
#include "settings.h"   /* for app_settings.use_trash_can */

void ptk_delete_files( GtkWindow* parent_win,
                       const char* cwd,
                       GList* sel_files )
{
    GtkWidget * dlg;
    gchar* file_path;
    gint ret;
    GList* sel;
    VFSFileInfo* file;
    PtkFileTask* task = NULL;
    GList* file_list;

    if ( ! sel_files )
        return ;

    if( ! app_settings.use_trash_can )
    {
        dlg = gtk_message_dialog_new( parent_win,
                                      GTK_DIALOG_MODAL,
                                      GTK_MESSAGE_WARNING,
                                      GTK_BUTTONS_YES_NO,
                                      _( "Really delete selected files?" ) );
        ret = gtk_dialog_run( GTK_DIALOG( dlg ) );
        gtk_widget_destroy( dlg );
        if ( ret != GTK_RESPONSE_YES )
            return ;
    }

    file_list = NULL;
    for ( sel = sel_files; sel; sel = g_list_next( sel ) )
    {
        file = ( VFSFileInfo* ) sel->data;
        file_path = g_build_filename( cwd,
                                      vfs_file_info_get_name( file ), NULL );
        file_list = g_list_prepend( file_list, file_path );
    }
    /* file_list = g_list_reverse( file_list ); */
    task = ptk_file_task_new( app_settings.use_trash_can ? VFS_FILE_TASK_TRASH : VFS_FILE_TASK_DELETE,
                              file_list,
                              NULL,
                              GTK_WINDOW( parent_win ) );
    ptk_file_task_run( task );
}

static void select_file_name_part( GtkEntry* entry )
{
    GtkEditable * editable = GTK_EDITABLE( entry );
    const char* file_name = gtk_entry_get_text( entry );
    const char* dot;
    int pos;

    if ( !file_name[ 0 ] || file_name[ 0 ] == '.' )
        return ;
    /* FIXME: Simply finding '.' usually gets wrong filename suffix. */
    dot = g_utf8_strrchr( file_name, -1, '.' );
    if ( dot )
    {
        pos = g_utf8_pointer_to_offset( file_name, dot );
        gtk_editable_select_region( editable, 0, pos );
    }
}

gboolean  ptk_rename_file( GtkWindow* parent_win,
                      const char* cwd,
                      VFSFileInfo* file )
{
    GtkWidget * input_dlg;
    GtkLabel* prompt;
    char* ufile_name;
    char* file_name;
    char* from_path;
    char* to_path;
    gboolean ret = FALSE;
    char* disp_name = NULL;

    if ( !cwd || !file )
        return FALSE;

    /* special processing for files with incosistent real name and display name */
    if( G_UNLIKELY( vfs_file_info_is_desktop_entry(file) ) )
        disp_name = g_filename_display_name( file->name );

    input_dlg = ptk_input_dialog_new( _( "Rename File" ),
                                      _( "Please input new file name:" ),
                                      disp_name ? disp_name : vfs_file_info_get_disp_name( file ), parent_win );
    g_free( disp_name );
    gtk_window_set_default_size( GTK_WINDOW( input_dlg ),
                                 360, -1 );

    /* Without this line, selected region in entry cannot be set */
    gtk_widget_show( input_dlg );
    if ( ! vfs_file_info_is_dir( file ) )
        select_file_name_part( GTK_ENTRY( ptk_input_dialog_get_entry( GTK_WIDGET( input_dlg ) ) ) );

    while ( gtk_dialog_run( GTK_DIALOG( input_dlg ) ) == GTK_RESPONSE_OK )
    {
        prompt = GTK_LABEL( ptk_input_dialog_get_label( input_dlg ) );

        ufile_name = ptk_input_dialog_get_text( input_dlg );
        file_name = g_filename_from_utf8( ufile_name, -1, NULL, NULL, NULL );
        g_free( ufile_name );

        if ( file_name )
        {
            from_path = g_build_filename( cwd,
                                          vfs_file_info_get_name( file ), NULL );
            to_path = g_build_filename( cwd,
                                        file_name, NULL );

            if ( g_file_test( to_path, G_FILE_TEST_EXISTS ) )
            {
                gtk_label_set_text( prompt,
                                    _( "The file name you specified already exists.\n"
                                       "Please input a new one:" ) );
            }
            else
            {
                if ( 0 == rename( from_path, to_path ) )
                {
                    ret = TRUE;
                    break;
                }
                else
                {
                    gtk_label_set_text( prompt, strerror( errno ) );
                }
            }
            g_free( from_path );
            g_free( to_path );
        }
        else
        {
            gtk_label_set_text( prompt,
                                _( "The file name you specified already exists.\n"
                                   "Please input a new one:" ) );
        }
    }
    gtk_widget_destroy( input_dlg );
    return ret;
}

gboolean  ptk_create_new_file( GtkWindow* parent_win,
                          const char* cwd,
                          gboolean create_folder,
                          VFSFileInfo** file )
{
    gchar * full_path;
    gchar* ufile_name;
    gchar* file_name;
    GtkLabel* prompt;
    int result;
    GtkWidget* dlg;
    gboolean ret = FALSE;

    if ( create_folder )
    {
        dlg = ptk_input_dialog_new( _( "New Folder" ),
                                    _( "Input a name for the new folder:" ),
                                    _( "New Folder" ),
                                    parent_win );
    }
    else
    {
        dlg = ptk_input_dialog_new( _( "New File" ),
                                    _( "Input a name for the new file:" ),
                                    _( "New File" ),
                                    parent_win );
    }

    while ( gtk_dialog_run( GTK_DIALOG( dlg ) ) == GTK_RESPONSE_OK )
    {
        ufile_name = ptk_input_dialog_get_text( dlg );
        if ( g_get_filename_charsets( NULL ) )
            file_name = ufile_name;
        else
        {
            file_name = g_filename_from_utf8( ufile_name, -1,
                                              NULL, NULL, NULL );
            g_free( ufile_name );
        }
        full_path = g_build_filename( cwd, file_name, NULL );
        g_free( file_name );
        if ( g_file_test( full_path, G_FILE_TEST_EXISTS ) )
        {
            prompt = GTK_LABEL( ptk_input_dialog_get_label( dlg ) );
            gtk_label_set_text( prompt,
                                _( "The file name you specified already exists.\n"
                                   "Please input a new one:" ) );
            g_free( full_path );
            continue;
        }
        if ( create_folder )
        {
            result = mkdir( full_path, 0755 );
            ret = (result==0);
        }
        else
        {
            result = open( full_path, O_CREAT, 0644 );
            if ( result != -1 )
            {
                close( result );
                ret = TRUE;
            }
        }

        if( ret && file )
        {
            *file = vfs_file_info_new();
            vfs_file_info_get( *file, full_path, NULL );
        }

        g_free( full_path );
        break;
    }
    gtk_widget_destroy( dlg );

    if( ! ret )
        ptk_show_error( parent_win,
                        _("Error"),
                        _( "The new file cannot be created!" ) );

    return ret;
}

void ptk_show_file_properties( GtkWindow* parent_win,
                               const char* cwd,
                               GList* sel_files )
{
    GtkWidget * dlg;

    /* Make a copy of the list */
    sel_files = g_list_copy( sel_files );
    g_list_foreach( sel_files, (GFunc) vfs_file_info_ref, NULL );

    dlg = file_properties_dlg_new( parent_win, cwd, sel_files );
    g_signal_connect_swapped( dlg, "destroy",
                              G_CALLBACK( vfs_file_info_list_free ), sel_files );
    gtk_widget_show( dlg );
}

static gboolean open_files_with_app( const char* cwd,
                                     GList* files,
                                     const char* app_desktop,
                                     PtkFileBrowser* file_browser )
{
    gchar * name;
    GError* err = NULL;
    VFSAppDesktop* app;
    GdkScreen* screen;

    /* Check whether this is an app desktop file or just a command line */
    /* Not a desktop entry name */
    if ( g_str_has_suffix ( app_desktop, ".desktop" ) )
    {
        app = vfs_app_desktop_new( app_desktop );
    }
    else
    {
        /*
        * If we are lucky enough, there might be a desktop entry
        * for this program
        */
        name = g_strconcat ( app_desktop, ".desktop", NULL );
        if ( g_file_test( name, G_FILE_TEST_EXISTS ) )
        {
            app = vfs_app_desktop_new( name );
        }
        else
        {
            /* dirty hack! */
            app = vfs_app_desktop_new( NULL );
            app->exec = g_strdup( app_desktop );
        }
        g_free( name );
    }

    if( file_browser )
        screen = gtk_widget_get_screen( GTK_WIDGET(file_browser) );
    else
        screen = gdk_screen_get_default();

    if ( ! vfs_app_desktop_open_files( screen, cwd, app, files, &err ) )
    {
        GtkWidget * toplevel = file_browser ? gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ) : NULL;
        char* msg = g_markup_escape_text(err->message, -1);
        ptk_show_error( GTK_WINDOW( toplevel ),
                        _("Error"),
                        msg );
        g_free(msg);
        g_error_free( err );
    }
    vfs_app_desktop_unref( app );
    return TRUE;
}

static void open_files_with_each_app( gpointer key, gpointer value, gpointer user_data )
{
    const char * app_desktop = ( const char* ) key;
    const char* cwd;
    GList* files = ( GList* ) value;
    PtkFileBrowser* file_browser = ( PtkFileBrowser* ) user_data;
    /* FIXME: cwd should be passed into this function */
    cwd = file_browser ? ptk_file_browser_get_cwd(file_browser) : NULL;
    open_files_with_app( cwd, files, app_desktop, file_browser );
}

static void free_file_list_hash( gpointer key, gpointer value, gpointer user_data )
{
    const char * app_desktop;
    GList* files;

    app_desktop = ( const char* ) key;
    files = ( GList* ) value;
    g_list_foreach( files, ( GFunc ) g_free, NULL );
    g_list_free( files );
}

void ptk_open_files_with_app( const char* cwd,
                              GList* sel_files,
                              char* app_desktop,
                              PtkFileBrowser* file_browser )

{
    GList * l;
    gchar* full_path = NULL;
    VFSFileInfo* file;
    VFSMimeType* mime_type;
    GList* files_to_open = NULL;
    GHashTable* file_list_hash = NULL;
    GError* err;
    char* new_dir = NULL;
    char* choosen_app = NULL;
    GtkWidget* toplevel;

    for ( l = sel_files; l; l = l->next )
    {
        file = ( VFSFileInfo* ) l->data;
        if ( G_UNLIKELY( ! file ) )
            continue;

        full_path = g_build_filename( cwd,
                                      vfs_file_info_get_name( file ),
                                      NULL );
        if ( G_LIKELY( full_path ) )
        {
            if ( ! app_desktop )    /* Use default apps for each file */
            {
                if ( g_file_test( full_path, G_FILE_TEST_IS_DIR ) )
                {
                    if ( ! new_dir )
                        new_dir = full_path;
                    else
                    {
                        if ( G_LIKELY( file_browser ) )
                        {
                            ptk_file_browser_emit_open( file_browser,
                                                        full_path, PTK_OPEN_NEW_TAB );
                        }
                        g_free( full_path );
                    }
                    continue;
                }
                /* If this file is an executable file, run it. */
                if ( vfs_file_info_is_executable( file, full_path ) )
                {
                    char * argv[ 2 ] = { full_path, NULL };
                    GdkScreen* screen = file_browser ? gtk_widget_get_screen( GTK_WIDGET(file_browser) ) : gdk_screen_get_default();
                    err = NULL;
                    if ( ! vfs_exec_on_screen ( screen, cwd, argv, NULL,
                                                vfs_file_info_get_disp_name( file ),
                                                VFS_EXEC_DEFAULT_FLAGS, &err ) )
                    {
                        char* msg = g_markup_escape_text(err->message, -1);
                        toplevel = file_browser ? gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ) : NULL;
                        ptk_show_error( ( GtkWindow* ) toplevel,
                                        _("Error"),
                                        msg );
                        g_free(msg);
                        g_error_free( err );
                    }
                    else
                    {
                        if ( G_LIKELY( file_browser ) )
                        {
                            ptk_file_browser_emit_open( file_browser,
                                                        full_path, PTK_OPEN_FILE );
                        }
                    }
                    g_free( full_path );
                    continue;
                }
                mime_type = vfs_file_info_get_mime_type( file );
                /* The file itself is a desktop entry file. */
                /*                if( g_str_has_suffix( vfs_file_info_get_name( file ), ".desktop" ) ) */
                if ( file->flags & VFS_FILE_INFO_DESKTOP_ENTRY )
                    app_desktop = full_path;
                else
                    app_desktop = vfs_mime_type_get_default_action( mime_type );

                if ( !app_desktop && mime_type_is_text_file( full_path, mime_type->type ) )
                {
                    /* FIXME: special handling for plain text file */
                    vfs_mime_type_unref( mime_type );
                    mime_type = vfs_mime_type_get_from_type( XDG_MIME_TYPE_PLAIN_TEXT );
                    app_desktop = vfs_mime_type_get_default_action( mime_type );
                }
                if ( !app_desktop )
                {
                    toplevel = file_browser ? gtk_widget_get_toplevel( GTK_WIDGET( file_browser ) ) : NULL;                    /* Let the user choose an application */
                    choosen_app = (char *) ptk_choose_app_for_mime_type(
                                      ( GtkWindow* ) toplevel,
                                      mime_type );
                    app_desktop = choosen_app;
                }
                if ( ! app_desktop )
                {
                    g_free( full_path );
                    continue;
                }

                files_to_open = NULL;
                if ( ! file_list_hash )
                    file_list_hash = g_hash_table_new_full( g_str_hash, g_str_equal, g_free, NULL );
                else
                    files_to_open = g_hash_table_lookup( file_list_hash, app_desktop );
                if ( app_desktop != full_path )
                    files_to_open = g_list_append( files_to_open, full_path );
                g_hash_table_replace( file_list_hash, app_desktop, files_to_open );
                app_desktop = NULL;
                vfs_mime_type_unref( mime_type );
            }
            else
            {
                files_to_open = g_list_append( files_to_open, full_path );
            }
        }
    }

    if ( file_list_hash )
    {
        g_hash_table_foreach( file_list_hash,
                              open_files_with_each_app, file_browser );
        g_hash_table_foreach( file_list_hash,
                              free_file_list_hash, NULL );
        g_hash_table_destroy( file_list_hash );
    }
    else if ( files_to_open && app_desktop )
    {
        open_files_with_app( cwd, files_to_open,
                             app_desktop, file_browser );
        g_list_foreach( files_to_open, ( GFunc ) g_free, NULL );
        g_list_free( files_to_open );
    }

    if ( new_dir )
    {
        /*
        ptk_file_browser_chdir( file_browser, new_dir, PTK_FB_CHDIR_ADD_HISTORY );
        */
        if ( G_LIKELY( file_browser ) )
        {
            ptk_file_browser_emit_open( file_browser,
                                        full_path, PTK_OPEN_DIR );
        }

        g_free( new_dir );
    }
}
