/*
*  C Implementation: file_properties
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "private.h"

#include <gtk/gtk.h>
#include "glib-mem.h"

#include "ptk-file-properties.h"

#include "mime-type/mime-type.h"

#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <string.h>

#include "ptk-file-task.h"
#include "ptk-utils.h"

#include "vfs-file-info.h"
#include "vfs-app-desktop.h"
#include "ptk-app-chooser.h"

const char* chmod_names[] =
    {
        "owner_r", "owner_w", "owner_x",
        "group_r", "group_w", "group_x",
        "others_r", "others_w", "others_x",
        "set_uid", "set_gid", "sticky"
    };

typedef struct
{
    char* dir_path;
    GList* file_list;
    GtkWidget* dlg;

    GtkEntry* owner;
    GtkEntry* group;
    char* owner_name;
    char* group_name;

    GtkToggleButton* chmod_btns[ N_CHMOD_ACTIONS ];
    guchar chmod_states[ N_CHMOD_ACTIONS ];

    GtkLabel* total_size_label;
    GtkLabel* size_on_disk_label;
    off_t total_size;
    off_t size_on_disk;
    guint total_count;
    gboolean cancel;
    gboolean done;
    GThread* calc_size_thread;
    guint update_label_timer;
}
FilePropertiesDialogData;

static void
on_dlg_response ( GtkDialog *dialog,
                                gint response_id,
                                gpointer user_data );

/*
* void get_total_size_of_dir( const char* path, off_t* size )
* Recursively count total size of all files in the specified directory.
* If the path specified is a file, the size of the file is directly returned.
* cancel is used to cancel the operation. This function will check the value
* pointed by cancel in every iteration. If cancel is set to TRUE, the
* calculation is cancelled.
* NOTE: path is encoded in on-disk encoding and not necessarily UTF-8.
*/
static void calc_total_size_of_files( const char* path, FilePropertiesDialogData* data )
{
    GDir * dir;
    const char* name;
    char* full_path;
    struct stat file_stat;

    if ( data->cancel )
        return ;

    if ( lstat( path, &file_stat ) )
        return ;

    data->total_size += file_stat.st_size;
    data->size_on_disk += ( file_stat.st_blocks << 9 ); /* block x 512 */
    ++data->total_count;

    dir = g_dir_open( path, 0, NULL );
    if ( dir )
    {
        while ( !data->cancel && ( name = g_dir_read_name( dir ) ) )
        {
            full_path = g_build_filename( path, name, NULL );
            lstat( full_path, &file_stat );
            if ( S_ISDIR( file_stat.st_mode ) )
            {
                calc_total_size_of_files( full_path, data );
            }
            else
            {
                data->total_size += file_stat.st_size;
                data->size_on_disk += ( file_stat.st_blocks << 9 );
                ++data->total_count;
            }
            g_free( full_path );
        }
        g_dir_close( dir );
    }
}

static gpointer calc_size( gpointer user_data )
{
    FilePropertiesDialogData * data = ( FilePropertiesDialogData* ) user_data;
    GList* l;
    char* path;
    VFSFileInfo* file;
    for ( l = data->file_list; l; l = l->next )
    {
        if ( data->cancel )
            break;
        file = ( VFSFileInfo* ) l->data;
        path = g_build_filename( data->dir_path,
                                 vfs_file_info_get_name( file ), NULL );
        if ( path )
        {
            calc_total_size_of_files( path, data );
            g_free( path );
        }
    }
    data->done = TRUE;
    return NULL;
}

gboolean on_update_labels( FilePropertiesDialogData* data )
{
    char buf[ 64 ];
    char buf2[ 32 ];

    gdk_threads_enter();

    vfs_file_size_to_string( buf2, data->total_size );
    sprintf( buf, "%s  (%llu Bytes)", buf2, ( guint64 ) data->total_size );
    gtk_label_set_text( data->total_size_label, buf );

    vfs_file_size_to_string( buf2, data->size_on_disk );
    sprintf( buf, "%s  (%llu Bytes)", buf2, ( guint64 ) data->size_on_disk );
    gtk_label_set_text( data->size_on_disk_label, buf );

    gdk_threads_leave();

    return !data->done;
}

