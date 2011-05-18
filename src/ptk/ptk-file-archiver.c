/*
*  C Implementation: ptk-file-archiver
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include <glib/gi18n.h>
#include <string.h>

#include "ptk-file-archiver.h"
#include "ptk-console-output.h"

#include "vfs-file-info.h"
#include "vfs-mime-type.h"

typedef struct _ArchiveHandler
{
    const char* mime_type;
    const char* compress_cmd;
    const char* extract_cmd;
    const char* file_ext;
    gboolean multiple_files;
}
ArchiveHandler;

const ArchiveHandler handlers[]=
    {
        {
            "application/x-bzip-compressed-tar",
            "tar --bzip2 -cvf",
            "tar --bzip2 -xvf",
            ".tar.bz2", TRUE
        },
        {
            "application/x-compressed-tar",
            "tar -czvf",
            "tar -xzvf",
            ".tar.gz", TRUE
        },
        {
            "application/x-gzip",
            "gzip",
            "gunzip",
            ".gz", TRUE
        },
        {
            "application/zip",
            "zip -r",
            "unzip",
            ".zip", TRUE
        },
        {
            "application/x-tar",
            "tar -cvf",
            "tar -xvf",
            ".tar", TRUE
        },
        /*
        {
            "application/x-rar",
            NULL,
            "unrar -o- e",
            ".rar", TRUE
        }
        */
    };


static void on_format_changed( GtkComboBox* combo, gpointer user_data )
{
    int i, n, len;
    GtkFileChooser* dlg = GTK_FILE_CHOOSER(user_data);
    char* ext = NULL;
    char *path, *name, *new_name;

    path = gtk_file_chooser_get_filename( dlg );
    if( !path )
        return;
    ext = gtk_combo_box_get_active_text(combo);
    name = g_path_get_basename( path );
    g_free( path );
    n = gtk_tree_model_iter_n_children( gtk_combo_box_get_model(combo),
                                        NULL );
    for( i = 0; i < n; ++i )
    {
        if( g_str_has_suffix( name, handlers[i].file_ext ) )
            break;
    }
    if( i < n )
    {
        len = strlen( name ) - strlen( handlers[i].file_ext );
        name[len] = '\0';
    }
    new_name = g_strjoin( NULL, name, ext, NULL );
    g_free( name );
    g_free( ext );
    gtk_file_chooser_set_current_name( dlg, new_name );
    g_free( new_name );
}

void ptk_file_archiver_create( GtkWindow* parent_win,
                               const char* working_dir,
                               GList* files )
{
    GList* l;
    GtkWidget* dlg;
    GtkFileFilter* filter;
    char* dest_file;
    char* ext;
    int res;
    char **argv, **cmdv;
    int argc, cmdc, i, n;
    int format;
    GtkWidget* combo;
    GtkWidget* hbox;
    char* udest_file;
    char* desc;

    dlg = gtk_file_chooser_dialog_new( _("Save Compressed Files to..."),
                                       parent_win,
                                       GTK_FILE_CHOOSER_ACTION_SAVE,
                                       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                       GTK_STOCK_SAVE, GTK_RESPONSE_OK, NULL );
    filter = gtk_file_filter_new();
    hbox = gtk_hbox_new( FALSE, 4 );
    gtk_box_pack_start( GTK_BOX(hbox),
                        gtk_label_new( _("Format of compressed File:") ),
                        FALSE, FALSE, 2 );

    combo = gtk_combo_box_new_text();

    for( i = 0; i < G_N_ELEMENTS(handlers); ++i )
    {
        if( handlers[i].compress_cmd )
        {
            gtk_file_filter_add_mime_type( filter, handlers[i].mime_type );
            gtk_combo_box_append_text( GTK_COMBO_BOX(combo), handlers[i].file_ext );
        }
    }
    gtk_file_chooser_set_filter( GTK_FILE_CHOOSER(dlg), filter );
    g_signal_connect( combo, "changed", G_CALLBACK(on_format_changed), dlg );
    gtk_combo_box_set_active( GTK_COMBO_BOX(combo), 0 );
    gtk_box_pack_start( GTK_BOX(hbox),
                        combo,
                        FALSE, FALSE, 2 );
    gtk_widget_show_all( hbox );
    gtk_file_chooser_set_extra_widget( GTK_FILE_CHOOSER(dlg), hbox );

    gtk_file_chooser_set_action( GTK_FILE_CHOOSER(dlg), GTK_FILE_CHOOSER_ACTION_SAVE );
#if GTK_CHECK_VERSION(2, 8, 0)
    gtk_file_chooser_set_do_overwrite_confirmation( GTK_FILE_CHOOSER(dlg), TRUE );
#endif
    if( files && ! files->next )
    {
        ext = gtk_combo_box_get_active_text( GTK_COMBO_BOX(combo) );
        dest_file = g_strjoin( NULL,
                                vfs_file_info_get_disp_name( (VFSFileInfo*)files->data ),
                                ext, NULL );
        g_free( ext );
        gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER(dlg), dest_file );
        g_free( dest_file );
    }

    res = gtk_dialog_run(GTK_DIALOG(dlg));

    dest_file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dlg));
    format = gtk_combo_box_get_active( GTK_COMBO_BOX(combo) );
    gtk_widget_destroy( dlg );

    if( res != GTK_RESPONSE_OK )
    {
        g_free( dest_file );
        return;
    }

    g_shell_parse_argv( handlers[format].compress_cmd,
                        &cmdc, &cmdv, NULL );

    n = g_list_length( files );
    argc = cmdc + n + 1;
    argv = g_new0( char*, argc + 1 );

    for( i = 0; i < cmdc; ++i )
        argv[i] = cmdv[i];

    argv[i] = dest_file;
    ++i;

    for( l = files; l; l = l->next )
    {
        /* FIXME: Maybe we should consider filename encoding here. */
        argv[i] = (char *) vfs_file_info_get_name( (VFSFileInfo*) l->data );
        ++i;
    }
    argv[i] = NULL;

    udest_file = g_filename_display_name( dest_file );
    desc = g_strdup_printf( _("Creating Compressed File: %s"), udest_file );
    g_free( udest_file );

    ptk_console_output_run( parent_win, _("Compress Files"),
                            desc,
                            working_dir,
                            argc, argv );
    g_free( dest_file );
    g_strfreev( cmdv );
    g_free( argv );
}

