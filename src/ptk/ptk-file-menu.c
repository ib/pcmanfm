/*
*  C Implementation: ptk-file-menu
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
#include <config.h>
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h> /* for access */

#include "ptk-file-menu.h"
#include <glib.h>
#include "glib-mem.h"
#include <string.h>

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "vfs-app-desktop.h"
#include "ptk-utils.h"
#include "ptk-file-misc.h"
#include "ptk-file-archiver.h"
#include "ptk-clipboard.h"
#include "ptk-app-chooser.h"

typedef struct _PtkFileMenu PtkFileMenu;
struct _PtkFileMenu
{
    PtkFileBrowser* browser;
    char* cwd;
    char* file_path;
    VFSFileInfo* info;
    GList* sel_files;
    GtkAccelGroup *accel_group;
};

#define get_toplevel_win(data)  ( (GtkWindow*) (data->browser ? ( gtk_widget_get_toplevel((GtkWidget*) data->browser) ) : NULL) )

/* Signal handlers for popup menu */
static void
on_popup_open_activate ( GtkMenuItem *menuitem,
                         PtkFileMenu* data );
static void
on_popup_open_with_another_activate ( GtkMenuItem *menuitem,
                                      PtkFileMenu* data );
#if 0
static void
on_file_properties_activate ( GtkMenuItem *menuitem,
                              PtkFileMenu* data );
#endif
static void
on_popup_run_app ( GtkMenuItem *menuitem,
                   PtkFileMenu* data );
static void
on_popup_open_in_new_tab_activate ( GtkMenuItem *menuitem,
                                    PtkFileMenu* data );
static void
on_popup_open_in_new_win_activate ( GtkMenuItem *menuitem,
                                    PtkFileMenu* data );
static void on_popup_open_in_terminal_activate( GtkMenuItem *menuitem,
                                                PtkFileMenu* data );
static void
on_popup_cut_activate ( GtkMenuItem *menuitem,
                        PtkFileMenu* data );
static void
on_popup_copy_activate ( GtkMenuItem *menuitem,
                         PtkFileMenu* data );
static void
on_popup_paste_activate ( GtkMenuItem *menuitem,
                          PtkFileMenu* data );
static void
on_popup_delete_activate ( GtkMenuItem *menuitem,
                           PtkFileMenu* data );
static void
on_popup_rename_activate ( GtkMenuItem *menuitem,
                           PtkFileMenu* data );
static void
on_popup_compress_activate ( GtkMenuItem *menuitem,
                             PtkFileMenu* data );
static void
on_popup_extract_here_activate ( GtkMenuItem *menuitem,
                                 PtkFileMenu* data );
static void
on_popup_extract_to_activate ( GtkMenuItem *menuitem,
                               PtkFileMenu* data );
static void
on_popup_new_folder_activate ( GtkMenuItem *menuitem,
                               PtkFileMenu* data );
static void
on_popup_new_text_file_activate ( GtkMenuItem *menuitem,
                                  PtkFileMenu* data );
static void
on_popup_file_properties_activate ( GtkMenuItem *menuitem,
                                    PtkFileMenu* data );

static PtkMenuItemEntry create_new_menu[] =
    {
        PTK_IMG_MENU_ITEM( N_( "_Folder" ), "gtk-directory", on_popup_new_folder_activate, 0, 0 ),
        PTK_IMG_MENU_ITEM( N_( "_Text File" ), "gtk-edit", on_popup_new_text_file_activate, 0, 0 ),
        PTK_MENU_END
    };

static PtkMenuItemEntry extract_menu[] =
    {
        PTK_MENU_ITEM( N_( "E_xtract Here" ), on_popup_extract_here_activate, 0, 0 ),
        PTK_IMG_MENU_ITEM( N_( "Extract _To" ), "gtk-directory", on_popup_extract_to_activate, 0, 0 ),
        PTK_MENU_END
    };