static void on_chmod_btn_toggled( GtkToggleButton* btn,
                                  FilePropertiesDialogData* data )
{
    /* Bypass the default handler */
    g_signal_stop_emission_by_name( btn, "toggled" );
    /* Block this handler while we are changing the state of buttons,
      or this handler will be called recursively. */
    g_signal_handlers_block_matched( btn, G_SIGNAL_MATCH_FUNC, 0,
                                     0, NULL, on_chmod_btn_toggled, NULL );

    if ( gtk_toggle_button_get_inconsistent( btn ) )
    {
        gtk_toggle_button_set_inconsistent( btn, FALSE );
        gtk_toggle_button_set_active( btn, FALSE );
    }
    else if ( ! gtk_toggle_button_get_active( btn ) )
    {
        gtk_toggle_button_set_inconsistent( btn, TRUE );
    }

    g_signal_handlers_unblock_matched( btn, G_SIGNAL_MATCH_FUNC, 0,
                                       0, NULL, on_chmod_btn_toggled, NULL );
}

static gboolean combo_sep( GtkTreeModel *model,
                           GtkTreeIter* it,
                           gpointer user_data )
{
    int i;
    for( i = 2; i > 0; --i )
    {
        char* tmp;
        gtk_tree_model_get( model, it, i, &tmp, -1 );
        if( tmp )
        {
            g_free( tmp );
            return FALSE;
        }
    }
    return TRUE;
}

static void on_combo_change( GtkComboBox* combo, gpointer user_data )
{
    GtkTreeIter it;
    if( gtk_combo_box_get_active_iter(combo, &it) )
    {
        const char* action;
        GtkTreeModel* model = gtk_combo_box_get_model( combo );
        gtk_tree_model_get( model, &it, 2, &action, -1 );
        if( ! action )
        {
            char* action;
            GtkWidget* parent;
            VFSMimeType* mime = (VFSMimeType*)user_data;
            parent = gtk_widget_get_toplevel( GTK_WIDGET( combo ) );
            action = (char *) ptk_choose_app_for_mime_type( GTK_WINDOW(parent),
                                                   mime );
            if( action )
            {
                gboolean exist = FALSE;
                /* check if the action is already in the list */
                if( gtk_tree_model_get_iter_first( model, &it ) )
                {
                    do
                    {
                        char* tmp;
                        gtk_tree_model_get( model, &it, 2, &tmp, -1 );
                        if( !tmp )
                            continue;
                        if( 0 == strcmp( tmp, action ) )
                        {
                            exist = TRUE;
                            g_free( tmp );
                            break;
                        }
                        g_free( tmp );
                    } while( gtk_tree_model_iter_next( model, &it ) );
                }

                if( ! exist ) /* It didn't exist */
                {
                    VFSAppDesktop* app = vfs_app_desktop_new( action );
                    if( app )
                    {
                        GdkPixbuf* icon;
                        icon = vfs_app_desktop_get_icon( app, 20, TRUE );
                        gtk_list_store_insert_with_values(
                                            GTK_LIST_STORE( model ), &it, 0,
                                            0, icon,
                                            1, vfs_app_desktop_get_disp_name(app),
                                            2, action, -1 );
                        if( icon )
                            gdk_pixbuf_unref( icon );
                        vfs_app_desktop_unref( app );
                        exist = TRUE;
                    }
                }

                if( exist )
                    gtk_combo_box_set_active_iter( combo, &it );
                g_free( action );
            }
            else
            {
                int prev_sel;
                prev_sel = GPOINTER_TO_INT( g_object_get_data( G_OBJECT(combo), "prev_sel") );
                gtk_combo_box_set_active( combo, prev_sel );
            }
        }
        else
        {
            int prev_sel = gtk_combo_box_get_active( combo );
            g_object_set_data( G_OBJECT(combo), "prev_sel", GINT_TO_POINTER(prev_sel) );
        }
    }
    else
    {
        g_object_set_data( G_OBJECT(combo), "prev_sel", GINT_TO_POINTER(-1) );
    }
}

