/*
*  C Implementation: ptk-file-task
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#include <glib.h>
#include <glib/gi18n.h>
#include "glib-mem.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "ptk-file-task.h"
#include "ptk-utils.h"

static gboolean open_up_progress_dlg( PtkFileTask* task );

static void on_vfs_file_task_progress_cb( VFSFileTask* task,
                                          int percent,
                                          const char* src_file,
                                          const char* dest_file,
                                          gpointer user_data );

static gboolean on_vfs_file_task_state_cb( VFSFileTask* task,
                                           VFSFileTaskState state,
                                           gpointer state_data,
                                           gpointer user_data );

static gboolean query_overwrite( PtkFileTask* task, char** new_dest );

PtkFileTask* ptk_file_task_new( VFSFileTaskType type,
                                GList* src_files,
                                const char* dest_dir,
                                GtkWindow* parent_window )
{
    PtkFileTask * task = g_slice_new0( PtkFileTask );
    task->task = vfs_task_new( type, src_files, dest_dir );
    vfs_file_task_set_progress_callback( task->task,
                                         on_vfs_file_task_progress_cb,
                                         task );
    vfs_file_task_set_state_callback( task->task,
                                      on_vfs_file_task_state_cb, task );
    task->parent_window = parent_window;
    return task;
}

void ptk_file_task_destroy( PtkFileTask* task )
{
    if ( task->task )
        vfs_file_task_free( task->task );
    if ( task->progress_dlg )
        gtk_widget_destroy( task->progress_dlg );
    if ( task->timeout )
        g_source_remove( task->timeout );
    g_slice_free( PtkFileTask, task );
}

void ptk_file_task_set_complete_notify( PtkFileTask* task,
                                        GFunc callback,
                                        gpointer user_data )
{
    task->complete_notify = callback;
    task->user_data = user_data;
}

void ptk_file_task_run( PtkFileTask* task )
{
    task->timeout = g_timeout_add( 500,
                                   ( GSourceFunc ) open_up_progress_dlg, task );
    vfs_file_task_run( task->task );
}

void ptk_file_task_cancel( PtkFileTask* task )
{
    vfs_file_task_abort( task->task );
}

void on_progress_dlg_response( GtkDialog* dlg, int response, PtkFileTask* task )
{
    switch ( response )
    {
    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_NONE:
        vfs_file_task_try_abort( task->task );
        break;
    }
}

gboolean open_up_progress_dlg( PtkFileTask* task )
{
    GtkTable* table;
    GtkLabel* label;

    const char * actions[] =
        {
            N_( "Move: " ),
            N_( "Copy: " ),
            N_( "Move to Trash: " ),
            N_( "Delete: " ),
            N_( "Link: " ),
            N_( "Change Properties: " )
        };
    const char* titles[] =
        {
            N_( "Moving..." ),
            N_( "Copying..." ),
            N_( "Moving to Trash..." ),
            N_( "Deleting..." ),
            N_( "Linking..." ),
            N_( "Changing Properties" )
        };

    gdk_threads_enter();

    g_source_remove( task->timeout );
    task->timeout = 0;

    task->progress_dlg = gtk_dialog_new_with_buttons(
                             _( titles[ task->task->type ] ),
                             task->parent_window,
                             0, GTK_STOCK_CANCEL,
                             GTK_RESPONSE_CANCEL, NULL );

    table = GTK_TABLE(gtk_table_new( 4, 2, FALSE ));
    gtk_container_set_border_width( GTK_CONTAINER ( table ), 4 );
    gtk_table_set_row_spacings( table, 4 );
    gtk_table_set_col_spacings( table, 4 );

    /* From: */
    label = GTK_LABEL(gtk_label_new( _( actions[ task->task->type ] ) ));
    gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0.5 );
    gtk_table_attach( table,
                      GTK_WIDGET(label),
                      0, 1, 0, 1, GTK_FILL, 0, 0, 0 );
    task->from = GTK_LABEL(gtk_label_new( task->task->current_file ));
    gtk_misc_set_alignment( GTK_MISC ( task->from ), 0, 0.5 );
    gtk_label_set_ellipsize( task->from, PANGO_ELLIPSIZE_MIDDLE );
    gtk_table_attach( table,
                      GTK_WIDGET( task->from ),
                      1, 2, 0, 1, GTK_FILL, 0, 0, 0 );

    /* To: */
    if ( task->task->dest_dir )
    {
        /* To: <Destination folder>
        ex. Copy file to..., Move file to...etc. */
        label = GTK_LABEL(gtk_label_new( _( "To:" ) ));
        gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0.5 );
        gtk_table_attach( table,
                          GTK_WIDGET(label),
                          0, 1, 1, 2, GTK_FILL, 0, 0, 0 );
        task->to = GTK_LABEL(gtk_label_new( task->task->dest_dir ));
        gtk_misc_set_alignment( GTK_MISC ( task->to ), 0, 0.5 );
        gtk_label_set_ellipsize( task->to, PANGO_ELLIPSIZE_MIDDLE );
        gtk_table_attach( table,
                          GTK_WIDGET( task->to ),
                          1, 2, 1, 2, GTK_FILL, 0, 0, 0 );
    }

    /* Processing: */
    /* Processing: <Name of currently proccesed file> */
    label = GTK_LABEL(gtk_label_new( _( "Processing:" ) ));
    gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0.5 );
    gtk_table_attach( table,
                      GTK_WIDGET(label),
                      0, 1, 2, 3, GTK_FILL, 0, 0, 0 );

    /* Preparing to do some file operation (Copy, Move, Delete...) */
    task->current = GTK_LABEL(gtk_label_new( _( "Preparing..." ) ));
    gtk_label_set_ellipsize( task->current, PANGO_ELLIPSIZE_MIDDLE );
    gtk_misc_set_alignment( GTK_MISC ( task->current ), 0, 0.5 );
    gtk_table_attach( table,
                      GTK_WIDGET( task->current ),
                      1, 2, 2, 3, GTK_FILL, 0, 0, 0 );

    /* Progress: */
    label = GTK_LABEL(gtk_label_new( _( "Progress:" ) ));
    gtk_misc_set_alignment( GTK_MISC ( label ), 0, 0.5 );
    gtk_table_attach( table,
                      GTK_WIDGET(label),
                      0, 1, 3, 4, GTK_FILL, 0, 0, 0 );
    task->progress = GTK_PROGRESS_BAR(gtk_progress_bar_new());
    gtk_table_attach( table,
                      GTK_WIDGET( task->progress ),
                      1, 2, 3, 4, GTK_FILL | GTK_EXPAND, 0, 0, 0 );

    gtk_box_pack_start( GTK_BOX( GTK_DIALOG( task->progress_dlg ) ->vbox ),
                        GTK_WIDGET( table ),
                        TRUE, TRUE, 0 );

    gtk_window_set_default_size( GTK_WINDOW( task->progress_dlg ),
                                 400, -1 );
    gtk_button_box_set_layout ( GTK_BUTTON_BOX ( GTK_DIALOG( task->progress_dlg ) ->action_area ),
                                GTK_BUTTONBOX_END );
    gtk_window_set_type_hint ( GTK_WINDOW ( task->progress_dlg ),
                               GDK_WINDOW_TYPE_HINT_DIALOG );
    gtk_window_set_gravity ( GTK_WINDOW ( task->progress_dlg ),
                             GDK_GRAVITY_NORTH_EAST );

    g_signal_connect( task->progress_dlg, "response",
                      G_CALLBACK( on_progress_dlg_response ), task );
    gtk_widget_show_all( task->progress_dlg );

    gdk_threads_leave();

    return TRUE;
}