static PtkMenuItemEntry basic_popup_menu[] =
    {
        PTK_MENU_ITEM( N_( "Open _with..." ), NULL, 0, 0 ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_STOCK_MENU_ITEM( "gtk-cut", on_popup_cut_activate ),
        PTK_STOCK_MENU_ITEM( "gtk-copy", on_popup_copy_activate ),
        PTK_STOCK_MENU_ITEM( "gtk-paste", on_popup_paste_activate ),
        PTK_IMG_MENU_ITEM( N_( "_Delete" ), "gtk-delete", on_popup_delete_activate, GDK_Delete, 0 ),
        PTK_IMG_MENU_ITEM( N_( "_Rename" ), "gtk-edit", on_popup_rename_activate, GDK_F2, 0 ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_MENU_ITEM( N_( "Compress" ), on_popup_compress_activate, 0, 0 ),
        PTK_POPUP_MENU( N_( "E_xtract" ), extract_menu ),
        PTK_POPUP_IMG_MENU( N_( "_Create New" ), "gtk-new", create_new_menu ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_IMG_MENU_ITEM( N_( "_Properties" ), "gtk-info", on_popup_file_properties_activate, GDK_Return, GDK_MOD1_MASK ),
        PTK_MENU_END
    };

static PtkMenuItemEntry dir_popup_menu_items[] =
    {
        PTK_SEPARATOR_MENU_ITEM,
        PTK_MENU_ITEM( N_( "Open in New _Tab" ), on_popup_open_in_new_tab_activate, 0, 0 ),
        PTK_MENU_ITEM( N_( "Open in New _Window" ), on_popup_open_in_new_win_activate, 0, 0 ),
        PTK_IMG_MENU_ITEM( N_( "Open in Terminal" ), GTK_STOCK_EXECUTE, on_popup_open_in_terminal_activate, GDK_F4, 0 ),
        PTK_MENU_END
    };

#if 0
static gboolean same_file_type( GList* files )
{
    GList * l;
    VFSMimeType* mime_type;
    if ( ! files || ! files->next )
        return TRUE;
    mime_type = vfs_file_info_get_mime_type( ( VFSFileInfo* ) l->data );
    for ( l = files->next; l ; l = l->next )
    {
        VFSMimeType * mime_type2;
        mime_type2 = vfs_file_info_get_mime_type( ( VFSFileInfo* ) l->data );
        vfs_mime_type_unref( mime_type2 );
        if ( mime_type != mime_type2 )
        {
            vfs_mime_type_unref( mime_type );
            return FALSE;
        }
    }
    vfs_mime_type_unref( mime_type );
    return TRUE;
}
#endif

static void ptk_file_menu_free( PtkFileMenu *data )
{
    g_free( data->file_path );
    vfs_file_info_unref( data->info );
    g_free( data->cwd );
    vfs_file_info_list_free( data->sel_files );
    g_object_unref( data->accel_group );
    g_slice_free( PtkFileMenu, data );
}

/* Retrive popup menu for selected file(s) */
GtkWidget* ptk_file_menu_new( const char* file_path,
                              VFSFileInfo* info, const char* cwd,
                              GList* sel_files, PtkFileBrowser* browser )
{
    GtkWidget * popup = NULL;
    VFSMimeType* mime_type;

    GtkWidget *open;
    char* open_title;
    GtkWidget *open_with, *cut, *copy, *del, *rename, *open_with_menu, *open_with_another;
    GtkWidget *seperator, *top_separator = NULL;
    GtkWidget *create_new;
    GtkWidget *extract;
    GtkWidget *paste;
    GtkWidget *app_menu_item;
    gboolean is_dir, menu_for_cwd = FALSE;

    char **apps, **app;
    const char* app_name = NULL;
    const char* default_app_name = NULL;
    VFSAppDesktop* desktop_file;

    GdkPixbuf* app_icon, *open_icon = NULL;
    int icon_w, icon_h;
    GtkWidget* app_img;

    PtkFileMenu* data;
    int no_write_access = 1, no_read_access = 1;

    data = g_slice_new0( PtkFileMenu );

    menu_for_cwd =  (0 == strcmp( file_path, cwd )); /* menu for cwd */
    if( menu_for_cwd )
        data->cwd = g_path_get_dirname(cwd);
    else
        data->cwd = g_strdup( cwd );

    data->browser = browser;
    data->file_path = g_strdup( file_path );
    data->info = vfs_file_info_ref( info );
    data->sel_files = sel_files;

    data->accel_group = gtk_accel_group_new ();

    popup = gtk_menu_new ();
    g_object_weak_ref( G_OBJECT( popup ), (GWeakNotify) ptk_file_menu_free, data );
    g_signal_connect_after( ( gpointer ) popup, "selection-done",
                            G_CALLBACK ( gtk_widget_destroy ), NULL );

    /* Add some special menu items */
    mime_type = vfs_file_info_get_mime_type( info );

    if ( g_file_test( file_path, G_FILE_TEST_IS_DIR ) )
    {
        if( browser )
        {
            dir_popup_menu_items[0].ret = &top_separator;
            ptk_menu_add_items_from_data( popup, dir_popup_menu_items,
                                          data, data->accel_group );
        }
        is_dir = TRUE;
    }
    else
    {
        is_dir = FALSE;
    }

    basic_popup_menu[ 0 ].ret = &open_with;
    basic_popup_menu[ 2 ].ret = &cut;
    basic_popup_menu[ 3 ].ret = &copy;
    basic_popup_menu[ 4 ].ret = &paste;
    basic_popup_menu[ 5 ].ret = &del;
    basic_popup_menu[ 6 ].ret = &rename;
    basic_popup_menu[ 9 ].ret = &extract;
    basic_popup_menu[ 10 ].ret = &create_new;
    ptk_menu_add_items_from_data( popup, basic_popup_menu, data, data->accel_group );
    gtk_widget_show_all( GTK_WIDGET( popup ) );

#if defined(HAVE_EUIDACCESS)
    no_read_access = euidaccess( file_path, R_OK );
    no_write_access = euidaccess( file_path, W_OK );
#elif defined(HAVE_EACCESS)
    no_read_access = eaccess( file_path, R_OK );
    no_write_access = eaccess( file_path, W_OK );
#endif

    if( no_write_access )
    {
        /* have no write access to current file */
        gtk_widget_set_sensitive( cut, FALSE );
        gtk_widget_set_sensitive( paste, FALSE );
        gtk_widget_set_sensitive( del, FALSE );
        gtk_widget_set_sensitive( rename, FALSE );
        gtk_widget_set_sensitive( create_new, FALSE );
    }

    if( no_read_access )
    {
        /* FIXME:  Open should be disabled, too */
        gtk_widget_set_sensitive( copy, FALSE );
    }

    if ( ! is_dir )
    {
        gtk_widget_destroy( paste );
        gtk_widget_destroy( create_new );
    }
    else if( menu_for_cwd ) /* menu for cwd */
    {
        /* delete some confusing menu item if this menu is for current folder */
        gtk_widget_destroy( cut );
        gtk_widget_destroy( copy );
        gtk_widget_destroy( del );
        gtk_widget_destroy( rename );
    }

    /*  Add all of the apps  */
    open_with_menu = gtk_menu_new ();
    gtk_menu_item_set_submenu ( GTK_MENU_ITEM ( open_with ), open_with_menu );

    apps = vfs_mime_type_get_actions( mime_type );
    if ( vfs_file_info_is_text( info, file_path ) )     /* for text files */
    {
        char **tmp, **txt_apps;
        VFSMimeType* txt_type;
        int len1, len2;
        txt_type = vfs_mime_type_get_from_type( XDG_MIME_TYPE_PLAIN_TEXT );
        txt_apps = vfs_mime_type_get_actions( txt_type );
        if ( txt_apps )
        {
            len1 = apps ? g_strv_length( apps ) : 0;
            len2 = g_strv_length( txt_apps );
            tmp = apps;
            apps = vfs_mime_type_join_actions( apps, len1, txt_apps, len2 );
            g_strfreev( txt_apps );
            g_strfreev( tmp );
        }
        vfs_mime_type_unref( txt_type );
    }

    default_app_name = NULL;
    if ( apps )
    {
        for ( app = apps; *app; ++app )
        {
            if ( ( app - apps ) == 1 )  /* Add a separator after default app */
            {
                seperator = gtk_separator_menu_item_new ();
                gtk_widget_show ( seperator );
                gtk_container_add ( GTK_CONTAINER ( open_with_menu ), seperator );
            }
            desktop_file = vfs_app_desktop_new( *app );
            app_name = vfs_app_desktop_get_disp_name( desktop_file );
            if ( app_name )
            {
                app_menu_item = gtk_image_menu_item_new_with_label ( app_name );
                if ( app == apps )
                    default_app_name = app_name;
            }
            else
                app_menu_item = gtk_image_menu_item_new_with_label ( *app );

            g_object_set_data_full( G_OBJECT( app_menu_item ), "desktop_file",
                                    desktop_file, vfs_app_desktop_unref );

            gtk_icon_size_lookup_for_settings( gtk_settings_get_default(),
                                               GTK_ICON_SIZE_MENU,
                                               &icon_w, &icon_h );

            app_icon = vfs_app_desktop_get_icon( desktop_file,
                                                 icon_w > icon_h ? icon_w : icon_h, TRUE );
            if ( app_icon )
            {
                app_img = gtk_image_new_from_pixbuf( app_icon );
                gtk_image_menu_item_set_image ( GTK_IMAGE_MENU_ITEM( app_menu_item ), app_img );
                if ( app == apps )                                    /* Default app */
                    open_icon = app_icon;
                else
                    gdk_pixbuf_unref( app_icon );
            }
            gtk_container_add ( GTK_CONTAINER ( open_with_menu ), app_menu_item );
            g_signal_connect( G_OBJECT( app_menu_item ), "activate",
                              G_CALLBACK( on_popup_run_app ), ( gpointer ) data );
        }
        seperator = gtk_separator_menu_item_new ();
        gtk_container_add ( GTK_CONTAINER ( open_with_menu ), seperator );

        g_strfreev( apps );
    }

    open_with_another = gtk_menu_item_new_with_mnemonic ( _( "_Open with another program" ) );
    gtk_container_add ( GTK_CONTAINER ( open_with_menu ), open_with_another );
    g_signal_connect ( ( gpointer ) open_with_another, "activate",
                       G_CALLBACK ( on_popup_open_with_another_activate ),
                       data );

    gtk_widget_show_all( open_with_menu );

    /* Create Open menu item */
    open = NULL;
    if ( !is_dir )                                     /* Not a dir */
    {
        if ( vfs_file_info_is_executable( info, file_path )
                || info->flags & VFS_FILE_INFO_DESKTOP_ENTRY )
        {
            open = gtk_image_menu_item_new_with_mnemonic( _( "E_xecute" ) );
            app_img = gtk_image_new_from_stock( GTK_STOCK_EXECUTE, GTK_ICON_SIZE_MENU );
            gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM( open ), app_img );
        }
        else
        {
            /* FIXME: Only show default app name when all selected files have the same type. */
            if (     /* same_file_type( info, sel_files ) && */ default_app_name )
            {
                open_title = g_strdup_printf( _( "_Open with \"%s\"" ), default_app_name );
                open = gtk_image_menu_item_new_with_mnemonic( open_title );
                g_free( open_title );
                if ( open_icon )
                {
                    app_img = gtk_image_new_from_pixbuf( open_icon );
                    gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM( open ), app_img );
                }
            }
            else
            {
                open = gtk_image_menu_item_new_with_mnemonic( _( "_Open" ) );
            }
        }
    }
    else
    {
        if( menu_for_cwd ) /* not menu for cwd */
        {
            /* delete some confusing menu item if this menu is for current folder */
            open = gtk_image_menu_item_new_with_mnemonic( _( "_Open" ) );
            app_img = gtk_image_new_from_icon_name( "gnome-fs-directory", GTK_ICON_SIZE_MENU );
            gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM( open ), app_img );
        }
    }
    if( open )
    {
        gtk_widget_show( open );
        g_signal_connect( open, "activate", G_CALLBACK( on_popup_open_activate ), data );
        gtk_menu_shell_insert( GTK_MENU_SHELL( popup ), open, 0 );
    }
    else if( top_separator )
        gtk_widget_destroy( top_separator );

    if ( open_icon )
        gdk_pixbuf_unref( open_icon );

    /* Compress & Extract */
    if ( ! ptk_file_archiver_is_format_supported( mime_type, TRUE ) )
    {
        /* This is not a supported archive format */
        gtk_widget_destroy( extract );
    }

    vfs_mime_type_unref( mime_type );
    return popup;
}

