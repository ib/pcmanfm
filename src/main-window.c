/*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "pcmanfm.h"
#include "private.h"

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>

#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include "ptk-file-browser.h"

#include "main-window.h"
#include "ptk-utils.h"

#include "pref-dialog.h"
#include "ptk-bookmarks.h"
#include "edit-bookmarks.h"
#include "ptk-file-properties.h"
#include "ptk-path-entry.h"

#include "settings.h"
#include "find-files.h"

#ifdef HAVE_STATVFS
/* FIXME: statvfs support should be moved to src/vfs */
#include <sys/statvfs.h>
#endif

#include "vfs-app-desktop.h"
#include "vfs-execute.h"
#include "vfs-utils.h"  /* for vfs_sudo() */
#include "go-dialog.h"

static void fm_main_window_class_init( FMMainWindowClass* klass );
static void fm_main_window_init( FMMainWindow* main_window );
static void fm_main_window_finalize( GObject *obj );
static void fm_main_window_get_property ( GObject *obj,
                                          guint prop_id,
                                          GValue *value,
                                          GParamSpec *pspec );
static void fm_main_window_set_property ( GObject *obj,
                                          guint prop_id,
                                          const GValue *value,
                                          GParamSpec *pspec );

static GtkWidget* create_bookmarks_menu ( FMMainWindow* main_window );
static GtkWidget* create_bookmarks_menu_item ( FMMainWindow* main_window,
                                               const char* item );

static gboolean fm_main_window_delete_event ( GtkWidget *widget,
                                              GdkEvent *event );
#if 0
static void fm_main_window_destroy ( GtkObject *object );
#endif

static gboolean fm_main_window_key_press_event ( GtkWidget *widget,
                                                 GdkEventKey *event );

static gboolean fm_main_window_window_state_event ( GtkWidget *widget,
                                                    GdkEventWindowState *event );

static gboolean fm_main_window_edit_address ( FMMainWindow* widget );

static void fm_main_window_next_tab ( FMMainWindow* widget );
static void fm_main_window_prev_tab ( FMMainWindow* widget );

/* Signal handlers */
static void on_back_activate ( GtkToolButton *toolbutton,
                               FMMainWindow *main_window );
static void on_forward_activate ( GtkToolButton *toolbutton,
                                  FMMainWindow *main_window );
static void on_up_activate ( GtkToolButton *toolbutton,
                             FMMainWindow *main_window );
static void on_home_activate( GtkToolButton *toolbutton,
                              FMMainWindow *main_window );
static void on_refresh_activate ( GtkToolButton *toolbutton,
                                  gpointer user_data );
static void on_quit_activate ( GtkMenuItem *menuitem,
                               gpointer user_data );
static void on_new_folder_activate ( GtkMenuItem *menuitem,
                                     gpointer user_data );
static void on_new_text_file_activate ( GtkMenuItem *menuitem,
                                        gpointer user_data );
static void on_preference_activate ( GtkMenuItem *menuitem,
                                     gpointer user_data );
#if 0
static void on_file_assoc_activate ( GtkMenuItem *menuitem,
                                     gpointer user_data );
#endif
static void on_about_activate ( GtkMenuItem *menuitem,
                                gpointer user_data );
#if 0
static gboolean on_back_btn_popup_menu ( GtkWidget *widget,
                                         gpointer user_data );
static gboolean on_forward_btn_popup_menu ( GtkWidget *widget,
                                            gpointer user_data );
#endif
static void on_address_bar_activate ( GtkWidget *entry,
                                      gpointer user_data );
static void on_new_window_activate ( GtkMenuItem *menuitem,
                                     gpointer user_data );
static void on_new_tab_activate ( GtkMenuItem *menuitem,
                                  gpointer user_data );
static void on_folder_notebook_switch_pape ( GtkNotebook *notebook,
                                             GtkNotebookPage *page,
                                             guint page_num,
                                             gpointer user_data );
static void on_cut_activate ( GtkMenuItem *menuitem,
                              gpointer user_data );
static void on_copy_activate ( GtkMenuItem *menuitem,
                               gpointer user_data );
static void on_paste_activate ( GtkMenuItem *menuitem,
                                gpointer user_data );
static void on_delete_activate ( GtkMenuItem *menuitem,
                                 gpointer user_data );
static void on_select_all_activate ( GtkMenuItem *menuitem,
                                     gpointer user_data );
static void on_add_to_bookmark_activate ( GtkMenuItem *menuitem,
                                          gpointer user_data );
static void on_invert_selection_activate ( GtkMenuItem *menuitem,
                                           gpointer user_data );
static void on_close_tab_activate ( GtkMenuItem *menuitem,
                                    gpointer user_data );
static void on_rename_activate ( GtkMenuItem *menuitem,
                                 gpointer user_data );
#if 0
static void on_fullscreen_activate ( GtkMenuItem *menuitem,
                                     gpointer user_data );
#endif
static void on_location_bar_activate ( GtkMenuItem *menuitem,
                                        gpointer user_data );
static void on_show_hidden_activate ( GtkMenuItem *menuitem,
                                      gpointer user_data );
static void on_sort_by_name_activate ( GtkMenuItem *menuitem,
                                       gpointer user_data );
static void on_sort_by_size_activate ( GtkMenuItem *menuitem,
                                       gpointer user_data );
static void on_sort_by_mtime_activate ( GtkMenuItem *menuitem,
                                        gpointer user_data );
static void on_sort_by_type_activate ( GtkMenuItem *menuitem,
                                       gpointer user_data );
static void on_sort_by_perm_activate ( GtkMenuItem *menuitem,
                                       gpointer user_data );
static void on_sort_by_owner_activate ( GtkMenuItem *menuitem,
                                        gpointer user_data );
static void on_sort_ascending_activate ( GtkMenuItem *menuitem,
                                         gpointer user_data );
static void on_sort_descending_activate ( GtkMenuItem *menuitem,
                                          gpointer user_data );
static void on_view_as_icon_activate ( GtkMenuItem *menuitem,
                                       gpointer user_data );
static void on_view_as_list_activate ( GtkMenuItem *menuitem,
                                       gpointer user_data );
static void on_view_as_compact_list_activate ( GtkMenuItem *menuitem,
                                       gpointer user_data );
static void on_open_side_pane_activate ( GtkMenuItem *menuitem,
                                         gpointer user_data );
static void on_show_dir_tree ( GtkMenuItem *menuitem,
                               gpointer user_data );
static void on_show_loation_pane ( GtkMenuItem *menuitem,
                                   gpointer user_data );
static void on_side_pane_toggled ( GtkToggleToolButton *toggletoolbutton,
                                   gpointer user_data );
static void on_go_btn_clicked ( GtkToolButton *toolbutton,
                                gpointer user_data );
static void on_open_terminal_activate ( GtkMenuItem *menuitem,
                                        gpointer user_data );
static void on_open_current_folder_as_root ( GtkMenuItem *menuitem,
                                             gpointer user_data );
static void on_find_file_activate ( GtkMenuItem *menuitem,
                                        gpointer user_data );
static void on_file_properties_activate ( GtkMenuItem *menuitem,
                                          gpointer user_data );
static void on_bookmark_item_activate ( GtkMenuItem* menu, gpointer user_data );


static void on_file_browser_before_chdir( PtkFileBrowser* file_browser,
                                          const char* path, gboolean* cancel,
                                          FMMainWindow* main_window );
static void on_file_browser_begin_chdir( PtkFileBrowser* file_browser,
                                         FMMainWindow* main_window );
static void on_file_browser_open_item( PtkFileBrowser* file_browser,
                                       const char* path, PtkOpenAction action,
                                       FMMainWindow* main_window );
static void on_file_browser_after_chdir( PtkFileBrowser* file_browser,
                                         FMMainWindow* main_window );
static void on_file_browser_content_change( PtkFileBrowser* file_browser,
                                            FMMainWindow* main_window );
static void on_file_browser_sel_change( PtkFileBrowser* file_browser,
                                        FMMainWindow* main_window );
static void on_file_browser_pane_mode_change( PtkFileBrowser* file_browser,
                                              FMMainWindow* main_window );
static void on_file_browser_splitter_pos_change( PtkFileBrowser* file_browser,
                                                 GParamSpec *param,
                                                 FMMainWindow* main_window );

static gboolean on_tab_drag_motion ( GtkWidget *widget,
                                     GdkDragContext *drag_context,
                                     gint x,
                                     gint y,
                                     guint time,
                                     PtkFileBrowser* file_browser );

static void on_view_menu_popup( GtkMenuItem* item,
                                FMMainWindow* main_window );


/* Callback called when the bookmarks change */
static void on_bookmarks_change( PtkBookmarks* bookmarks,
                                 FMMainWindow* main_window );

static gboolean on_main_window_focus( GtkWidget* main_window,
                                      GdkEventFocus *event,
                                      gpointer user_data );

static gboolean on_main_window_keypress( FMMainWindow* widget,
                                         GdkEventKey* event, gpointer data);

/* Utilities */
static void fm_main_window_update_status_bar( FMMainWindow* main_window,
                                              PtkFileBrowser* file_browser );
static void fm_main_window_update_command_ui( FMMainWindow* main_window,
                                              PtkFileBrowser* file_browser );

/* Automatically process busy cursor */
static void fm_main_window_start_busy_task( FMMainWindow* main_window );
static gboolean fm_main_window_stop_busy_task( FMMainWindow* main_window );


static GtkWindowClass *parent_class = NULL;

static int n_windows = 0;
static GList* all_windows = NULL;

static guint theme_change_notify = 0;

/*  Drag & Drop/Clipboard targets  */
static GtkTargetEntry drag_targets[] = {
                                           {"text/uri-list", 0 , 0 }
                                       };

GType fm_main_window_get_type()
{
    static GType type = G_TYPE_INVALID;
    if ( G_UNLIKELY ( type == G_TYPE_INVALID ) )
    {
        static const GTypeInfo info =
            {
                sizeof ( FMMainWindowClass ),
                NULL,
                NULL,
                ( GClassInitFunc ) fm_main_window_class_init,
                NULL,
                NULL,
                sizeof ( FMMainWindow ),
                0,
                ( GInstanceInitFunc ) fm_main_window_init,
                NULL,
            };
        type = g_type_register_static ( GTK_TYPE_WINDOW, "FMMainWindow", &info, 0 );
    }
    return type;
}

void fm_main_window_class_init( FMMainWindowClass* klass )
{
    GObjectClass * object_class;
    GtkWidgetClass *widget_class;

    object_class = ( GObjectClass * ) klass;
    parent_class = g_type_class_peek_parent ( klass );

    object_class->set_property = fm_main_window_set_property;
    object_class->get_property = fm_main_window_get_property;
    object_class->finalize = fm_main_window_finalize;

    widget_class = GTK_WIDGET_CLASS ( klass );
    widget_class->delete_event = (gpointer) fm_main_window_delete_event;
    widget_class->key_press_event = fm_main_window_key_press_event;
    widget_class->window_state_event = fm_main_window_window_state_event;
}


/* Main menu definition */