void ptk_file_task_set_chmod( PtkFileTask* task,
                              guchar* chmod_actions )
{
    vfs_file_task_set_chmod( task->task, chmod_actions );
}

void ptk_file_task_set_chown( PtkFileTask* task,
                              uid_t uid, gid_t gid )
{
    vfs_file_task_set_chown( task->task, uid, gid );
}

void ptk_file_task_set_recursive( PtkFileTask* task, gboolean recursive )
{
    vfs_file_task_set_recursive( task->task, recursive );
}

void on_vfs_file_task_progress_cb( VFSFileTask* task,
                                   int percent,
                                   const char* src_file,
                                   const char* dest_file,
                                   gpointer user_data )
{
    PtkFileTask * data = ( PtkFileTask* ) user_data;
    char* ufile_path;
    char percent_str[ 16 ];

    if ( ! data->progress_dlg )
        return ;

    gdk_threads_enter();

    /* update current src file */
    if ( data->old_src_file != src_file )
    {
        data->old_src_file = src_file;
        ufile_path = g_filename_display_name( src_file );
        gtk_label_set_text( data->current, ufile_path );
        g_free( ufile_path );
    }

    /* update current dest file */
    if ( data->old_dest_file != dest_file )
    {
        data->old_dest_file = dest_file;
        ufile_path = g_filename_display_name( dest_file );
        gtk_label_set_text( data->to, ufile_path );
        g_free( ufile_path );
    }

    /* update progress */
    if ( data->old_percent != percent )
    {
        data->old_percent = percent;
        gtk_progress_bar_set_fraction( data->progress,
                                       ( ( gdouble ) percent ) / 100 );
        g_snprintf( percent_str, 16, "%d %%", percent );
        gtk_progress_bar_set_text( data->progress, percent_str );
    }

    gdk_threads_leave();
}