void
on_popup_open_activate ( GtkMenuItem *menuitem,
                         PtkFileMenu* data )
{
    GList* sel_files = data->sel_files;
    if( ! sel_files )
        sel_files = g_list_prepend( sel_files, data->info );
    ptk_open_files_with_app( data->cwd, sel_files,
                             NULL, data->browser );
    if( sel_files != data->sel_files )
        g_list_free( sel_files );
}

void
on_popup_open_with_another_activate ( GtkMenuItem *menuitem,
                                      PtkFileMenu* data )
{
    char * app = NULL;
    PtkFileBrowser* browser = data->browser;
    VFSMimeType* mime_type;

    if ( data->info )
    {
        mime_type = vfs_file_info_get_mime_type( data->info );
        if ( G_LIKELY( ! mime_type ) )
        {
            mime_type = vfs_mime_type_get_from_type( XDG_MIME_TYPE_UNKNOWN );
        }
    }
    else
    {
        mime_type = vfs_mime_type_get_from_type( XDG_MIME_TYPE_DIRECTORY );
    }

    app = (char *) ptk_choose_app_for_mime_type( get_toplevel_win(data),  mime_type );
    if ( app )
    {
        GList* sel_files = data->sel_files;
        if( ! sel_files )
            sel_files = g_list_prepend( sel_files, data->info );
        ptk_open_files_with_app( data->cwd, sel_files,
                                 app, data->browser );
        if( sel_files != data->sel_files )
            g_list_free( sel_files );
        g_free( app );
    }
    vfs_mime_type_unref( mime_type );
}

