#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>

#include "file-assoc-dlg.h"

#include "ptk-utils.h"
#include "vfs-app-desktop.h"
#include "vfs-mime-type.h"

enum{
    COL_MIME_ICON,
    COL_MIME_DESC,
/*  COL_MIME_TYPE, */
    N_COL_MIME
};

enum{
    COL_APP_ICON,
    COL_APP_NAME,
    COL_APP_EXEC,
    COL_APP_FILE,
    N_COL_APP
};

#if 0
/* These strings are useless, just used to cheat intl-tools */
char* translated_mime_types[]=
    {
        /* mime-type: application */
        N_("application"),

        /*mime-type: audio */
        N_("audio"),

        /* mime-type: video */
        N_("video"),

        /* mime-type: text */
        N_("text"),

        /*mime-type: multipart */
        N_("multipart"),

        /* mime-type: image */
        N_("image"),

        /* mime-type: message */
        N_("message"),

        /*mime-type: model */
        N_("model"),

        /*mime-type: inode */
        N_("inode"),

        /*mime-type: packages */
        N_("packages")
    };
#endif

static void add_mime_types_to_tree( GtkTreeStore* tree,
                                    const char* file_name,
                                    const char* dir_path,
                                    gboolean is_dir,
                                    GtkTreeIter* parent_it )
{
    GtkTreeIter it, child;
    VFSMimeType* mime_type;
    GDir* dir;
    char* name;
    char* type;
    GdkPixbuf* icon;

    gtk_tree_store_append( tree, &it, parent_it );
    if( is_dir )
    {
        mime_type = vfs_mime_type_get_from_type( XDG_MIME_TYPE_DIRECTORY );
        /* FIXME: prevent duplication */
        icon = vfs_mime_type_get_icon( mime_type, FALSE );
        gtk_tree_store_set( tree, &it,
                            COL_MIME_ICON, icon,
                            COL_MIME_DESC, _(file_name),
                            /*COL_MIME_TYPE, file_name,*/ -1 );
        if( icon )
            gdk_pixbuf_unref( icon );
        vfs_mime_type_unref( mime_type );
        dir = g_dir_open( dir_path, 0, NULL );
        if( dir )
        {
            while( (name = g_dir_read_name( dir )) )
            {
                if( ! g_str_has_suffix( name, ".xml" ) )
                    continue;
                name = g_strdup( name );
                name[ strlen(name) - 4 ] = '\0';
                type = g_strdup_printf( "%s/%s", file_name, name );
                add_mime_types_to_tree( tree, type, NULL, FALSE, &it );
                g_free( type );
                g_free( name );
            }
            g_dir_close( dir );
        }
    }
    else
    {
        mime_type = vfs_mime_type_get_from_type( file_name );
        if( ! mime_type )
            return;
        /* FIXME: prevent duplication */
        gtk_tree_store_set( tree, &it,
                            COL_MIME_ICON, vfs_mime_type_get_icon( mime_type, FALSE ),
                            COL_MIME_DESC, vfs_mime_type_get_description( mime_type ),
                            /*COL_MIME_TYPE, file_name,*/ -1 );
        vfs_mime_type_unref( mime_type );
    }
}

static void init_type_tree( GtkTreeView* view )
{
    GDir* dir_scan;
    const char* const * sys_dirs = g_get_system_data_dirs();
    const char** dir;
    const char** dirs;
    int len;
    char* dir_path;
    char* file_path;
    char* name;

    GtkTreeStore* tree;
    GtkTreeModel *sorted;
    GtkTreeViewColumn *col;
    GtkCellRenderer *prender, *render;

    tree = gtk_tree_store_new( N_COL_MIME,
                               GDK_TYPE_PIXBUF,
                               G_TYPE_STRING/*,
                               G_TYPE_STRING*/ );

    sorted = gtk_tree_model_sort_new_with_model( GTK_TREE_MODEL( tree ) );
    gtk_tree_sortable_set_sort_column_id( GTK_TREE_SORTABLE( sorted ), COL_MIME_DESC, GTK_SORT_ASCENDING );

    len = g_strv_length( sys_dirs );
    dirs = g_new0( const char*, len + 2 );
    memcpy( dirs, sys_dirs, sizeof( char* ) * len );
    dirs[ len ] = g_get_user_data_dir();

    for( dir = dirs; *dir; ++dir )
    {
        dir_path = g_build_filename( *dir, "mime", NULL );
        dir_scan = g_dir_open( dir_path, 0, NULL );
        if( dir_scan )
        {
            while( (name = g_dir_read_name( dir_scan )) )
            {
                file_path = g_build_filename( dir_path, name, NULL );
                if( g_file_test( file_path, G_FILE_TEST_IS_DIR ) )
                {
                    add_mime_types_to_tree( tree, name,
                                            file_path, TRUE, NULL );
                }
                g_free( file_path );
            }
            g_dir_close( dir_scan );
        }
        g_free( dir_path );
    }
    g_free( dirs );

    gtk_tree_view_set_model( view, sorted );

    col = gtk_tree_view_column_new();
    prender = gtk_cell_renderer_pixbuf_new();
    g_object_set( prender, "xalign", 0.0, NULL );
    gtk_tree_view_column_pack_start( col, prender, FALSE );
    gtk_tree_view_column_add_attribute(
        col, prender, "pixbuf", COL_MIME_ICON );
    render = gtk_cell_renderer_text_new();
    g_object_set( render, "xalign", 0.0, NULL );
    gtk_tree_view_column_pack_start( col, render, FALSE );
    gtk_tree_view_column_add_attribute(
        col, render, "text", COL_MIME_DESC );
    gtk_tree_view_append_column( view, col );
}

void edit_file_associations( GtkWindow* parent_win )
{
    GtkBuilder* builder = _gtk_builder_new_from_file( PACKAGE_UI_DIR "/file-assoc-dlg.ui", NULL );
    GtkWidget* dlg = (GtkWidget*)gtk_builder_get_object( builder, "file_assoc_dlg" );
    gtk_window_set_transient_for( GTK_WINDOW( dlg ), parent_win );

    /* not yet working */
    gtk_widget_set_sensitive( (GtkWidget*)gtk_builder_get_object( builder, "add_app" ), FALSE );
    gtk_widget_set_sensitive( (GtkWidget*)gtk_builder_get_object( builder, "add_custom" ), FALSE );
    gtk_widget_set_sensitive( (GtkWidget*)gtk_builder_get_object( builder, "edit" ), FALSE );
    gtk_widget_set_sensitive( (GtkWidget*)gtk_builder_get_object( builder, "remove" ), FALSE );
    gtk_widget_set_sensitive( (GtkWidget*)gtk_builder_get_object( builder, "up" ), FALSE );
    gtk_widget_set_sensitive( (GtkWidget*)gtk_builder_get_object( builder, "down" ), FALSE );

    gtk_widget_set_size_request( (GtkWidget*)gtk_builder_get_object( builder, "vbox3" ), 215, -1 );
    gtk_widget_set_size_request( (GtkWidget*)gtk_builder_get_object( builder, "scrolledwindow2" ), 275, -1 );

    GtkWidget* types = (GtkWidget*)gtk_builder_get_object( builder, "types" );
    init_type_tree( (GtkTreeView*)types );

    gtk_dialog_run( GTK_DIALOG( dlg ) );
    gtk_widget_destroy( dlg );
}