gboolean on_vfs_file_task_state_cb( VFSFileTask* task,
                                    VFSFileTaskState state,
                                    gpointer state_data,
                                    gpointer user_data )
{
    PtkFileTask* data = ( PtkFileTask* ) user_data;
    GtkWidget* dlg;
    int response;
    gboolean ret = TRUE;
    char** new_dest;

    gdk_threads_enter();

    switch ( state )
    {
    case VFS_FILE_TASK_FINISH:
        if ( data->timeout )
        {
            g_source_remove( data->timeout );
            data->timeout = 0;
        }
        if ( data->progress_dlg )
        {
            gtk_widget_destroy( data->progress_dlg );
            data->progress_dlg = NULL;
        }
        if ( data->complete_notify )
            data->complete_notify( task, data->user_data );
        vfs_file_task_free( data->task );
        data->task = NULL;
        ptk_file_task_destroy( data );
        break;
    case VFS_FILE_TASK_QUERY_OVERWRITE:
        new_dest = ( char** ) state_data;
        ret = query_overwrite( data, new_dest );
        break;
    case VFS_FILE_TASK_QUERY_ABORT:
        dlg = gtk_message_dialog_new( GTK_WINDOW( data->progress_dlg ),
                                      GTK_DIALOG_MODAL,
                                      GTK_MESSAGE_QUESTION,
                                      GTK_BUTTONS_YES_NO,
                                      _( "Cancel the operation?" ) );
        response = gtk_dialog_run( GTK_DIALOG( dlg ) );
        gtk_widget_destroy( dlg );
        ret = ( response != GTK_RESPONSE_YES );
        break;
    case VFS_FILE_TASK_ERROR:
        if( data->timeout )
        {
            g_source_remove( data->timeout );
            data->timeout = 0;
        }
        ptk_show_error( data->progress_dlg ? GTK_WINDOW(data->progress_dlg) : data->parent_window,
                        _("Error"),
                        state_data ? (char*)state_data : g_strerror( task->error ) );
        ret = FALSE;   /* abort */
        break;
    /* FIXME:
        VFS_FILE_TASK_RUNNING, VFS_FILE_TASK_ABORTED not handled */
    default:
        break;
    }

    gdk_threads_leave();

    return ret;    /* return TRUE to continue */
}


enum{
    RESPONSE_OVERWRITE = 1 << 0,
    RESPONSE_OVERWRITEALL = 1 << 1,
    RESPONSE_RENAME = 1 << 2,
    RESPONSE_SKIP = 1 << 3,
    RESPONSE_SKIPALL = 1 << 4
};

static void on_file_name_entry_changed( GtkEntry* entry, GtkDialog* dlg )
{
    const char * old_name;
    gboolean can_rename;
    const char* new_name = gtk_entry_get_text( entry );

    old_name = ( const char* ) g_object_get_data( G_OBJECT( entry ), "old_name" );
    can_rename = new_name && ( 0 != strcmp( new_name, old_name ) );

    gtk_dialog_set_response_sensitive ( dlg, RESPONSE_RENAME, can_rename );
    gtk_dialog_set_response_sensitive ( dlg, RESPONSE_OVERWRITE, !can_rename );
    gtk_dialog_set_response_sensitive ( dlg, RESPONSE_OVERWRITEALL, !can_rename );
}