void on_popup_run_app( GtkMenuItem *menuitem, PtkFileMenu* data )
{
    VFSAppDesktop * desktop_file;
    const char* app = NULL;
    GList* sel_files;

    desktop_file = ( VFSAppDesktop* ) g_object_get_data( G_OBJECT( menuitem ),
                                                         "desktop_file" );
    if ( !desktop_file )
        return ;

    app = vfs_app_desktop_get_name( desktop_file );
    sel_files = data->sel_files;
    if( ! sel_files )
        sel_files = g_list_prepend( sel_files, data->info );
    ptk_open_files_with_app( data->cwd, sel_files,
                             (char *) app, data->browser );
    if( sel_files != data->sel_files )
        g_list_free( sel_files );
}

void on_popup_open_in_new_tab_activate( GtkMenuItem *menuitem,
                                        PtkFileMenu* data )
{
    GList * sel;
    VFSFileInfo* file;
    char* full_path;

    if ( data->sel_files )
    {
        for ( sel = data->sel_files; sel; sel = sel->next )
        {
            file = ( VFSFileInfo* ) sel->data;
            full_path = g_build_filename( data->cwd,
                                          vfs_file_info_get_name( file ), NULL );
            if ( g_file_test( full_path, G_FILE_TEST_IS_DIR ) )
            {
                ptk_file_browser_emit_open( data->browser, full_path, PTK_OPEN_NEW_TAB );
            }
            g_free( full_path );
        }
    }
    else
    {
        ptk_file_browser_emit_open( data->browser, data->file_path, PTK_OPEN_NEW_TAB );
    }
}