static PtkMenuItemEntry fm_file_create_new_manu[] =
    {
        PTK_IMG_MENU_ITEM( N_( "_Folder" ), "gtk-directory", on_new_folder_activate, 0, 0 ),
        PTK_IMG_MENU_ITEM( N_( "_Text File" ), "gtk-edit", on_new_text_file_activate, 0, 0 ),
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_file_menu[] =
    {
        PTK_IMG_MENU_ITEM( N_( "New _Window" ), "gtk-add", on_new_window_activate, GDK_N, GDK_CONTROL_MASK ),
        PTK_IMG_MENU_ITEM( N_( "New _Tab" ), "gtk-add", on_new_tab_activate, GDK_T, GDK_CONTROL_MASK ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_POPUP_IMG_MENU( N_( "_Create New" ), "gtk-new", fm_file_create_new_manu ),
        PTK_IMG_MENU_ITEM( N_( "File _Properties" ), "gtk-info", on_file_properties_activate, GDK_Return, GDK_MOD1_MASK ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_IMG_MENU_ITEM( N_( "Close Tab" ), "gtk-close", on_close_tab_activate, GDK_W, GDK_CONTROL_MASK ),
        PTK_IMG_MENU_ITEM( N_("_Close Window"), "gtk-quit", on_quit_activate, GDK_Q, GDK_CONTROL_MASK ),
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_edit_menu[] =
    {
        PTK_STOCK_MENU_ITEM( "gtk-cut", on_cut_activate ),
        PTK_STOCK_MENU_ITEM( "gtk-copy", on_copy_activate ),
        PTK_STOCK_MENU_ITEM( "gtk-paste", on_paste_activate ),
        PTK_IMG_MENU_ITEM( N_( "_Delete" ), "gtk-delete", on_delete_activate, GDK_Delete, 0 ),
        PTK_IMG_MENU_ITEM( N_( "_Rename" ), "gtk-edit", on_rename_activate, GDK_F2, 0 ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_MENU_ITEM( N_( "Select _All" ), on_select_all_activate, GDK_A, GDK_CONTROL_MASK ),
        PTK_MENU_ITEM( N_( "_Invert Selection" ), on_invert_selection_activate, GDK_I, GDK_CONTROL_MASK ),
        PTK_SEPARATOR_MENU_ITEM,
        // PTK_IMG_MENU_ITEM( N_( "_File Associations" ), "gtk-execute", on_file_assoc_activate , 0, 0 ),
        PTK_STOCK_MENU_ITEM( "gtk-preferences", on_preference_activate ),
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_go_menu[] =
    {
        PTK_IMG_MENU_ITEM( N_( "Go _Back" ), "gtk-go-back", on_back_activate, GDK_Left, GDK_MOD1_MASK ),
        PTK_IMG_MENU_ITEM( N_( "Go _Forward" ), "gtk-go-forward", on_forward_activate, GDK_Right, GDK_MOD1_MASK ),
        PTK_IMG_MENU_ITEM( N_( "Go to _Parent Folder" ), "gtk-go-up", on_up_activate, GDK_Up, GDK_MOD1_MASK ),
        PTK_IMG_MENU_ITEM( N_( "Go _Home" ), "gtk-home", on_home_activate, GDK_Home, GDK_MOD1_MASK ),
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_help_menu[] =
    {
        PTK_STOCK_MENU_ITEM( "gtk-about", on_about_activate ),
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_sort_menu[] =
    {
        PTK_RADIO_MENU_ITEM( N_( "Sort by _Name" ), on_sort_by_name_activate, 0, 0 ),
        PTK_RADIO_MENU_ITEM( N_( "Sort by _Size" ), on_sort_by_size_activate, 0, 0 ),
        PTK_RADIO_MENU_ITEM( N_( "Sort by _Modification Time" ), on_sort_by_mtime_activate, 0, 0 ),
        PTK_RADIO_MENU_ITEM( N_( "Sort by _Type" ), on_sort_by_type_activate, 0, 0 ),
        PTK_RADIO_MENU_ITEM( N_( "Sort by _Permission" ), on_sort_by_perm_activate, 0, 0 ),
        PTK_RADIO_MENU_ITEM( N_( "Sort by _Owner" ), on_sort_by_owner_activate, 0, 0 ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_RADIO_MENU_ITEM( N_( "Ascending" ), on_sort_ascending_activate, 0, 0 ),
        PTK_RADIO_MENU_ITEM( N_( "Descending" ), on_sort_descending_activate, 0, 0 ),
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_side_pane_menu[] =
    {
        PTK_CHECK_MENU_ITEM( N_( "_Open Side Pane" ), on_open_side_pane_activate, GDK_F9, 0 ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_RADIO_MENU_ITEM( N_( "Show _Location Pane" ), on_show_loation_pane, GDK_B, GDK_CONTROL_MASK ),
        PTK_RADIO_MENU_ITEM( N_( "Show _Directory Tree" ), on_show_dir_tree, GDK_D, GDK_CONTROL_MASK ),
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_view_menu[] =
    {
        PTK_IMG_MENU_ITEM( N_( "_Refresh" ), "gtk-refresh", on_refresh_activate, GDK_F5, 0 ),
        PTK_POPUP_MENU( N_( "Side _Pane" ), fm_side_pane_menu ),
        /* PTK_CHECK_MENU_ITEM( N_( "_Full Screen" ), on_fullscreen_activate, GDK_F11, 0 ), */
        PTK_CHECK_MENU_ITEM( N_( "L_ocation Bar" ), on_location_bar_activate, 0, 0 ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_CHECK_MENU_ITEM( N_( "Show _Hidden Files" ), on_show_hidden_activate, GDK_H, GDK_CONTROL_MASK ),
        PTK_POPUP_MENU( N_( "_Sort..." ), fm_sort_menu ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_RADIO_MENU_ITEM( N_( "View as _Icons" ), on_view_as_icon_activate, 0, 0 ),
        PTK_RADIO_MENU_ITEM( N_( "View as _Compact List" ), on_view_as_compact_list_activate, 0, 0 ),
        PTK_RADIO_MENU_ITEM( N_( "View as Detailed _List" ), on_view_as_list_activate, 0, 0 ),
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_tool_menu[] =
    {
        PTK_IMG_MENU_ITEM( N_( "Open _Terminal" ), GTK_STOCK_EXECUTE, on_open_terminal_activate, GDK_F4, 0 ),
        PTK_IMG_MENU_ITEM( N_( "Open Current Folder as _Root" ),
                           GTK_STOCK_DIALOG_AUTHENTICATION,
                           on_open_current_folder_as_root, 0, 0 ),
        PTK_SEPARATOR_MENU_ITEM,
        PTK_IMG_MENU_ITEM( N_( "_Find Files" ), GTK_STOCK_FIND, on_find_file_activate, GDK_F3, 0 ),
        PTK_MENU_END
    };

static PtkMenuItemEntry fm_menu_bar[] =
    {
        PTK_POPUP_MENU( N_( "_File" ), fm_file_menu ),
        PTK_MENU_ITEM( N_( "_Edit" ), NULL, 0, 0 ),
        /* PTK_POPUP_MENU( N_( "_Edit" ), fm_edit_menu ), */
        PTK_POPUP_MENU( N_( "_Go" ), fm_go_menu ),
        PTK_MENU_ITEM( N_( "_Bookmark" ), NULL, 0, 0 ),
        PTK_POPUP_MENU( N_( "_View" ), fm_view_menu ),
        PTK_POPUP_MENU( N_( "_Tool" ), fm_tool_menu ),
        PTK_POPUP_MENU( N_( "_Help" ), fm_help_menu ),
        PTK_MENU_END
    };

/* Toolbar items defiinition */

static PtkToolItemEntry fm_toolbar_btns[] =
    {
        PTK_TOOL_ITEM( N_( "New Tab" ), "gtk-add", N_( "New Tab" ), on_new_tab_activate ),
        PTK_MENU_TOOL_ITEM( N_( "Back" ), "gtk-go-back", N_( "Back" ), G_CALLBACK(on_back_activate), PTK_EMPTY_MENU ),
        PTK_MENU_TOOL_ITEM( N_( "Forward" ), "gtk-go-forward", N_( "Forward" ), G_CALLBACK(on_forward_activate), PTK_EMPTY_MENU ),
        PTK_TOOL_ITEM( N_( "Parent Folder" ), "gtk-go-up", N_( "Parent Folder" ), on_up_activate ),
        PTK_TOOL_ITEM( N_( "Refresh" ), "gtk-refresh", N_( "Refresh" ), on_refresh_activate ),
        PTK_TOOL_ITEM( N_( "Home Directory" ), "gtk-home", N_( "Home Directory" ), on_home_activate ),
        PTK_CHECK_TOOL_ITEM( N_( "Open Side Pane" ), "gtk-open", N_( "Open Side Pane" ), on_side_pane_toggled ),
        PTK_TOOL_END
    };

static PtkToolItemEntry fm_toolbar_jump_btn[] =
    {
        PTK_TOOL_ITEM( NULL, "gtk-jump-to", N_( "Go" ), on_go_btn_clicked ),
        PTK_TOOL_END
    };

static void update_window_icon( GtkWindow* window, GtkIconTheme* theme )
{
    GdkPixbuf* icon;
    icon = gtk_icon_theme_load_icon( theme, "gnome-fs-directory", 48, 0, NULL );
    gtk_window_set_icon( window, icon );
    g_object_unref( icon );
}

static void update_window_icons( GtkIconTheme* theme, GtkWindow* window )
{
	g_list_foreach( all_windows, (GFunc)update_window_icon, theme );
}

static void on_history_menu_item_activate( GtkWidget* menu_item, FMMainWindow* main_window )
{
    GList* l = (GList*)g_object_get_data( G_OBJECT(menu_item), "path"), *tmp;
    PtkFileBrowser* fb = (PtkFileBrowser*)fm_main_window_get_current_file_browser(main_window);
    tmp = fb->curHistory;
    fb->curHistory = l;
    if( !  ptk_file_browser_chdir( fb, (char*)l->data, PTK_FB_CHDIR_NO_HISTORY ) )
        fb->curHistory = tmp;
}

static void remove_all_menu_items( GtkWidget* menu, gpointer user_data )
{
    gtk_container_foreach( (GtkContainer*)menu, (GtkCallback)gtk_widget_destroy, NULL );
}

static GtkWidget* add_history_menu_item( FMMainWindow* main_window, GtkWidget* menu, GList* l )
{
    GtkWidget* menu_item, *folder_image;
    char *disp_name;
    disp_name = g_filename_display_basename( (char*)l->data );
    menu_item = gtk_image_menu_item_new_with_label( disp_name );
    g_object_set_data( G_OBJECT( menu_item ), "path", l );
    folder_image = gtk_image_new_from_icon_name( "gnome-fs-directory",
                                                 GTK_ICON_SIZE_MENU );
    gtk_image_menu_item_set_image ( GTK_IMAGE_MENU_ITEM ( menu_item ),
                                    folder_image );
    g_signal_connect( menu_item, "activate",
                      G_CALLBACK( on_history_menu_item_activate ), main_window );

    gtk_menu_shell_append( GTK_MENU_SHELL(menu), menu_item );
    return menu_item;
}

static void on_show_history_menu( GtkMenuToolButton* btn, FMMainWindow* main_window )
{
    GtkMenuShell* menu = (GtkMenuShell*)gtk_menu_tool_button_get_menu(btn);
    GList *l;
    PtkFileBrowser* fb = (PtkFileBrowser*)fm_main_window_get_current_file_browser(main_window);

    if( btn == (GtkMenuToolButton *) main_window->back_btn )  /* back history */
    {
        for( l = fb->curHistory->prev; l != NULL; l = l->prev )
            add_history_menu_item( main_window, GTK_WIDGET(menu), l );
    }
    else    /* forward history */
    {
        for( l = fb->curHistory->next; l != NULL; l = l->next )
            add_history_menu_item( main_window, GTK_WIDGET(menu), l );
    }
    gtk_widget_show_all( GTK_WIDGET(menu) );
}

void fm_main_window_init( FMMainWindow* main_window )
{
    GtkWidget *bookmark_menu;
    GtkWidget *view_menu_item;
    GtkWidget *edit_menu_item, *edit_menu, *history_menu;
    GtkToolItem *toolitem;
    GtkWidget *hbox;
    GtkLabel *label;
    GtkAccelGroup *edit_accel_group;
    GClosure* closure;

    /* Initialize parent class */
    GTK_WINDOW( main_window )->type = GTK_WINDOW_TOPLEVEL;

    /* this is used to limit the scope of gtk_grab and modal dialogs */
    main_window->wgroup = gtk_window_group_new();
    gtk_window_group_add_window( main_window->wgroup, (GtkWindow*)main_window );

    /* Add to total window count */
    ++n_windows;
    all_windows = g_list_prepend( all_windows, main_window );

    pcmanfm_ref();

    /* Set a monitor for changes of the bookmarks */
    ptk_bookmarks_add_callback( ( GFunc ) on_bookmarks_change, main_window );

    /* Start building GUI */
    /*
    NOTE: gtk_window_set_icon_name doesn't work under some WMs, such as IceWM.
    gtk_window_set_icon_name( GTK_WINDOW( main_window ),
                              "gnome-fs-directory" ); */
	if( 0 == theme_change_notify )
	{
		theme_change_notify = g_signal_connect( gtk_icon_theme_get_default(), "changed",
										G_CALLBACK(update_window_icons), NULL );
	}
    update_window_icon( (GtkWindow*)main_window, gtk_icon_theme_get_default() );

    main_window->main_vbox = gtk_vbox_new ( FALSE, 0 );
    gtk_container_add ( GTK_CONTAINER ( main_window ), main_window->main_vbox );

    main_window->splitter_pos = app_settings.splitter_pos;

    /* Create menu bar */
    main_window->menu_bar = gtk_menu_bar_new ();
    gtk_box_pack_start ( GTK_BOX ( main_window->main_vbox ),
                         main_window->menu_bar, FALSE, FALSE, 0 );

    main_window->accel_group = gtk_accel_group_new ();
    fm_side_pane_menu[ 0 ].ret = ( GtkWidget** ) (GtkWidget*) & main_window->open_side_pane_menu;
    fm_side_pane_menu[ 2 ].ret = ( GtkWidget** ) (GtkWidget*) & main_window->show_location_menu;
    fm_side_pane_menu[ 3 ].ret = ( GtkWidget** ) (GtkWidget*) & main_window->show_dir_tree_menu;
    fm_view_menu[ 2 ].ret = ( GtkWidget** ) (GtkWidget*) & main_window->show_location_bar_menu;
    fm_view_menu[ 4 ].ret = ( GtkWidget** ) (GtkWidget*) & main_window->show_hidden_files_menu;
    fm_view_menu[ 7 ].ret = ( GtkWidget** ) (GtkWidget*) & main_window->view_as_icon;
    fm_view_menu[ 8 ].ret = ( GtkWidget** ) (GtkWidget*) & main_window->view_as_compact_list;
    fm_view_menu[ 9 ].ret = ( GtkWidget** ) (GtkWidget*) & main_window->view_as_list;
    fm_sort_menu[ 0 ].ret = ( GtkWidget** ) (GtkWidget*) & main_window->sort_by_name;
    fm_sort_menu[ 1 ].ret = ( GtkWidget** ) (GtkWidget*) & main_window->sort_by_size;
    fm_sort_menu[ 2 ].ret = ( GtkWidget** ) (GtkWidget*) & main_window->sort_by_mtime;
    fm_sort_menu[ 3 ].ret = ( GtkWidget** ) (GtkWidget*) & main_window->sort_by_type;
    fm_sort_menu[ 4 ].ret = ( GtkWidget** ) (GtkWidget*) & main_window->sort_by_perm;
    fm_sort_menu[ 5 ].ret = ( GtkWidget** ) (GtkWidget*) & main_window->sort_by_owner;
    fm_sort_menu[ 7 ].ret = ( GtkWidget** ) (GtkWidget*) & main_window->sort_ascending;
    fm_sort_menu[ 8 ].ret = ( GtkWidget** ) (GtkWidget*) & main_window->sort_descending;
    fm_menu_bar[ 1 ].ret = &edit_menu_item;
    fm_menu_bar[ 3 ].ret = &main_window->bookmarks;
    fm_menu_bar[ 4 ].ret = &view_menu_item;
    ptk_menu_add_items_from_data( main_window->menu_bar, fm_menu_bar,
                                  main_window, main_window->accel_group );
    /* This accel_group is useless and just used to show
       shortcuts in edit menu. These shortcuts are
       provided by PtkFileBrowser itself. */
    edit_accel_group = gtk_accel_group_new();
    edit_menu = ptk_menu_new_from_data( fm_edit_menu, main_window, edit_accel_group );
    g_object_weak_ref( G_OBJECT(edit_menu), (GWeakNotify) g_object_unref, edit_accel_group);

    gtk_menu_item_set_submenu( GTK_MENU_ITEM( edit_menu_item ), edit_menu );
    bookmark_menu = create_bookmarks_menu( main_window );
    gtk_menu_item_set_submenu( GTK_MENU_ITEM( main_window->bookmarks ),
                               bookmark_menu );
    g_signal_connect( view_menu_item, "activate",
                      G_CALLBACK( on_view_menu_popup ), main_window );

    /* Create toolbar */
    main_window->toolbar = gtk_toolbar_new ();
    gtk_toolbar_set_style ( GTK_TOOLBAR ( main_window->toolbar ), GTK_TOOLBAR_ICONS );
    gtk_box_pack_start ( GTK_BOX ( main_window->main_vbox ),
                         main_window->toolbar, FALSE, FALSE, 0 );

    main_window->tooltips = gtk_tooltips_new ();
    fm_toolbar_btns[ 1 ].ret = &main_window->back_btn;
    fm_toolbar_btns[ 2 ].ret = &main_window->forward_btn;
    fm_toolbar_btns[ 6 ].ret = ( GtkWidget** ) (GtkWidget*) & main_window->open_side_pane_btn;
    ptk_toolbar_add_items_from_data( ( main_window->toolbar ),
                                     fm_toolbar_btns,
                                     main_window, main_window->tooltips );

    /* setup history menu */
    history_menu = gtk_menu_new();  /* back history */
    g_signal_connect( history_menu, "selection-done", G_CALLBACK(remove_all_menu_items), NULL );
    gtk_menu_tool_button_set_menu( (GtkMenuToolButton*)main_window->back_btn, history_menu );
    g_signal_connect( main_window->back_btn, "show-menu", G_CALLBACK(on_show_history_menu), main_window );

    history_menu = gtk_menu_new();  /* forward history */
    g_signal_connect( history_menu, "selection-done", G_CALLBACK(remove_all_menu_items), NULL );
    gtk_menu_tool_button_set_menu( (GtkMenuToolButton*)main_window->forward_btn, history_menu );
    g_signal_connect( main_window->forward_btn, "show-menu", G_CALLBACK(on_show_history_menu), main_window );

    toolitem = gtk_tool_item_new ();
    gtk_tool_item_set_expand ( toolitem, TRUE );
    gtk_toolbar_insert( GTK_TOOLBAR( main_window->toolbar ), toolitem, -1 );

    hbox = gtk_hbox_new ( FALSE, 0 );
    gtk_container_add ( GTK_CONTAINER ( toolitem ), hbox );

    gtk_box_pack_start ( GTK_BOX ( hbox ), (GtkWidget *) gtk_separator_tool_item_new(),
                         FALSE, FALSE, 0 );

    main_window->address_bar = ( GtkEntry* ) ptk_path_entry_new ();
/*
    gtk_tooltips_set_tip( main_window->tooltips, main_window->address_bar,
            ("Useful tips: \n1. Shortcut keys \'Alt+D\' or \'Ctrl+L\' can focus this bar.\n"
                "2. Left click on the path with \'Ctrl\' key held down can bring you to parent folders."), NULL );
*/

    /* Add shortcut keys Alt+D and Ctrl+L for the address bar */
    closure = g_cclosure_new_swap( G_CALLBACK(fm_main_window_edit_address), main_window, NULL );
    gtk_accel_group_connect( main_window->accel_group, GDK_L, GDK_CONTROL_MASK, 0, closure );

    closure = g_cclosure_new_swap( G_CALLBACK(fm_main_window_edit_address), main_window, NULL );
    gtk_accel_group_connect( main_window->accel_group, GDK_D, GDK_MOD1_MASK, 0, closure );
    
    /* Add shortcut keys Ctrl+PageUp/PageDown and Ctrl+Tab/Ctrl+Shift+Tab to swtich tabs */
    closure = g_cclosure_new_swap( G_CALLBACK(fm_main_window_next_tab), main_window, NULL );
    gtk_accel_group_connect( main_window->accel_group, GDK_Page_Down, GDK_CONTROL_MASK, 0, closure );
    
    closure = g_cclosure_new_swap( G_CALLBACK(fm_main_window_prev_tab), main_window, NULL );
    gtk_accel_group_connect( main_window->accel_group, GDK_Page_Up, GDK_CONTROL_MASK, 0, closure );
    
      /* No way to assign Tab with accel group (http://bugzilla.gnome.org/show_bug.cgi?id=123994) */
    g_signal_connect( G_OBJECT( main_window ), "key-press-event",
                      G_CALLBACK( on_main_window_keypress ), NULL );

    
    g_signal_connect( main_window->address_bar, "activate",
                      G_CALLBACK(on_address_bar_activate), main_window );
    gtk_box_pack_start ( GTK_BOX ( hbox ), GTK_WIDGET( main_window->address_bar ), TRUE, TRUE, 0 );
    ptk_toolbar_add_items_from_data( ( main_window->toolbar ),
                                     fm_toolbar_jump_btn,
                                     main_window, main_window->tooltips );

    gtk_window_add_accel_group ( GTK_WINDOW ( main_window ), main_window->accel_group );
    gtk_widget_grab_focus ( GTK_WIDGET( main_window->address_bar ) );

#ifdef SUPER_USER_CHECKS
    /* Create warning bar for super user */
    if ( geteuid() == 0 )                 /* Run as super user! */
    {
        main_window->status_bar = gtk_event_box_new();
        gtk_widget_modify_bg( main_window->status_bar, GTK_STATE_NORMAL,
                              &main_window->status_bar->style->bg[ GTK_STATE_SELECTED ] );
        label = GTK_LABEL( gtk_label_new ( _( "Warning: You are in super user mode" ) ) );
        gtk_misc_set_padding( GTK_MISC( label ), 0, 2 );
        gtk_widget_modify_fg( GTK_WIDGET( label ), GTK_STATE_NORMAL,
                              &GTK_WIDGET( label ) ->style->fg[ GTK_STATE_SELECTED ] );
        gtk_container_add( GTK_CONTAINER( main_window->status_bar ), GTK_WIDGET( label ) );
        gtk_box_pack_start ( GTK_BOX ( main_window->main_vbox ),
                             main_window->status_bar, FALSE, FALSE, 2 );
    }
#endif

    /* Create client area */
    main_window->notebook = GTK_NOTEBOOK( gtk_notebook_new() );
    gtk_notebook_set_show_border( main_window->notebook, FALSE );
    gtk_notebook_set_scrollable ( main_window->notebook, TRUE );
    gtk_box_pack_start ( GTK_BOX ( main_window->main_vbox ), GTK_WIDGET( main_window->notebook ), TRUE, TRUE, 0 );

    g_signal_connect ( main_window->notebook, "switch_page",
                       G_CALLBACK ( on_folder_notebook_switch_pape ), main_window );

    /* Create Status bar */
    main_window->status_bar = gtk_statusbar_new ();
    gtk_box_pack_start ( GTK_BOX ( main_window->main_vbox ),
                         main_window->status_bar, FALSE, FALSE, 0 );

    gtk_widget_show_all( main_window->main_vbox );

    g_signal_connect ( main_window, "focus-in-event",
                       G_CALLBACK ( on_main_window_focus ), NULL );

    /* Set other main window parameters according to application settings */
    gtk_check_menu_item_set_active ( main_window->show_location_bar_menu,
                                     app_settings.show_location_bar );
    if ( !app_settings.show_location_bar )
        gtk_widget_hide ( main_window->toolbar );
}

void fm_main_window_finalize( GObject *obj )
{
    all_windows = g_list_remove( all_windows, obj );
    --n_windows;

    g_object_unref( ((FMMainWindow*)obj)->wgroup );

    pcmanfm_unref();

    /* Remove the monitor for changes of the bookmarks */
    ptk_bookmarks_remove_callback( ( GFunc ) on_bookmarks_change, obj );
    if ( 0 == n_windows )
    {
    	g_signal_handler_disconnect( gtk_icon_theme_get_default(), theme_change_notify );
		theme_change_notify = 0;
	}

    G_OBJECT_CLASS( parent_class ) ->finalize( obj );
}

void fm_main_window_get_property ( GObject *obj,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec )
{}

void fm_main_window_set_property ( GObject *obj,
                                   guint prop_id,
                                   const GValue *value,
                                   GParamSpec *pspec )
{}

static void fm_main_window_close( GtkWidget* main_window )
{
    app_settings.width = GTK_WIDGET( main_window ) ->allocation.width;
    app_settings.height = GTK_WIDGET( main_window ) ->allocation.height;
    app_settings.splitter_pos = FM_MAIN_WINDOW( main_window ) ->splitter_pos;
    gtk_widget_destroy( main_window );
}

gboolean
fm_main_window_delete_event ( GtkWidget *widget,
                              GdkEvent *event )
{
    fm_main_window_close( widget );
    return TRUE;
}

static gboolean fm_main_window_window_state_event ( GtkWidget *widget,
                                                    GdkEventWindowState *event )
{
    app_settings.maximized = ((event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0);
    return TRUE;
}

static gboolean
fm_main_window_edit_address ( FMMainWindow* main_window )
{
    if ( GTK_WIDGET_VISIBLE( main_window->toolbar ) )
        gtk_widget_grab_focus( GTK_WIDGET( main_window->address_bar ) );
    else
        fm_go( main_window );
    return TRUE;
}

static void
fm_main_window_next_tab ( FMMainWindow* widget )
{
    if ( gtk_notebook_get_current_page( widget->notebook ) + 1 ==
      gtk_notebook_get_n_pages( widget->notebook ) )
    {
        gtk_notebook_set_current_page( widget->notebook, 0 );
    }
    else
    {
        gtk_notebook_next_page( widget->notebook );
    }
}

static void
fm_main_window_prev_tab ( FMMainWindow* widget )
{
    if ( gtk_notebook_get_current_page( widget->notebook ) == 0 )
    {
        gtk_notebook_set_current_page( widget->notebook, 
          gtk_notebook_get_n_pages( widget->notebook ) - 1 );
    }
    else
    {
        gtk_notebook_prev_page( widget->notebook );
    }
}

static void
on_close_notebook_page( GtkButton* btn, PtkFileBrowser* file_browser )
{
    GtkNotebook * notebook = GTK_NOTEBOOK(
                                 gtk_widget_get_ancestor( GTK_WIDGET( file_browser ),
                                                          GTK_TYPE_NOTEBOOK ) );

    gtk_widget_destroy( GTK_WIDGET( file_browser ) );
    if ( !app_settings.always_show_tabs )
      if ( gtk_notebook_get_n_pages ( notebook ) == 1 )
          gtk_notebook_set_show_tabs( notebook, FALSE );
}


GtkWidget* fm_main_window_create_tab_label( FMMainWindow* main_window,
                                            PtkFileBrowser* file_browser )
{
    GtkEventBox * evt_box;
    GtkWidget* tab_label;
    GtkWidget* tab_text;
    GtkWidget* tab_icon;
    GtkWidget* close_btn;

    /* Create tab label */
    evt_box = GTK_EVENT_BOX( gtk_event_box_new () );
    gtk_event_box_set_visible_window ( GTK_EVENT_BOX( evt_box ), FALSE );

    tab_label = gtk_hbox_new( FALSE, 0 );
    tab_icon = gtk_image_new_from_icon_name ( "gnome-fs-directory",
                                              GTK_ICON_SIZE_MENU );
    gtk_box_pack_start( GTK_BOX( tab_label ),
                        tab_icon, FALSE, FALSE, 4 );

    tab_text = gtk_label_new( "" );
    gtk_box_pack_start( GTK_BOX( tab_label ),
                        tab_text, FALSE, FALSE, 4 );

    if ( !app_settings.hide_close_tab_buttons ) {
        close_btn = gtk_button_new ();
        gtk_button_set_focus_on_click ( GTK_BUTTON ( close_btn ), FALSE );
        gtk_button_set_relief( GTK_BUTTON ( close_btn ), GTK_RELIEF_NONE );
        gtk_container_add ( GTK_CONTAINER ( close_btn ),
                            gtk_image_new_from_icon_name( "gtk-close", GTK_ICON_SIZE_MENU ) );
        gtk_widget_set_size_request ( close_btn, 20, 20 );
        gtk_box_pack_start ( GTK_BOX( tab_label ),
                             close_btn, FALSE, FALSE, 0 );
        g_signal_connect( G_OBJECT( close_btn ), "clicked",
                          G_CALLBACK( on_close_notebook_page ), file_browser );
    }

    gtk_container_add ( GTK_CONTAINER ( evt_box ), tab_label );

    gtk_widget_set_events ( GTK_WIDGET( evt_box ), GDK_ALL_EVENTS_MASK );
    gtk_drag_dest_set ( GTK_WIDGET( evt_box ), GTK_DEST_DEFAULT_ALL,
                        drag_targets,
                        sizeof( drag_targets ) / sizeof( GtkTargetEntry ),
                        GDK_ACTION_DEFAULT | GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK );
    g_signal_connect ( ( gpointer ) evt_box, "drag-motion",
                       G_CALLBACK ( on_tab_drag_motion ),
                       file_browser );

    gtk_widget_show_all( GTK_WIDGET( evt_box ) );
    return GTK_WIDGET( evt_box );
}

void fm_main_window_update_tab_label( FMMainWindow* main_window,
                                      PtkFileBrowser* file_browser,
                                      const char * path )
{
    GtkWidget * label;
    GtkContainer* hbox;
    GtkImage* icon;
    GtkLabel* text;
    GList* children;
    gchar* name;

    label = gtk_notebook_get_tab_label ( main_window->notebook,
                                         GTK_WIDGET( file_browser ) );
    hbox = GTK_CONTAINER( gtk_bin_get_child ( GTK_BIN( label ) ) );
    children = gtk_container_get_children( hbox );
    icon = GTK_IMAGE( children->data );
    text = GTK_LABEL( children->next->data );
    g_list_free( children );

    /* TODO: Change the icon */

    name = g_path_get_basename( path );
    gtk_label_set_text( text, name );
    g_free( name );
}

void fm_main_window_add_new_tab( FMMainWindow* main_window,
                                 const char* folder_path,
                                 gboolean open_side_pane,
                                 PtkFBSidePaneMode side_pane_mode )
{
    GtkWidget * tab_label;
    gint idx;
    GtkNotebook* notebook = main_window->notebook;
    PtkFileBrowser* file_browser = (PtkFileBrowser*)ptk_file_browser_new ( GTK_WIDGET( main_window ),
                                                                            app_settings.view_mode );
    gtk_paned_set_position ( GTK_PANED ( file_browser ), main_window->splitter_pos );
    ptk_file_browser_set_single_click( file_browser, app_settings.single_click );
    /* FIXME: this shouldn't be hard-code */
    ptk_file_browser_set_single_click_timeout( file_browser, 400 );
    ptk_file_browser_show_hidden_files( file_browser,
                                        app_settings.show_hidden_files );
    ptk_file_browser_show_thumbnails( file_browser,
                                      app_settings.show_thumbnail ? app_settings.max_thumb_size : 0 );

    if ( open_side_pane )
        ptk_file_browser_show_side_pane( file_browser, side_pane_mode );
    else
        ptk_file_browser_hide_side_pane( file_browser );

    if ( app_settings.hide_side_pane_buttons )
        ptk_file_browser_hide_side_pane_buttons( file_browser );
    else
        ptk_file_browser_show_side_pane_buttons( file_browser );

    if ( app_settings.hide_folder_content_border )
        ptk_file_browser_hide_shadow( file_browser );
    else
        ptk_file_browser_show_shadow( file_browser );

    ptk_file_browser_set_sort_order( file_browser, app_settings.sort_order );
    ptk_file_browser_set_sort_type( file_browser, app_settings.sort_type );

    gtk_widget_show( GTK_WIDGET( file_browser ) );

    g_signal_connect( file_browser, "before-chdir",
                      G_CALLBACK( on_file_browser_before_chdir ), main_window );
    g_signal_connect( file_browser, "begin-chdir",
                      G_CALLBACK( on_file_browser_begin_chdir ), main_window );
    g_signal_connect( file_browser, "after-chdir",
                      G_CALLBACK( on_file_browser_after_chdir ), main_window );
    g_signal_connect( file_browser, "open-item",
                      G_CALLBACK( on_file_browser_open_item ), main_window );
    g_signal_connect( file_browser, "content-change",
                      G_CALLBACK( on_file_browser_content_change ), main_window );
    g_signal_connect( file_browser, "sel-change",
                      G_CALLBACK( on_file_browser_sel_change ), main_window );
    g_signal_connect( file_browser, "pane-mode-change",
                      G_CALLBACK( on_file_browser_pane_mode_change ), main_window );
    g_signal_connect( file_browser, "notify::position",
                      G_CALLBACK( on_file_browser_splitter_pos_change ), main_window );

    tab_label = fm_main_window_create_tab_label( main_window, file_browser );
    idx = gtk_notebook_append_page( notebook, GTK_WIDGET( file_browser ), tab_label );
#if GTK_CHECK_VERSION( 2, 10, 0 )
    gtk_notebook_set_tab_reorderable( notebook, GTK_WIDGET( file_browser ), TRUE );
#endif
    gtk_notebook_set_current_page ( notebook, idx );

    if (app_settings.always_show_tabs)
        gtk_notebook_set_show_tabs( notebook, TRUE );
    else
        if ( gtk_notebook_get_n_pages ( notebook ) > 1 )
            gtk_notebook_set_show_tabs( notebook, TRUE );
        else
            gtk_notebook_set_show_tabs( notebook, FALSE );

    ptk_file_browser_chdir( file_browser, folder_path, PTK_FB_CHDIR_ADD_HISTORY );

    gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );
}

GtkWidget* fm_main_window_new()
{
    return ( GtkWidget* ) g_object_new ( FM_TYPE_MAIN_WINDOW, NULL );
}

GtkWidget* fm_main_window_get_current_file_browser ( FMMainWindow* main_window )
{
    GtkNotebook * notebook = main_window->notebook;
    gint idx = gtk_notebook_get_current_page( notebook );
    return gtk_notebook_get_nth_page( notebook, idx );
}

void on_back_activate( GtkToolButton *toolbutton, FMMainWindow *main_window )
{
    PtkFileBrowser * file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    if ( file_browser && ptk_file_browser_can_back( file_browser ) )
        ptk_file_browser_go_back( file_browser );
}


void on_forward_activate( GtkToolButton *toolbutton, FMMainWindow *main_window )
{
    PtkFileBrowser * file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    if ( file_browser && ptk_file_browser_can_forward( file_browser ) )
        ptk_file_browser_go_forward( file_browser );
}


void on_up_activate( GtkToolButton *toolbutton, FMMainWindow *main_window )
{
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    ptk_file_browser_go_up( file_browser );
}


void on_home_activate( GtkToolButton *toolbutton, FMMainWindow *main_window )
{
    GtkWidget * file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_chdir( PTK_FILE_BROWSER( file_browser ), g_get_home_dir(), PTK_FB_CHDIR_ADD_HISTORY );
}


void on_address_bar_activate ( GtkWidget *entry,
                               gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    gchar *dir_path, *final_path;
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    gtk_editable_select_region( (GtkEditable*)entry, 0, 0 );    /* clear selection */
    /* Convert to on-disk encoding */
    dir_path = g_filename_from_utf8( gtk_entry_get_text( GTK_ENTRY( entry ) ), -1,
                                     NULL, NULL, NULL );
    final_path = vfs_file_resolve_path( ptk_file_browser_get_cwd(file_browser), dir_path );
    g_free( dir_path );
    ptk_file_browser_chdir( file_browser, final_path, PTK_FB_CHDIR_ADD_HISTORY );
    g_free( final_path );
    gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );
}

#if 0
/* This is not needed by a file manager */
/* Patched by <cantona@cantona.net> */
void on_fullscreen_activate ( GtkMenuItem *menuitem,
                              gpointer user_data )
{
    GtkWindow * window;
    int is_fullscreen;

    window = GTK_WINDOW( user_data );
    is_fullscreen = gtk_check_menu_item_get_active( GTK_CHECK_MENU_ITEM( menuitem ) );

    if ( is_fullscreen )
    {
        gtk_window_fullscreen( GTK_WINDOW( window ) );
    }
    else
    {
        gtk_window_unfullscreen( GTK_WINDOW( window ) );
    }
}
#endif

void on_refresh_activate ( GtkToolButton *toolbutton,
                           gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_refresh( PTK_FILE_BROWSER( file_browser ) );
}


void
on_quit_activate ( GtkMenuItem *menuitem,
                   gpointer user_data )
{
    fm_main_window_close( GTK_WIDGET( user_data ) );
}


void
on_new_folder_activate ( GtkMenuItem *menuitem,
                         gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_create_new_file( PTK_FILE_BROWSER( file_browser ), TRUE );
}


void
on_new_text_file_activate ( GtkMenuItem *menuitem,
                            gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_create_new_file( PTK_FILE_BROWSER( file_browser ), FALSE );
}

void
on_preference_activate ( GtkMenuItem *menuitem,
                         gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    fm_main_window_preference( main_window );
}

void fm_main_window_preference( FMMainWindow* main_window )
{
    fm_edit_preference( (GtkWindow*)main_window, PREF_GENERAL );
}


#if 0
void
on_file_assoc_activate ( GtkMenuItem *menuitem,
                         gpointer user_data )
{
    GtkWindow * main_window = GTK_WINDOW( user_data );
    edit_file_associations( main_window );
}
#endif

/* callback used to open default browser when URLs got clicked */
static void open_url( GtkAboutDialog *dlg, const gchar *url, gpointer data)
{
    /* FIXME: is there any better way to do this? */
    char* programs[] = { "xdg-open", "gnome-open" /* Sorry, KDE users. :-P */, "exo-open" };
    int i;
    for(  i = 0; i < G_N_ELEMENTS(programs); ++i)
    {
        char* open_cmd = NULL;
        if( (open_cmd = g_find_program_in_path( programs[i] )) )
        {
             char* cmd = g_strdup_printf( "%s \'%s\'", open_cmd, url );
             g_spawn_command_line_async( cmd, NULL );
             g_free( cmd );
             g_free( open_cmd );
             break;
        }
    }
}

void
on_about_activate ( GtkMenuItem *menuitem,
                    gpointer user_data )
{
    static GtkWidget * about_dlg = NULL;
    GtkBuilder* builder = gtk_builder_new();
    if( ! about_dlg )
    {
        GtkBuilder* builder;
        gtk_about_dialog_set_url_hook( open_url, NULL, NULL);

        pcmanfm_ref();
        builder = _gtk_builder_new_from_file( PACKAGE_UI_DIR "/about-dlg.ui", NULL );
        about_dlg = GTK_WIDGET( gtk_builder_get_object( builder, "dlg" ) );
        g_object_unref( builder );
        gtk_about_dialog_set_version ( GTK_ABOUT_DIALOG ( about_dlg ), VERSION );
        gtk_about_dialog_set_logo_icon_name( GTK_ABOUT_DIALOG ( about_dlg ), "pcmanfm" );

        g_object_add_weak_pointer( G_OBJECT(about_dlg), &about_dlg );
        g_signal_connect( about_dlg, "response", G_CALLBACK(gtk_widget_destroy), NULL );
        g_signal_connect( about_dlg, "destroy", G_CALLBACK(pcmanfm_unref), NULL );
    }
    gtk_window_set_transient_for( GTK_WINDOW( about_dlg ), GTK_WINDOW( user_data ) );
    gtk_window_present( (GtkWindow*)about_dlg );
}


#if 0
gboolean
on_back_btn_popup_menu ( GtkWidget *widget,
                         gpointer user_data )
{
    /*
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( widget );
    */
    return FALSE;
}
#endif


#if 0
gboolean
on_forward_btn_popup_menu ( GtkWidget *widget,
                            gpointer user_data )
{
    /*
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( widget );
    */
    return FALSE;
}
#endif

void fm_main_window_add_new_window( FMMainWindow* main_window,
                                    const char* path,
                                    gboolean open_side_pane,
                                    PtkFBSidePaneMode side_pane_mode )
{
    GtkWidget * new_win = fm_main_window_new();
    FM_MAIN_WINDOW( new_win ) ->splitter_pos = main_window->splitter_pos;
    gtk_window_set_default_size( GTK_WINDOW( new_win ),
                                 GTK_WIDGET( main_window ) ->allocation.width,
                                 GTK_WIDGET( main_window ) ->allocation.height );
    gtk_widget_show( new_win );
    fm_main_window_add_new_tab( FM_MAIN_WINDOW( new_win ), path,
                                open_side_pane, side_pane_mode );
}

void
on_new_window_activate ( GtkMenuItem *menuitem,
                         gpointer user_data )
{
    /*
    * FIXME: There sould be an option to let the users choose wether
    * home dir or current dir should be opened in new windows and new tabs.
    */
    PtkFileBrowser * file_browser;
    const char* path;
    FMMainWindow* main_window = FM_MAIN_WINDOW( user_data );
    file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    path = file_browser ? ptk_file_browser_get_cwd( file_browser ) : g_get_home_dir();
    fm_main_window_add_new_window( main_window, path,
                                   app_settings.show_side_pane,
                                   file_browser->side_pane_mode );
}

void
on_new_tab_activate ( GtkMenuItem *menuitem,
                      gpointer user_data )
{
    /*
    * FIXME: There sould be an option to let the users choose wether
    * home dir or current dir should be opened in new windows and new tabs.
    */
    PtkFileBrowser * file_browser;
    const char* path;
    gboolean show_side_pane;
    PtkFBSidePaneMode side_pane_mode;

    FMMainWindow* main_window = FM_MAIN_WINDOW( user_data );
    file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );

    if ( file_browser )
    {
        path = ptk_file_browser_get_cwd( file_browser );
        show_side_pane = ptk_file_browser_is_side_pane_visible( file_browser );
        side_pane_mode = ptk_file_browser_get_side_pane_mode( file_browser );
    }
    else
    {
        path = g_get_home_dir();
        show_side_pane = app_settings.show_side_pane;
        side_pane_mode = app_settings.side_pane_mode;
    }

    fm_main_window_add_new_tab( main_window, path,
                                show_side_pane, side_pane_mode );
}

static gboolean delayed_focus( GtkWidget* widget )
{
    gdk_threads_enter();
    gtk_widget_grab_focus( widget );
    gdk_threads_leave();
    return FALSE;
}

void
on_folder_notebook_switch_pape ( GtkNotebook *notebook,
                                 GtkNotebookPage *page,
                                 guint page_num,
                                 gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser;
    const char* path;
    char* disp_path;

    file_browser = ( PtkFileBrowser* ) gtk_notebook_get_nth_page( notebook, page_num );

    fm_main_window_update_command_ui( main_window, file_browser );
    fm_main_window_update_status_bar( main_window, file_browser );

    gtk_paned_set_position ( GTK_PANED ( file_browser ), main_window->splitter_pos );

    if( file_browser->dir && (disp_path = file_browser->dir->disp_path) )
    {
        char* disp_name = g_path_get_basename( disp_path );
        gtk_entry_set_text( main_window->address_bar, disp_path );
        gtk_window_set_title( GTK_WINDOW( main_window ), disp_name );
        g_free( disp_name );
    }
    else
    {
        path = ptk_file_browser_get_cwd( file_browser );
        if ( path )
        {
            char* disp_name;
            disp_path = g_filename_display_name( path );
            gtk_entry_set_text( main_window->address_bar, disp_path );
            disp_name = g_path_get_basename( disp_path );
            g_free( disp_path );
            gtk_window_set_title( GTK_WINDOW( main_window ), disp_name );
            g_free( disp_name );
        }
    }
    g_idle_add( ( GSourceFunc ) delayed_focus, file_browser->folder_view );
}


void
on_cut_activate ( GtkMenuItem *menuitem,
                  gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser;
    file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_cut( PTK_FILE_BROWSER( file_browser ) );
}

void
on_copy_activate ( GtkMenuItem *menuitem,
                   gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser;
    file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_copy( PTK_FILE_BROWSER( file_browser ) );
}


void
on_paste_activate ( GtkMenuItem *menuitem,
                    gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser;
    file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_paste( PTK_FILE_BROWSER( file_browser ) );
}


void
on_delete_activate ( GtkMenuItem *menuitem,
                     gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser;

    file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_delete( PTK_FILE_BROWSER( file_browser ) );
}


void
on_select_all_activate ( GtkMenuItem *menuitem,
                         gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_select_all( PTK_FILE_BROWSER( file_browser ) );
}

void
on_edit_bookmark_activate ( GtkMenuItem *menuitem,
                            gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );

    edit_bookmarks( GTK_WINDOW( main_window ) );
}

int bookmark_item_comp( const char* item, const char* path )
{
    return strcmp( ptk_bookmarks_item_get_path( item ), path );
}

void
on_add_to_bookmark_activate ( GtkMenuItem *menuitem,
                              gpointer user_data )
{
    FMMainWindow* main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    const char* path = ptk_file_browser_get_cwd( PTK_FILE_BROWSER( file_browser ) );
    gchar* name;

    if ( ! g_list_find_custom( app_settings.bookmarks->list,
                               path,
                               ( GCompareFunc ) bookmark_item_comp ) )
    {
        name = g_path_get_basename( path );
        ptk_bookmarks_append( name, path );
        g_free( name );
    }
}

void
on_invert_selection_activate ( GtkMenuItem *menuitem,
                               gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_invert_selection( PTK_FILE_BROWSER( file_browser ) );
}


void
on_close_tab_activate ( GtkMenuItem *menuitem,
                        gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkNotebook* notebook = main_window->notebook;
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    gint idx;

    if ( gtk_notebook_get_n_pages ( notebook ) <= 1 )
    {
        fm_main_window_close( GTK_WIDGET( main_window ) );
        return ;
    }
    idx = gtk_notebook_page_num ( GTK_NOTEBOOK( notebook ),
                                  file_browser );
    gtk_notebook_remove_page( notebook, idx );
    if ( !app_settings.always_show_tabs )
        if ( gtk_notebook_get_n_pages ( notebook ) == 1 )
            gtk_notebook_set_show_tabs( notebook, FALSE );
}


void
on_rename_activate ( GtkMenuItem *menuitem,
                     gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    ptk_file_browser_rename_selected_file( PTK_FILE_BROWSER( file_browser ) );
}


void
on_open_side_pane_activate ( GtkMenuItem *menuitem,
                             gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkCheckMenuItem* check = GTK_CHECK_MENU_ITEM( menuitem );
    PtkFileBrowser* file_browser;
    GtkNotebook* nb = main_window->notebook;
    GtkToggleToolButton* btn = main_window->open_side_pane_btn;
    int i;
    int n = gtk_notebook_get_n_pages( nb );

    app_settings.show_side_pane = gtk_check_menu_item_get_active( check );
    g_signal_handlers_block_matched ( btn, G_SIGNAL_MATCH_FUNC,
                                      0, 0, NULL, on_side_pane_toggled, NULL );
    gtk_toggle_tool_button_set_active( btn, app_settings.show_side_pane );
    g_signal_handlers_unblock_matched ( btn, G_SIGNAL_MATCH_FUNC,
                                        0, 0, NULL, on_side_pane_toggled, NULL );

    for ( i = 0; i < n; ++i )
    {
        file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page( nb, i ) );
        if ( app_settings.show_side_pane )
        {
            ptk_file_browser_show_side_pane( file_browser,
                                             file_browser->side_pane_mode );
        }
        else
        {
            ptk_file_browser_hide_side_pane( file_browser );
        }
    }
}


void on_show_dir_tree ( GtkMenuItem *menuitem, gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser;
    int i, n;
    if ( ! GTK_CHECK_MENU_ITEM( menuitem ) ->active )
        return ;
    app_settings.side_pane_mode = PTK_FB_SIDE_PANE_DIR_TREE;

    n = gtk_notebook_get_n_pages( main_window->notebook );
    for ( i = 0; i < n; ++i )
    {
        file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                             main_window->notebook, i ) );
        ptk_file_browser_set_side_pane_mode( file_browser, PTK_FB_SIDE_PANE_DIR_TREE );
    }
}

void on_show_loation_pane ( GtkMenuItem *menuitem, gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser;
    int i, n;
    if ( ! GTK_CHECK_MENU_ITEM( menuitem ) ->active )
        return ;
    app_settings.side_pane_mode = PTK_FB_SIDE_PANE_BOOKMARKS;
    n = gtk_notebook_get_n_pages( main_window->notebook );
    for ( i = 0; i < n; ++i )
    {
        file_browser = PTK_FILE_BROWSER( gtk_notebook_get_nth_page(
                                             main_window->notebook, i ) );
        ptk_file_browser_set_side_pane_mode( file_browser, PTK_FB_SIDE_PANE_BOOKMARKS );
    }
}

void
on_location_bar_activate ( GtkMenuItem *menuitem,
                           gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    app_settings.show_location_bar = gtk_check_menu_item_get_active( GTK_CHECK_MENU_ITEM( menuitem ) );
    if ( app_settings.show_location_bar )
        gtk_widget_show( main_window->toolbar );
    else
        gtk_widget_hide( main_window->toolbar );
}

void
on_show_hidden_activate ( GtkMenuItem *menuitem,
                          gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    app_settings.show_hidden_files = gtk_check_menu_item_get_active( GTK_CHECK_MENU_ITEM( menuitem ) );
    if ( file_browser )
        ptk_file_browser_show_hidden_files( file_browser,
                                            app_settings.show_hidden_files );
}


void
on_sort_by_name_activate ( GtkMenuItem *menuitem,
                           gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    app_settings.sort_order = PTK_FB_SORT_BY_NAME;
    if ( file_browser )
        ptk_file_browser_set_sort_order( PTK_FILE_BROWSER( file_browser ), app_settings.sort_order );
}


void
on_sort_by_size_activate ( GtkMenuItem *menuitem,
                           gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    app_settings.sort_order = PTK_FB_SORT_BY_SIZE;
    if ( file_browser )
        ptk_file_browser_set_sort_order( PTK_FILE_BROWSER( file_browser ), app_settings.sort_order );
}


void
on_sort_by_mtime_activate ( GtkMenuItem *menuitem,
                            gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    app_settings.sort_order = PTK_FB_SORT_BY_MTIME;
    if ( file_browser )
        ptk_file_browser_set_sort_order( PTK_FILE_BROWSER( file_browser ), app_settings.sort_order );
}

void on_sort_by_type_activate ( GtkMenuItem *menuitem,
                                gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    app_settings.sort_order = PTK_FB_SORT_BY_TYPE;
    if ( file_browser )
        ptk_file_browser_set_sort_order( PTK_FILE_BROWSER( file_browser ), app_settings.sort_order );
}

void on_sort_by_perm_activate ( GtkMenuItem *menuitem,
                                gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    app_settings.sort_order = PTK_FB_SORT_BY_PERM;
    if ( file_browser )
        ptk_file_browser_set_sort_order( PTK_FILE_BROWSER( file_browser ), app_settings.sort_order );
}

void on_sort_by_owner_activate ( GtkMenuItem *menuitem,
                                 gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    app_settings.sort_order = PTK_FB_SORT_BY_OWNER;
    if ( file_browser )
        ptk_file_browser_set_sort_order( PTK_FILE_BROWSER( file_browser ), app_settings.sort_order );
}

void
on_sort_ascending_activate ( GtkMenuItem *menuitem,
                             gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    app_settings.sort_type = GTK_SORT_ASCENDING;
    if ( file_browser )
        ptk_file_browser_set_sort_type( PTK_FILE_BROWSER( file_browser ), app_settings.sort_type );
}


void
on_sort_descending_activate ( GtkMenuItem *menuitem,
                              gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    app_settings.sort_type = GTK_SORT_DESCENDING;
    if ( file_browser )
        ptk_file_browser_set_sort_type( PTK_FILE_BROWSER( file_browser ), app_settings.sort_type );
}


void
on_view_as_icon_activate ( GtkMenuItem *menuitem,
                           gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser;
    GtkCheckMenuItem* check_menu = GTK_CHECK_MENU_ITEM( menuitem );
    if ( ! check_menu->active )
        return ;
    file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    app_settings.view_mode = PTK_FB_ICON_VIEW;
    if ( file_browser && GTK_CHECK_MENU_ITEM( menuitem ) ->active )
        ptk_file_browser_view_as_icons( file_browser );
}

void
on_view_as_compact_list_activate ( GtkMenuItem *menuitem,
                           gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser;
    GtkCheckMenuItem* check_menu = GTK_CHECK_MENU_ITEM( menuitem );
    if ( ! check_menu->active )
        return ;
    file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    app_settings.view_mode = PTK_FB_COMPACT_VIEW;
    if ( file_browser && GTK_CHECK_MENU_ITEM( menuitem )->active )
        ptk_file_browser_view_as_compact_list( file_browser );
}

void
on_view_as_list_activate ( GtkMenuItem *menuitem,
                           gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser;
    GtkCheckMenuItem* check_menu = GTK_CHECK_MENU_ITEM( menuitem );
    if ( ! check_menu->active )
        return ;
    file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    app_settings.view_mode = PTK_FB_LIST_VIEW;
    if ( file_browser )
        ptk_file_browser_view_as_list( file_browser );
}


void
on_side_pane_toggled ( GtkToggleToolButton *toggletoolbutton,
                       gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    gtk_check_menu_item_set_active( main_window->open_side_pane_menu,
                                    !app_settings.show_side_pane );
}

void on_file_browser_open_item( PtkFileBrowser* file_browser,
                                const char* path, PtkOpenAction action,
                                FMMainWindow* main_window )
{
    if ( G_LIKELY( path ) )
    {
        switch ( action )
        {
        case PTK_OPEN_DIR:
            ptk_file_browser_chdir( file_browser, path, PTK_FB_CHDIR_ADD_HISTORY );
            break;
        case PTK_OPEN_NEW_TAB:
            file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
            fm_main_window_add_new_tab( main_window, path,
                                        file_browser->show_side_pane,
                                        file_browser->side_pane_mode );
            break;
        case PTK_OPEN_NEW_WINDOW:
            file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
            fm_main_window_add_new_window( main_window, path,
                                           file_browser->show_side_pane,
                                           file_browser->side_pane_mode );
            break;
        case PTK_OPEN_TERMINAL:
            fm_main_window_open_terminal( GTK_WINDOW(main_window), path );
            break;
        case PTK_OPEN_FILE:
            fm_main_window_start_busy_task( main_window );
            g_timeout_add( 1000, ( GSourceFunc ) fm_main_window_stop_busy_task, main_window );
            break;
        }
    }
}

/* FIXME: Dirty hack for fm-desktop.c.
 *        This should be fixed someday. */
void fm_main_window_open_terminal( GtkWindow* parent,
                                   const char* path )
{
    char** argv = NULL;
    int argc = 0;

    if ( !app_settings.terminal )
    {
        ptk_show_error( parent,
                        _("Error"),
                        _( "Terminal program has not been set" ) );
        fm_edit_preference( (GtkWindow*)parent, PREF_ADVANCED );
    }
    if ( app_settings.terminal )
    {
#if 0
        /* FIXME: This should be support in the future once
                  vfs_app_desktop_open_files can accept working dir.
                  This requires API change, and shouldn't be added now.
        */
        VFSAppDesktop* app = vfs_app_deaktop_new( NULL );
        app->exec = app_settings.terminal;
        vfs_app_desktop_execute( app, NULL );
        app->exec = NULL;
        vfs_app_desktop_unref( app );
#endif
        if ( g_shell_parse_argv( app_settings.terminal,
             &argc, &argv, NULL ) )
        {
            vfs_exec_on_screen( gtk_widget_get_screen(GTK_WIDGET(parent)),
                                path, argv, NULL, path,
                                VFS_EXEC_DEFAULT_FLAGS, NULL );
            g_strfreev( argv );
        }
    }
}

void on_open_terminal_activate ( GtkMenuItem *menuitem,
                                 gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    ptk_file_browser_open_terminal( file_browser );
}

void on_find_file_activate ( GtkMenuItem *menuitem,
                                      gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser;
    const char* cwd;
    const char* dirs[2];
    file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    cwd = ptk_file_browser_get_cwd( file_browser );

    dirs[0] = cwd;
    dirs[1] = NULL;
    fm_find_files( dirs );
}

void on_open_current_folder_as_root ( GtkMenuItem *menuitem,
                                      gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser;
    const char* cwd;
    char* cmd_line;
    GError *err = NULL;

    file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    cwd = ptk_file_browser_get_cwd( file_browser );

    cmd_line = g_strdup_printf( "%s --no-desktop '%s'", g_get_prgname(), cwd );

    if( ! vfs_sudo_cmd_async( cwd, cmd_line, &err ) )
    {
        ptk_show_error( GTK_WINDOW( main_window ), _("Error"), err->message );
        g_error_free( err );
    }

    g_free( cmd_line );
}

void on_file_properties_activate ( GtkMenuItem *menuitem,
                                   gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    ptk_file_browser_file_properties( file_browser );
}


void on_bookmark_item_activate ( GtkMenuItem* menu, gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    GtkWidget* file_browser = fm_main_window_get_current_file_browser( main_window );
    char* path = ( char* ) g_object_get_data( G_OBJECT( menu ), "path" );
    if ( !path )
        return ;

    switch ( app_settings.open_bookmark_method )
    {
    case 1:                /* current tab */
        ptk_file_browser_chdir( PTK_FILE_BROWSER( file_browser ), path, PTK_FB_CHDIR_ADD_HISTORY );
        break;
    case 3:                /* new window */
        fm_main_window_add_new_window( main_window, path,
                                       app_settings.show_side_pane,
                                       app_settings.side_pane_mode );
        break;
    case 2:                /* new tab */
        fm_main_window_add_new_tab( main_window, path,
                                    app_settings.show_side_pane,
                                    app_settings.side_pane_mode );
        break;
    }
}

void on_bookmarks_change( PtkBookmarks* bookmarks, FMMainWindow* main_window )
{
    GtkWidget * bookmarks_menu = create_bookmarks_menu( main_window );
    /* FIXME: We have to popupdown the menu first, if it's showed on screen.
      * Otherwise, it's rare but possible that we try to replace the menu while it's in use.
      *  In that way, gtk+ will hang.  So some dirty hack is used here.
      *  We popup down the old menu, if it's currently shown.
      */
    gtk_menu_popdown( (GtkMenu*)gtk_menu_item_get_submenu ( GTK_MENU_ITEM ( main_window->bookmarks ) ) );
    gtk_menu_item_set_submenu ( GTK_MENU_ITEM ( main_window->bookmarks ),
                                bookmarks_menu );
}

GtkWidget* create_bookmarks_menu_item ( FMMainWindow* main_window,
                                        const char* item )
{
    GtkWidget * folder_image;
    GtkWidget* menu_item;
    const char* path;

    menu_item = gtk_image_menu_item_new_with_label( item );
    path = ptk_bookmarks_item_get_path( item );
    g_object_set_data( G_OBJECT( menu_item ), "path", (gpointer) path );
    folder_image = gtk_image_new_from_icon_name( "gnome-fs-directory",
                                                 GTK_ICON_SIZE_MENU );
    gtk_image_menu_item_set_image ( GTK_IMAGE_MENU_ITEM ( menu_item ),
                                    folder_image );
    g_signal_connect( menu_item, "activate",
                      G_CALLBACK( on_bookmark_item_activate ), main_window );
    return menu_item;
}

static PtkMenuItemEntry fm_bookmarks_menu[] = {
                                                  PTK_IMG_MENU_ITEM( N_( "_Add to Bookmarks" ), "gtk-add", on_add_to_bookmark_activate, 0, 0 ),
                                                  PTK_IMG_MENU_ITEM( N_( "_Edit Bookmarks" ), "gtk-edit", on_edit_bookmark_activate, 0, 0 ),
                                                  PTK_SEPARATOR_MENU_ITEM,
                                                  PTK_MENU_END
                                              };

GtkWidget* create_bookmarks_menu ( FMMainWindow* main_window )
{
    GtkWidget * bookmark_menu;
    GtkWidget* menu_item;
    GList* l;

    bookmark_menu = ptk_menu_new_from_data( fm_bookmarks_menu, main_window,
                                            main_window->accel_group );
    for ( l = app_settings.bookmarks->list; l; l = l->next )
    {
        menu_item = create_bookmarks_menu_item( main_window,
                                                ( char* ) l->data );
        gtk_menu_shell_append ( GTK_MENU_SHELL( bookmark_menu ), menu_item );
    }
    gtk_widget_show_all( bookmark_menu );
    return bookmark_menu;
}



void
on_go_btn_clicked ( GtkToolButton *toolbutton,
                    gpointer user_data )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( user_data );
    on_address_bar_activate( GTK_WIDGET( main_window->address_bar ), main_window );
}


gboolean
fm_main_window_key_press_event ( GtkWidget *widget,
                                 GdkEventKey *event )
{
    FMMainWindow * main_window = FM_MAIN_WINDOW( widget );
    gint page;
    GdkModifierType mod = event->state & ( GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK );

    /* Process Alt 1~0 to switch among tabs */
    if ( mod == GDK_MOD1_MASK && event->keyval >= GDK_0 && event->keyval <= GDK_9 )
    {
        if ( event->keyval == GDK_0 )
            page = 9;
        else
            page = event->keyval - GDK_1;
        if ( page < gtk_notebook_get_n_pages( main_window->notebook ) )
            gtk_notebook_set_current_page( main_window->notebook, page );
        return TRUE;
    }
    return GTK_WIDGET_CLASS( parent_class ) ->key_press_event( widget, event );
}

void on_file_browser_before_chdir( PtkFileBrowser* file_browser,
                                   const char* path, gboolean* cancel,
                                   FMMainWindow* main_window )
{
    gchar* disp_path;

    /* don't add busy cursor again if the previous state of file_browser is already busy */
    if( ! file_browser->busy )
        fm_main_window_start_busy_task( main_window );

    if ( fm_main_window_get_current_file_browser( main_window ) == GTK_WIDGET( file_browser ) )
    {
        disp_path = g_filename_display_name( path );
        gtk_entry_set_text( main_window->address_bar, disp_path );
        g_free( disp_path );
        gtk_statusbar_push( GTK_STATUSBAR( main_window->status_bar ), 0, _( "Loading..." ) );
    }

    fm_main_window_update_tab_label( main_window, file_browser, path );
}

static void on_file_browser_begin_chdir( PtkFileBrowser* file_browser,
                                         FMMainWindow* main_window )
{
    gtk_widget_set_sensitive( main_window->back_btn,
                              ptk_file_browser_can_back( file_browser ) );
    gtk_widget_set_sensitive( main_window->forward_btn,
                              ptk_file_browser_can_forward( file_browser ) );
}

void on_file_browser_after_chdir( PtkFileBrowser* file_browser,
                                  FMMainWindow* main_window )
{
    fm_main_window_stop_busy_task( main_window );

    if ( fm_main_window_get_current_file_browser( main_window ) == GTK_WIDGET( file_browser ) )
    {
        char* disp_name = g_path_get_basename( file_browser->dir->disp_path );
        gtk_window_set_title( GTK_WINDOW( main_window ),
                              disp_name );
        g_free( disp_name );
        gtk_entry_set_text( main_window->address_bar, file_browser->dir->disp_path );
        gtk_statusbar_push( GTK_STATUSBAR( main_window->status_bar ), 0, "" );
        fm_main_window_update_command_ui( main_window, file_browser );
    }

    fm_main_window_update_tab_label( main_window, file_browser, file_browser->dir->disp_path );
}

void fm_main_window_update_command_ui( FMMainWindow* main_window,
                                       PtkFileBrowser* file_browser )
{
    if ( ! file_browser )
        file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );

    g_signal_handlers_block_matched( main_window->show_hidden_files_menu,
                                     G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                     on_show_hidden_activate, NULL );
    gtk_check_menu_item_set_active( main_window->show_hidden_files_menu,
                                    file_browser->show_hidden_files );
    g_signal_handlers_unblock_matched( main_window->show_hidden_files_menu,
                                       G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                       on_show_hidden_activate, NULL );

    /* Open side pane */
    g_signal_handlers_block_matched( main_window->open_side_pane_menu,
                                     G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                     on_open_side_pane_activate, NULL );
    gtk_check_menu_item_set_active( main_window->open_side_pane_menu,
                                    ptk_file_browser_is_side_pane_visible( file_browser ) );
    g_signal_handlers_unblock_matched( main_window->open_side_pane_menu,
                                       G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                       on_open_side_pane_activate, NULL );

    g_signal_handlers_block_matched ( main_window->open_side_pane_btn,
                                      G_SIGNAL_MATCH_FUNC,
                                      0, 0, NULL, on_side_pane_toggled, NULL );
    gtk_toggle_tool_button_set_active( main_window->open_side_pane_btn,
                                       ptk_file_browser_is_side_pane_visible( file_browser ) );
    g_signal_handlers_unblock_matched ( main_window->open_side_pane_btn,
                                        G_SIGNAL_MATCH_FUNC,
                                        0, 0, NULL, on_side_pane_toggled, NULL );

    switch ( ptk_file_browser_get_side_pane_mode( file_browser ) )
    {
    case PTK_FB_SIDE_PANE_BOOKMARKS:
        g_signal_handlers_block_matched( main_window->show_location_menu,
                                         G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                         on_show_loation_pane, NULL );
        gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( main_window->show_location_menu ), TRUE );
        g_signal_handlers_unblock_matched( main_window->show_location_menu,
                                           G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                           on_show_loation_pane, NULL );
        break;
    case PTK_FB_SIDE_PANE_DIR_TREE:
        g_signal_handlers_block_matched( main_window->show_dir_tree_menu,
                                         G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                         on_show_dir_tree, NULL );
        gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( main_window->show_dir_tree_menu ), TRUE );
        g_signal_handlers_unblock_matched( main_window->show_dir_tree_menu,
                                           G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                           on_show_dir_tree, NULL );
        break;
    }

    switch ( file_browser->view_mode )
    {
    case PTK_FB_ICON_VIEW:
        g_signal_handlers_block_matched( main_window->view_as_icon,
                                         G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                         on_view_as_icon_activate, NULL );
        gtk_check_menu_item_set_active( main_window->view_as_icon, TRUE );
        g_signal_handlers_unblock_matched( main_window->view_as_icon,
                                           G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                           on_view_as_icon_activate, NULL );
        break;
    case PTK_FB_COMPACT_VIEW:
        g_signal_handlers_block_matched( main_window->view_as_icon,
                                         G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                         on_view_as_icon_activate, NULL );
        gtk_check_menu_item_set_active( main_window->view_as_compact_list, TRUE );
        g_signal_handlers_unblock_matched( main_window->view_as_icon,
                                           G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                           on_view_as_icon_activate, NULL );
        break;
    case PTK_FB_LIST_VIEW:
        g_signal_handlers_block_matched( main_window->view_as_list,
                                         G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                         on_view_as_list_activate, NULL );
        gtk_check_menu_item_set_active( main_window->view_as_list, TRUE );
        g_signal_handlers_unblock_matched( main_window->view_as_list,
                                           G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                           on_view_as_list_activate, NULL );
        break;
    }

    gtk_widget_set_sensitive( main_window->back_btn,
                              ptk_file_browser_can_back( file_browser ) );
    gtk_widget_set_sensitive( main_window->forward_btn,
                              ptk_file_browser_can_forward( file_browser ) );
}

void on_view_menu_popup( GtkMenuItem* item, FMMainWindow* main_window )
{
    PtkFileBrowser * file_browser;

    file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );
    switch ( ptk_file_browser_get_sort_order( file_browser ) )
    {
    case PTK_FB_SORT_BY_NAME:
        gtk_check_menu_item_set_active( main_window->sort_by_name, TRUE );
        break;
    case PTK_FB_SORT_BY_SIZE:
        gtk_check_menu_item_set_active( main_window->sort_by_size, TRUE );
        break;
    case PTK_FB_SORT_BY_MTIME:
        gtk_check_menu_item_set_active( main_window->sort_by_mtime, TRUE );
        break;
    case PTK_FB_SORT_BY_TYPE:
        gtk_check_menu_item_set_active( main_window->sort_by_type, TRUE );
        break;
    case PTK_FB_SORT_BY_PERM:
        gtk_check_menu_item_set_active( main_window->sort_by_perm, TRUE );
        break;
    case PTK_FB_SORT_BY_OWNER:
        gtk_check_menu_item_set_active( main_window->sort_by_owner, TRUE );
        break;
    }

    if ( ptk_file_browser_get_sort_type( file_browser ) == GTK_SORT_ASCENDING )
        gtk_check_menu_item_set_active( main_window->sort_ascending, TRUE );
    else
        gtk_check_menu_item_set_active( main_window->sort_descending, TRUE );
}

void fm_main_window_update_status_bar( FMMainWindow* main_window,
                                       PtkFileBrowser* file_browser )
{
    int n, hn;
    guint64 total_size;
    char *msg;
    char size_str[ 64 ];
    char free_space[100];
#ifdef HAVE_STATVFS
    struct statvfs fs_stat = {0};
#endif

    if ( ! file_browser )
        file_browser = PTK_FILE_BROWSER( fm_main_window_get_current_file_browser( main_window ) );

    free_space[0] = '\0';
#ifdef HAVE_STATVFS
/* FIXME: statvfs support should be moved to src/vfs */

    if( statvfs( ptk_file_browser_get_cwd(file_browser), &fs_stat ) == 0 )
    {
        char total_size_str[ 64 ];
        vfs_file_size_to_string( size_str, fs_stat.f_bsize * fs_stat.f_bavail );
        vfs_file_size_to_string( total_size_str, fs_stat.f_frsize * fs_stat.f_blocks );
        g_snprintf( free_space, G_N_ELEMENTS(free_space),
                    _(", Free space: %s (Total: %s )"), size_str, total_size_str );
    }
#endif

    n = ptk_file_browser_get_n_sel( file_browser, &total_size );

    if ( n > 0 )
    {
        vfs_file_size_to_string( size_str, total_size );
        msg = g_strdup_printf( ngettext( "%d item selected (%s)%s",
                                         "%d items selected (%s)%s", n ), n, size_str, free_space );
        gtk_statusbar_push( GTK_STATUSBAR( main_window->status_bar ), 0, msg );
    }
    else
    {
        /* "%d hidden" means there are %d hidden files in current folder */
        char hidden[128];
        n = ptk_file_browser_get_n_visible_files( file_browser );
        hn = ptk_file_browser_get_n_all_files( file_browser ) - n;
        g_snprintf( hidden, 127, ngettext( "%d hidden", "%d hidden", hn), hn );
        msg = g_strdup_printf( ngettext( "%d visible item (%s)%s",
                                         "%d visible items (%s)%s", n ), n, hidden, free_space );
        gtk_statusbar_push( GTK_STATUSBAR( main_window->status_bar ), 0, msg );
    }
    g_free( msg );
}

void on_file_browser_content_change( PtkFileBrowser* file_browser,
                                     FMMainWindow* main_window )
{
    if ( fm_main_window_get_current_file_browser( main_window ) == GTK_WIDGET( file_browser ) )
    {
        fm_main_window_update_status_bar( main_window, file_browser );
    }
}

void on_file_browser_sel_change( PtkFileBrowser* file_browser,
                                 FMMainWindow* main_window )
{
    if ( fm_main_window_get_current_file_browser( main_window ) == GTK_WIDGET( file_browser ) )
    {
        fm_main_window_update_status_bar( main_window, file_browser );
    }
}

void on_file_browser_pane_mode_change( PtkFileBrowser* file_browser,
                                       FMMainWindow* main_window )
{
    PtkFBSidePaneMode mode;
    GtkCheckMenuItem* check = NULL;

    if ( GTK_WIDGET( file_browser ) != fm_main_window_get_current_file_browser( main_window ) )
        return ;

    mode = ptk_file_browser_get_side_pane_mode( file_browser );
    switch ( mode )
    {
    case PTK_FB_SIDE_PANE_BOOKMARKS:
        check = GTK_CHECK_MENU_ITEM( main_window->show_location_menu );
        break;
    case PTK_FB_SIDE_PANE_DIR_TREE:
        check = GTK_CHECK_MENU_ITEM( main_window->show_dir_tree_menu );
        break;
    }
    gtk_check_menu_item_set_active( check, TRUE );
}

gboolean on_tab_drag_motion ( GtkWidget *widget,
                              GdkDragContext *drag_context,
                              gint x,
                              gint y,
                              guint time,
                              PtkFileBrowser* file_browser )
{
    GtkNotebook * notebook;
    gint idx;

    notebook = GTK_NOTEBOOK( gtk_widget_get_parent( GTK_WIDGET( file_browser ) ) );
    /* TODO: Add a timeout here and don't set current page immediately */
    idx = gtk_notebook_page_num ( notebook, GTK_WIDGET( file_browser ) );
    gtk_notebook_set_current_page( notebook, idx );
    return FALSE;
}

void fm_main_window_start_busy_task( FMMainWindow* main_window )
{
    GdkCursor * cursor;
    if ( 0 == main_window->n_busy_tasks )                /* Their is no busy task */
    {
        /* Create busy cursor */
        cursor = gdk_cursor_new_for_display( gtk_widget_get_display( GTK_WIDGET(main_window) ), GDK_WATCH );
        if ( ! GTK_WIDGET_REALIZED( GTK_WIDGET( main_window ) ) )
            gtk_widget_realize( GTK_WIDGET( main_window ) );
        gdk_window_set_cursor ( GTK_WIDGET( main_window ) ->window, cursor );
        gdk_cursor_unref( cursor );
    }
    ++main_window->n_busy_tasks;
}

/* Return gboolean and it can be used in a timeout callback */
gboolean fm_main_window_stop_busy_task( FMMainWindow* main_window )
{
    --main_window->n_busy_tasks;
    if ( 0 == main_window->n_busy_tasks )                /* Their is no more busy task */
    {
        /* Remove busy cursor */
        gdk_window_set_cursor ( GTK_WIDGET( main_window ) ->window, NULL );
    }
    return FALSE;
}

void on_file_browser_splitter_pos_change( PtkFileBrowser* file_browser,
                                          GParamSpec *param,
                                          FMMainWindow* main_window )
{
    int pos;
    int i, n;
    GtkWidget* page;

    pos = gtk_paned_get_position( GTK_PANED( file_browser ) );
    main_window->splitter_pos = pos;
    n = gtk_notebook_get_n_pages( main_window->notebook );
    for ( i = 0; i < n; ++i )
    {
        page = gtk_notebook_get_nth_page( main_window->notebook, i );
        if ( page == GTK_WIDGET( file_browser ) )
            continue;
        g_signal_handlers_block_matched( page, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                         on_file_browser_splitter_pos_change, NULL );
        gtk_paned_set_position( GTK_PANED( page ), pos );
        g_signal_handlers_unblock_matched( page, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                           on_file_browser_splitter_pos_change, NULL );
    }
}

gboolean on_main_window_focus( GtkWidget* main_window,
                               GdkEventFocus *event,
                               gpointer user_data )
{
    GList * active;
    if ( all_windows->data == ( gpointer ) main_window )
        return FALSE;
    active = g_list_find( all_windows, main_window );
    if ( active )
    {
        all_windows = g_list_remove_link( all_windows, active );
        all_windows->prev = active;
        active->next = all_windows;
        all_windows = active;
    }
    return FALSE;
}

static gboolean
on_main_window_keypress( FMMainWindow* widget, GdkEventKey* event, gpointer user_data)
{
    if (event->state & GDK_CONTROL_MASK)
    {
        switch (event->keyval) {
        case GDK_Tab:
        case GDK_KP_Tab:
        case GDK_ISO_Left_Tab:
            if ( event->state & GDK_SHIFT_MASK )
                fm_main_window_prev_tab( widget );
            else
                fm_main_window_next_tab( widget );
            return TRUE;
        }
    }
    return FALSE;
}

FMMainWindow* fm_main_window_get_last_active()
{
    return all_windows ? FM_MAIN_WINDOW( all_windows->data ) : NULL;
}

const GList* fm_main_window_get_all()
{
    return all_windows;
}