gboolean query_overwrite( PtkFileTask* task, char** new_dest )
{
    const char * message;
    const char* question;
    GtkWidget* dlg;
    GtkWidget* parent_win;
    GtkEntry* entry;

    int response;
    gboolean has_overwrite_btn = TRUE;
    char* udest_file;
    char* file_name;
    char* ufile_name;
    char* dir_name;
    gboolean different_files, is_src_dir, is_dest_dir;
    struct stat src_stat, dest_stat;
    gboolean restart_timer = FALSE;

    different_files = ( 0 != strcmp( task->task->current_file,
                                     task->task->current_dest ) );

    lstat( task->task->current_file, &src_stat );
    lstat( task->task->current_dest, &dest_stat );

    is_src_dir = !!S_ISDIR( dest_stat.st_mode );
    is_dest_dir = !!S_ISDIR( src_stat.st_mode );

    if ( task->timeout )
    {
        g_source_remove( task->timeout );
        task->timeout = 0;
        restart_timer = TRUE;
    }

    if ( different_files && is_dest_dir == is_src_dir )
    {
        if ( is_dest_dir )
        {
            /* Ask the user whether to overwrite dir content or not */
            question = _( "Do you want to overwrite the folder and its content?" );
        }
        else
        {
            /* Ask the user whether to overwrite the file or not */
            question = _( "Do you want to overwrite this file?" );
        }
    }
    else
    { /* Rename is required */
        question = _( "Please choose a new file name." );
        has_overwrite_btn = FALSE;
    }

    if ( different_files )
    {
        /* Ths first %s is a file name, and the second one represents followed message.
        ex: "/home/pcman/some_file" already exists.\n\nDo you want to overwrite existing file?" */
        message = _( "\"%s\" already exists.\n\n%s" );
    }
    else
    {
        /* Ths first %s is a file name, and the second one represents followed message. */
        message = _( "\"%s\"\n\nDestination and source are the same file.\n\n%s" );
    }

    udest_file = g_filename_display_name( task->task->current_dest );
    parent_win = task->progress_dlg ? task->progress_dlg : GTK_WIDGET(task->parent_window);
    dlg = gtk_message_dialog_new( GTK_WINDOW( parent_win ),
                                  GTK_DIALOG_MODAL,
                                  GTK_MESSAGE_QUESTION,
                                  GTK_BUTTONS_NONE,
                                  message,
                                  udest_file,
                                  question );
    g_free( udest_file );

    if ( has_overwrite_btn )
    {
        gtk_dialog_add_buttons ( GTK_DIALOG( dlg ),
                                 _( "Overwrite" ), RESPONSE_OVERWRITE,
                                 _( "Overwrite All" ), RESPONSE_OVERWRITEALL,
                                 NULL );
    }

    gtk_dialog_add_buttons ( GTK_DIALOG( dlg ),
                             _( "Rename" ), RESPONSE_RENAME,
                             _( "Skip" ), RESPONSE_SKIP,
                             _( "Skip All" ), RESPONSE_SKIPALL,
                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                             NULL );

    file_name = g_path_get_basename( task->task->current_dest );
    ufile_name = g_filename_display_name( file_name );
    g_free( file_name );

    entry = ( GtkEntry* ) gtk_entry_new();
    g_object_set_data_full( G_OBJECT( entry ), "old_name",
                            ufile_name, g_free );
    g_signal_connect( G_OBJECT( entry ), "changed",
                      G_CALLBACK( on_file_name_entry_changed ), dlg );

    gtk_entry_set_text( entry, ufile_name );

    gtk_box_pack_start( GTK_BOX( GTK_DIALOG( dlg ) ->vbox ), GTK_WIDGET( entry ),
                        FALSE, TRUE, 4 );

    gtk_widget_show_all( dlg );
    response = gtk_dialog_run( GTK_DIALOG( dlg ) );

    switch ( response )
    {
    case RESPONSE_OVERWRITEALL:
        vfs_file_task_set_overwrite_mode( task->task, VFS_FILE_TASK_OVERWRITE_ALL );
        break;
    case RESPONSE_OVERWRITE:
        vfs_file_task_set_overwrite_mode( task->task, VFS_FILE_TASK_OVERWRITE );
        break;
    case RESPONSE_SKIPALL:
        vfs_file_task_set_overwrite_mode( task->task, VFS_FILE_TASK_SKIP_ALL );
        break;
    case RESPONSE_SKIP:
        vfs_file_task_set_overwrite_mode( task->task, VFS_FILE_TASK_SKIP );
        break;
    case RESPONSE_RENAME:
        vfs_file_task_set_overwrite_mode( task->task, VFS_FILE_TASK_RENAME );
        file_name = g_filename_from_utf8( gtk_entry_get_text( entry ),
                                          - 1, NULL, NULL, NULL );
        if ( file_name )
        {
            dir_name = g_path_get_dirname( task->task->current_dest );
            *new_dest = g_build_filename( dir_name, file_name, NULL );
            g_free( file_name );
            g_free( dir_name );
        }
        break;
    case GTK_RESPONSE_DELETE_EVENT: /* escape was pressed */
    case GTK_RESPONSE_CANCEL:
        vfs_file_task_abort( task->task );
        break;
    }
    gtk_widget_destroy( dlg );
    if( restart_timer )
    {
        task->timeout = g_timeout_add( 500,
                                    ( GSourceFunc ) open_up_progress_dlg,
                                    ( gpointer ) task );
    }
    return (response != GTK_RESPONSE_DELETE_EVENT)
        && (response != GTK_RESPONSE_CANCEL);
}