void on_popup_open_in_terminal_activate( GtkMenuItem *menuitem,
                                         PtkFileMenu* data )
{
    ptk_file_browser_open_terminal( data->browser );
}

void on_popup_open_in_new_win_activate( GtkMenuItem *menuitem,
                                        PtkFileMenu* data )
{
    GList * sel;
    GList* sel_files = data->sel_files;
    VFSFileInfo* file;
    char* full_path;

    if ( sel_files )
    {
        for ( sel = sel_files; sel; sel = sel->next )
        {
            file = ( VFSFileInfo* ) sel->data;
            full_path = g_build_filename( data->cwd,
                                          vfs_file_info_get_name( file ), NULL );
            if ( g_file_test( full_path, G_FILE_TEST_IS_DIR ) )
            {
                ptk_file_browser_emit_open( data->browser, full_path, PTK_OPEN_NEW_WINDOW );
            }
            g_free( full_path );
        }
    }
    else
    {
        ptk_file_browser_emit_open( data->browser, data->file_path, PTK_OPEN_NEW_WINDOW );
    }
}

void
on_popup_cut_activate ( GtkMenuItem *menuitem,
                        PtkFileMenu* data )
{
    if ( data->sel_files )
        ptk_clipboard_cut_or_copy_files( data->cwd,
                                         data->sel_files, FALSE );
}