GtkWidget* file_properties_dlg_new( GtkWindow* parent,
                                    const char* dir_path,
                                    GList* sel_files )
{
    GtkBuilder* builder = _gtk_builder_new_from_file( PACKAGE_UI_DIR "/file-properties.ui", NULL );

    GtkWidget * dlg = (GtkWidget*)gtk_builder_get_object( builder, "dlg" );

    FilePropertiesDialogData* data;
    gboolean need_calc_size = TRUE;

    VFSFileInfo *file, *file2;
    VFSMimeType* mime;

    const char* multiple_files = _( "Multiple files are selected" );
    const char* calculating;
    GtkWidget* name = (GtkWidget*)gtk_builder_get_object( builder, "file_name" );
    GtkWidget* location = (GtkWidget*)gtk_builder_get_object( builder, "location" );
    GtkWidget* mime_type = (GtkWidget*)gtk_builder_get_object( builder, "mime_type" );
    GtkWidget* open_with = (GtkWidget*)gtk_builder_get_object( builder, "open_with" );

    GtkWidget* mtime = (GtkWidget*)gtk_builder_get_object( builder, "mtime" );
    GtkWidget* atime = (GtkWidget*)gtk_builder_get_object( builder, "atime" );

    char buf[ 64 ];
    char buf2[ 32 ];
    const char* time_format = "%Y-%m-%d %H:%M";

    gchar* disp_path;
    gchar* file_type;

    int i;
    GList* l;
    gboolean same_type = TRUE;
    char *owner_group, *tmp;

    g_object_set_data( G_OBJECT(dlg), "open_with", open_with );

    gtk_dialog_set_alternative_button_order( GTK_DIALOG(dlg), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL, -1 );
    gtk_window_set_transient_for( GTK_WINDOW( dlg ), parent );

    data = g_slice_new0( FilePropertiesDialogData );
    /* FIXME: When will the data be freed??? */
    g_object_set_data( G_OBJECT( dlg ), "DialogData", data );
    data->file_list = sel_files;
    data->dlg = dlg;

    data->dir_path = g_strdup( dir_path );
    disp_path = g_filename_display_name( dir_path );
    gtk_label_set_text( GTK_LABEL( location ), disp_path );
    g_free( disp_path );

    data->total_size_label = GTK_LABEL( (GtkWidget*)gtk_builder_get_object( builder, "total_size" ) );
    data->size_on_disk_label = GTK_LABEL( (GtkWidget*)gtk_builder_get_object( builder, "size_on_disk" ) );
    data->owner = GTK_ENTRY( (GtkWidget*)gtk_builder_get_object( builder, "owner" ) );
    data->group = GTK_ENTRY( (GtkWidget*)gtk_builder_get_object( builder, "group" ) );

    for ( i = 0; i < N_CHMOD_ACTIONS; ++i )
    {
        data->chmod_btns[ i ] = GTK_TOGGLE_BUTTON( (GtkWidget*)gtk_builder_get_object( builder, chmod_names[ i ] ) );
    }

    for ( l = sel_files; l && l->next; l = l->next )
    {
        VFSMimeType *type, *type2;
        file = ( VFSFileInfo* ) l->data;
        file2 = ( VFSFileInfo* ) l->next->data;
        type = vfs_file_info_get_mime_type( file );
        type2 = vfs_file_info_get_mime_type( file2 );
        if ( type != type2 )
        {
            vfs_mime_type_unref( type );
            vfs_mime_type_unref( type2 );
            same_type = FALSE;
            break;
        }
        vfs_mime_type_unref( type );
        vfs_mime_type_unref( type2 );
    }

    file = ( VFSFileInfo* ) sel_files->data;
    if ( same_type )
    {
        mime = vfs_file_info_get_mime_type( file );
        file_type = g_strdup_printf( "%s (%s)",
                                     vfs_mime_type_get_description( mime ),
                                     vfs_mime_type_get_type( mime ) );
        gtk_label_set_text( GTK_LABEL( mime_type ), file_type );
        g_free( file_type );
        vfs_mime_type_unref( mime );
    }
    else
    {
        gtk_label_set_text( GTK_LABEL( mime_type ), _( "Multiple files of different types" ) );
    }

    /* Open with...
     * Don't show this option menu if files of different types are selected,
     * ,the selected file is a folder, or its type is unknown.
     */
    if( ! same_type ||
          vfs_file_info_is_dir( file ) ||
          vfs_file_info_is_desktop_entry( file ) ||
          vfs_file_info_is_unknown_type( file ) ||
          vfs_file_info_is_executable( file, NULL ) )
    {
        /* if open with shouldn't show, destroy it. */
        gtk_widget_destroy( open_with );
        gtk_widget_destroy( (GtkWidget*)gtk_builder_get_object( builder, "open_with_label" ) );
    }
    else /* Add available actions to the option menu */
    {
        GtkTreeIter it;
        char **action, **actions;

        mime = vfs_file_info_get_mime_type( file );
        actions = vfs_mime_type_get_actions( mime );
        GtkCellRenderer* renderer;
        GtkListStore* model;
        gtk_cell_layout_clear( GTK_CELL_LAYOUT(open_with) );
        renderer = gtk_cell_renderer_pixbuf_new();
        gtk_cell_layout_pack_start( GTK_CELL_LAYOUT(open_with), renderer, FALSE);
        gtk_cell_layout_set_attributes( GTK_CELL_LAYOUT(open_with), renderer,
                                        "pixbuf", 0, NULL );
        renderer = gtk_cell_renderer_text_new();
        gtk_cell_layout_pack_start( GTK_CELL_LAYOUT(open_with), renderer, TRUE);
        gtk_cell_layout_set_attributes( GTK_CELL_LAYOUT(open_with),renderer,
                                        "text", 1, NULL );
        model = gtk_list_store_new( 3, GDK_TYPE_PIXBUF,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING );
        if( actions )
        {
            for( action = actions; *action; ++action )
            {
                VFSAppDesktop* desktop;
                GdkPixbuf* icon;
                desktop = vfs_app_desktop_new( *action );
                gtk_list_store_append( model, &it );
                icon = vfs_app_desktop_get_icon(desktop, 20, TRUE);
                gtk_list_store_set( model, &it,
                                    0, icon,
                                    1, vfs_app_desktop_get_disp_name(desktop),
                                    2, *action, -1 );
                if( icon )
                    gdk_pixbuf_unref( icon );
                vfs_app_desktop_unref( desktop );
            }
        }
        else
        {
            g_object_set_data( G_OBJECT(open_with), "prev_sel", GINT_TO_POINTER(-1) );
        }

        /* separator */
        gtk_list_store_append( model, &it );

        gtk_list_store_append( model, &it );
        gtk_list_store_set( model, &it,
                            0, NULL,
                            1, _("Other program..."), -1 );
        gtk_combo_box_set_model( GTK_COMBO_BOX(open_with),
                                 GTK_TREE_MODEL(model) );
        gtk_combo_box_set_row_separator_func(
                GTK_COMBO_BOX(open_with), combo_sep,
                NULL, NULL );
        gtk_combo_box_set_active(GTK_COMBO_BOX(open_with), 0);
        g_signal_connect( open_with, "changed",
                          G_CALLBACK(on_combo_change), mime );

        /* vfs_mime_type_unref( mime ); */
        /* We can unref mime when combo box gets destroyed */
        g_object_weak_ref( G_OBJECT(open_with),
                           (GWeakNotify)vfs_mime_type_unref, mime );
    }

    /* Multiple files are selected */
    if ( sel_files && sel_files->next )
    {
        gtk_widget_set_sensitive( name, FALSE );
        gtk_entry_set_text( GTK_ENTRY( name ), multiple_files );

        gtk_label_set_text( GTK_LABEL( mtime ), multiple_files );
        gtk_label_set_text( GTK_LABEL( atime ), multiple_files );

        for ( i = 0; i < N_CHMOD_ACTIONS; ++i )
        {
            gtk_toggle_button_set_inconsistent ( data->chmod_btns[ i ], TRUE );
            data->chmod_states[ i ] = 2; /* Don't touch this bit */
            g_signal_connect( G_OBJECT( data->chmod_btns[ i ] ), "toggled",
                              G_CALLBACK( on_chmod_btn_toggled ), data );
        }
    }
    else
    {
        /* special processing for files with special display names */
        if( vfs_file_info_is_desktop_entry( file ) )
        {
            char* disp_name = g_filename_display_name( file->name );
            gtk_entry_set_text( GTK_ENTRY( name ),
                                disp_name );
            g_free( disp_name );
        }
        else
            gtk_entry_set_text( GTK_ENTRY( name ),
                                vfs_file_info_get_disp_name( file ) );

        gtk_editable_set_editable ( GTK_EDITABLE( name ), FALSE );

        if ( ! vfs_file_info_is_dir( file ) )
        {
            /* Only single "file" is selected, so we don't need to
                caculate total file size */
            need_calc_size = FALSE;

            sprintf( buf, "%s  (%llu Bytes)",
                     vfs_file_info_get_disp_size( file ),
                     ( guint64 ) vfs_file_info_get_size( file ) );
            gtk_label_set_text( data->total_size_label, buf );

            vfs_file_size_to_string( buf2,
                                 vfs_file_info_get_blocks( file ) * 512 );
            sprintf( buf, "%s  (%llu Bytes)", buf2,
                     ( guint64 ) vfs_file_info_get_blocks( file ) * 512 );
            gtk_label_set_text( data->size_on_disk_label, buf );
        }
        gtk_label_set_text( GTK_LABEL( mtime ),
                            vfs_file_info_get_disp_mtime( file ) );

        strftime( buf, sizeof( buf ),
                  time_format, localtime( vfs_file_info_get_atime( file ) ) );
        gtk_label_set_text( GTK_LABEL( atime ), buf );

        owner_group = (char *) vfs_file_info_get_disp_owner( file );
        tmp = strchr( owner_group, ':' );
        data->owner_name = g_strndup( owner_group, tmp - owner_group );
        gtk_entry_set_text( GTK_ENTRY( data->owner ), data->owner_name );
        data->group_name = g_strdup( tmp + 1 );
        gtk_entry_set_text( GTK_ENTRY( data->group ), data->group_name );

        for ( i = 0; i < N_CHMOD_ACTIONS; ++i )
        {
            if ( data->chmod_states[ i ] != 2 ) /* allow to touch this bit */
            {
                data->chmod_states[ i ] = ( vfs_file_info_get_mode( file ) & chmod_flags[ i ] ? 1 : 0 );
                gtk_toggle_button_set_active( data->chmod_btns[ i ], data->chmod_states[ i ] );
            }
        }
    }

    if ( need_calc_size )
    {
        /* The total file size displayed in "File Properties" is not
           completely calculated yet. So "Calculating..." is displayed. */
        calculating = _( "Calculating..." );
        gtk_label_set_text( data->total_size_label, calculating );
        gtk_label_set_text( data->size_on_disk_label, calculating );

        g_object_set_data( G_OBJECT( dlg ), "calc_size", data );
        data->calc_size_thread = g_thread_create ( ( GThreadFunc ) calc_size,
                                                   data, TRUE, NULL );
        data->update_label_timer = g_timeout_add( 250,
                                                  ( GSourceFunc ) on_update_labels,
                                                  data );
    }

    g_signal_connect( dlg, "response",
                        G_CALLBACK(on_dlg_response), dlg );
    g_signal_connect_swapped( gtk_builder_get_object(builder, "ok_button"),
                        "clicked",
                        G_CALLBACK(gtk_widget_destroy), dlg );
    g_signal_connect_swapped( gtk_builder_get_object(builder, "cancel_button"),
                        "clicked",
                        G_CALLBACK(gtk_widget_destroy), dlg );

    ptk_dialog_fit_small_screen( GTK_DIALOG(dlg) );

    g_object_unref( builder );

    return dlg;
}