void ptk_file_archiver_extract( GtkWindow* parent_win,
                                const char* working_dir,
                                GList* files,
                                const char* dest_dir )
{
    GtkWidget* dlg;
    char* _dest_dir = NULL;
    VFSFileInfo* file;
    VFSMimeType* mime;
    const char* type;
    char* full_path;
    int i, n;
    char **argv, **cmdv;
    int argc, cmdc;
    GList* l;
    char* desc;
    char* udest_dir;

    if( !files )
        return;
    if( !dest_dir )
    {
        dlg = gtk_file_chooser_dialog_new( _("Extract File to..."),
                                           parent_win,
                                           GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                           GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                           GTK_STOCK_OK, GTK_RESPONSE_OK, NULL );
        if( gtk_dialog_run( GTK_DIALOG(dlg) ) == GTK_RESPONSE_OK )
            _dest_dir = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(dlg) );
        gtk_widget_destroy( dlg );
        if( !_dest_dir )
            return;
        dest_dir = _dest_dir;
    }

    file = (VFSFileInfo*)files->data;
    mime = vfs_file_info_get_mime_type( file );
    type = vfs_mime_type_get_type( mime );

    for( i = 0; i < G_N_ELEMENTS(handlers); ++i )
    {
        if( 0 == strcmp( type, handlers[i].mime_type ) )
            break;
    }

    if( i < G_N_ELEMENTS(handlers) )    /* handler found */
    {
        g_shell_parse_argv( handlers[i].extract_cmd,
                            &cmdc, &cmdv, NULL );

        n = g_list_length( files );
        argc = cmdc + n;
        argv = g_new0( char*, argc + 1 );

        for( i = 0; i < cmdc; ++i )
            argv[i] = cmdv[i];

        for( l = files; l; l = l->next )
        {
            file = (VFSFileInfo*)l->data;
            full_path = g_build_filename( working_dir,
                                          vfs_file_info_get_name( file ),
                                          NULL );
            /* FIXME: Maybe we should consider filename encoding here. */
            argv[i] = full_path;
            ++i;
        }
        argv[i] = NULL;
        argc = i;

        udest_dir = g_filename_display_name( dest_dir );
        desc = g_strdup_printf( _("Extracting Files to: %s"), udest_dir );
        g_free( udest_dir );
        ptk_console_output_run( parent_win, _("Extract Files"),
                                desc,
                                dest_dir,
                                argc, argv );
        g_strfreev( cmdv );
        for( i = cmdc; i < argc; ++i )
            g_free( argv[i] );
        g_free( argv );
    }

    g_free( _dest_dir );
}

gboolean ptk_file_archiver_is_format_supported( VFSMimeType* mime,
                                                gboolean extract )
{
    int i;
    const char* type;

    if( !mime ) return FALSE;
    type = vfs_mime_type_get_type( mime );
    if(! type ) return FALSE;

    /* alias = mime_type_get_alias( type ); */

    for( i = 0; i < G_N_ELEMENTS(handlers); ++i )
    {
        if( 0 == strcmp( type, handlers[i].mime_type ) )
        {
            if( extract )
            {
                if( handlers[i].extract_cmd )
                    return TRUE;
            }
            else if( handlers[i].compress_cmd )
            {
                return TRUE;
            }
            break;
        }
    }
    return FALSE;
}