void
on_popup_copy_activate ( GtkMenuItem *menuitem,
                         PtkFileMenu* data )
{
    if ( data->sel_files )
        ptk_clipboard_cut_or_copy_files( data->cwd,
                                         data->sel_files, TRUE );
}

void
on_popup_paste_activate ( GtkMenuItem *menuitem,
                          PtkFileMenu* data )
{
    if ( data->sel_files )
    {
        char* dest_dir;
        GtkWidget* parent;
        parent = (GtkWidget*)get_toplevel_win( data );
        dest_dir = g_build_filename( data->cwd,
                                     vfs_file_info_get_name( data->info ), NULL );
        if( ! g_file_test( dest_dir, G_FILE_TEST_IS_DIR ) )
        {
            g_free( dest_dir );
            dest_dir = NULL;
        }
        ptk_clipboard_paste_files( GTK_WINDOW( parent ), dest_dir ? dest_dir : data->cwd );
    }
}

void
on_popup_delete_activate ( GtkMenuItem *menuitem,
                           PtkFileMenu* data )
{
    if ( data->sel_files )
    {
        GtkWidget* parent_win;
        parent_win = (GtkWidget*)get_toplevel_win( data );
        ptk_delete_files( GTK_WINDOW(parent_win),
                          data->cwd,
                          data->sel_files );
    }
}

void
on_popup_rename_activate ( GtkMenuItem *menuitem,
                           PtkFileMenu* data )
{
    if ( data->info )
    {
        GtkWidget* parent = (GtkWidget*)get_toplevel_win( data );
        ptk_rename_file( GTK_WINDOW(parent), data->cwd, data->info );
    }
}

void on_popup_compress_activate ( GtkMenuItem *menuitem,
                                  PtkFileMenu* data )
{
    ptk_file_archiver_create(
        GTK_WINDOW( get_toplevel_win( data ) ),
        data->cwd,
        data->sel_files );
}

void on_popup_extract_to_activate ( GtkMenuItem *menuitem,
                                    PtkFileMenu* data )
{
    ptk_file_archiver_extract(
        GTK_WINDOW( get_toplevel_win( data ) ),
        data->cwd, data->sel_files, NULL );
}

void on_popup_extract_here_activate ( GtkMenuItem *menuitem,
                                      PtkFileMenu* data )
{
    ptk_file_archiver_extract(
        GTK_WINDOW( get_toplevel_win( data ) ),
        data->cwd, data->sel_files, data->cwd );
}

static void
create_new_file( PtkFileMenu* data, gboolean create_dir )
{
    if ( data->cwd )
    {
        char* cwd;
        GtkWidget* parent;
        parent = (GtkWidget*)get_toplevel_win( data );
        if( data->file_path &&
            g_file_test( data->file_path, G_FILE_TEST_IS_DIR ) )
            cwd = data->file_path;
        else
            cwd = data->cwd;
        if( G_LIKELY(data->browser) )
            ptk_file_browser_create_new_file( data->browser, create_dir );
        else
            ptk_create_new_file( GTK_WINDOW( parent ),
                                 cwd, create_dir, NULL );
    }
}

void
on_popup_new_folder_activate ( GtkMenuItem *menuitem,
                               PtkFileMenu* data )
{
    create_new_file( data, TRUE );
}

void
on_popup_new_text_file_activate ( GtkMenuItem *menuitem,
                                  PtkFileMenu* data )
{
    create_new_file( data, FALSE );
}

void
on_popup_file_properties_activate ( GtkMenuItem *menuitem,
                                    PtkFileMenu* data )
{
    GtkWidget* parent;
    parent = (GtkWidget*)get_toplevel_win( data );
    ptk_show_file_properties( GTK_WINDOW( parent ),
                              data->cwd,
                              data->sel_files );
}