static uid_t uid_from_name( const char* user_name )
{
    struct passwd * pw;
    uid_t uid = -1;
    const char* p;

    pw = getpwnam( user_name );
    if ( pw )
    {
        uid = pw->pw_uid;
    }
    else
    {
        uid = 0;
        for ( p = user_name; *p; ++p )
        {
            if ( !g_ascii_isdigit( *p ) )
                return -1;
            uid *= 10;
            uid += ( *p - '0' );
        }
#if 0 /* This is not needed */
        /* Check the existance */
        pw = getpwuid( uid );
        if ( !pw )     /* Invalid uid */
            return -1;
#endif

    }
    return uid;
}

gid_t gid_from_name( const char* group_name )
{
    struct group * grp;
    gid_t gid = -1;
    const char* p;

    grp = getgrnam( group_name );
    if ( grp )
    {
        gid = grp->gr_gid;
    }
    else
    {
        gid = 0;
        for ( p = group_name; *p; ++p )
        {
            if ( !g_ascii_isdigit( *p ) )
                return -1;
            gid *= 10;
            gid += ( *p - '0' );
        }
#if 0 /* This is not needed */
        /* Check the existance */
        grp = getgrgid( gid );
        if ( !grp )     /* Invalid gid */
            return -1;
#endif

    }
    return gid;
}

void
on_dlg_response ( GtkDialog *dialog,
                                gint response_id,
                                gpointer user_data )
{
    FilePropertiesDialogData * data;
    PtkFileTask* task;
    gboolean mod_change;
    uid_t uid = -1;
    gid_t gid = -1;
    const char* owner_name;
    const char* group_name;
    int i;
    GList* l;
    GList* file_list;
    char* file_path;
    GtkWidget* ask_recursive;
    VFSFileInfo* file;

    data = ( FilePropertiesDialogData* ) g_object_get_data( G_OBJECT( dialog ),
                                                            "DialogData" );
    if ( data )
    {
        if ( data->update_label_timer )
            g_source_remove( data->update_label_timer );
        data->cancel = TRUE;

        if ( data->calc_size_thread )
            g_thread_join( data->calc_size_thread );

        if ( response_id == GTK_RESPONSE_OK )
        {
            GtkWidget* open_with;

            /* Set default action for mimetype */
            if( ( open_with = (GtkWidget*)g_object_get_data( G_OBJECT(dialog), "open_with" ) ) )
            {
                GtkTreeModel* model = gtk_combo_box_get_model( GTK_COMBO_BOX(open_with) );
                GtkTreeIter it;

                if( model && gtk_combo_box_get_active_iter( GTK_COMBO_BOX(open_with), &it ) )
                {
                    char* action;
                    gtk_tree_model_get( model, &it, 2, &action, -1 );
                    if( action )
                    {
                        file = ( VFSFileInfo* ) data->file_list->data;
                        VFSMimeType* mime = vfs_file_info_get_mime_type( file );
                        vfs_mime_type_set_default_action( mime, action );
                        vfs_mime_type_unref( mime );
                        g_free( action );
                    }
                }
            }

            /* Check if we need chown */
            owner_name = gtk_entry_get_text( data->owner );
            if ( owner_name && *owner_name &&
                 (!data->owner_name || strcmp( owner_name, data->owner_name )) )
            {
                uid = uid_from_name( owner_name );
                if ( uid == -1 )
                {
                    ptk_show_error( GTK_WINDOW( dialog ), _("Error"), _( "Invalid User" ) );
                    return ;
                }
            }
            group_name = gtk_entry_get_text( data->group );
            if ( group_name && *group_name &&
                 (!data->group_name || strcmp( group_name, data->group_name )) )
            {
                gid = gid_from_name( group_name );
                if ( gid == -1 )
                {
                    ptk_show_error( GTK_WINDOW( dialog ), _("Error"), _( "Invalid Group" ) );
                    return ;
                }
            }

            for ( i = 0; i < N_CHMOD_ACTIONS; ++i )
            {
                if ( gtk_toggle_button_get_inconsistent( data->chmod_btns[ i ] ) )
                {
                    data->chmod_states[ i ] = 2;  /* Don't touch this bit */
                }
                else if ( data->chmod_states[ i ] != gtk_toggle_button_get_active( data->chmod_btns[ i ] ) )
                {
                    mod_change = TRUE;
                    data->chmod_states[ i ] = gtk_toggle_button_get_active( data->chmod_btns[ i ] );
                }
                else /* Don't change this bit */
                {
                    data->chmod_states[ i ] = 2;
                }
            }

            if ( uid != -1 || gid != -1 || mod_change )
            {
                file_list = NULL;
                for ( l = data->file_list; l; l = l->next )
                {
                    file = ( VFSFileInfo* ) l->data;
                    file_path = g_build_filename( data->dir_path,
                            vfs_file_info_get_name( file ), NULL );
                    file_list = g_list_prepend( file_list, file_path );
                }

                task = ptk_file_task_new( VFS_FILE_TASK_CHMOD_CHOWN,
                                          file_list,
                                          NULL,
                                          GTK_WINDOW(gtk_widget_get_parent( GTK_WIDGET( dialog ) )) );

                for ( l = data->file_list; l; l = l->next )
                {
                    file = ( VFSFileInfo* ) l->data;
                    if ( vfs_file_info_is_dir( file ) )
                    {
                        ask_recursive = gtk_message_dialog_new(
                                            GTK_WINDOW( data->dlg ),
                                            GTK_DIALOG_MODAL,
                                            GTK_MESSAGE_QUESTION,
                                            GTK_BUTTONS_YES_NO,
                                            _( "Do you want to recursively apply these changes to all files and sub-folders?" ) );
                        ptk_file_task_set_recursive( task,
                                ( GTK_RESPONSE_YES == gtk_dialog_run( GTK_DIALOG( ask_recursive ) ) ) );
                        gtk_widget_destroy( ask_recursive );
                        break;
                    }
                }

                if ( mod_change )
                {
                     /* If the permissions of file has been changed by the user */
                    ptk_file_task_set_chmod( task, data->chmod_states );
                }
                /* For chown */
                ptk_file_task_set_chown( task, uid, gid );
                ptk_file_task_run( task );

                /*
                * This file list will be freed by file operation, so we don't
                * need to do this. Just set the pointer to NULL.
                */
                data->file_list = NULL;
            }
        }

        g_free( data->owner_name );
        g_free( data->group_name );
        /*
         *NOTE: File operation chmod/chown will free the list when it's done,
         *and we only need to free it when there is no file operation applyed.
        */
        g_slice_free( FilePropertiesDialogData, data );
    }

    gtk_widget_destroy( GTK_WIDGET( dialog ) );
}

