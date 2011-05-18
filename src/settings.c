/*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "settings.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include "glib-utils.h" /* for g_mkdir_with_parents() */

#include <gtk/gtk.h>
#include "ptk-file-browser.h"

#include "desktop.h"

/* Dirty hack: check whether we are under LXDE or not */
#define is_under_LXDE()     (g_getenv( "_LXSESSION_PID" ) != NULL)

AppSettings app_settings = {0};
/* const gboolean singleInstance_default = TRUE; */
const gboolean show_hidden_files_default = FALSE;
const gboolean show_side_pane_default = TRUE;
const int side_pane_mode_default = PTK_FB_SIDE_PANE_BOOKMARKS;
const gboolean show_thumbnail_default = TRUE;
const int max_thumb_size_default = 1 << 20;
const int big_icon_size_default = 48;
const int small_icon_size_default = 20;
const gboolean single_click_default = FALSE;
const gboolean show_location_bar_default = TRUE;

/* FIXME: temporarily disable trash since it's not finished */
const gboolean use_trash_can_default = FALSE;
const int open_bookmark_method_default = 1;
const int view_mode_default = PTK_FB_ICON_VIEW;
const int sort_order_default = PTK_FB_SORT_BY_NAME;
const int sort_type_default = GTK_SORT_ASCENDING;

gboolean show_desktop_default = FALSE;
const gboolean show_wallpaper_default = FALSE;
const WallpaperMode wallpaper_mode_default=WPM_STRETCH;
const GdkColor desktop_bg1_default={0};
const GdkColor desktop_bg2_default={0};
const GdkColor desktop_text_default={0, 65535, 65535, 65535};
const GdkColor desktop_shadow_default={0};
const int desktop_sort_by_default = DW_SORT_BY_MTIME;
const int desktop_sort_type_default = GTK_SORT_ASCENDING;
const gboolean show_wm_menu_default = FALSE;

/* Default values of interface settings */
const gboolean always_show_tabs_default = FALSE;
const gboolean hide_close_tab_buttons_default = FALSE;
const gboolean hide_side_pane_buttons_default = FALSE;
const gboolean hide_folder_content_border_default = FALSE;

const gboolean use_si_prefix_default = TRUE;

typedef void ( *SettingsParseFunc ) ( char* line );

static void color_from_str( GdkColor* ret, const char* value );
static void save_color( FILE* file, const char* name,
                 GdkColor* color );

static void parse_general_settings( char* line )
{
    char * sep = strstr( line, "=" );
    char* name;
    char* value;
    if ( !sep )
        return ;
    name = line;
    value = sep + 1;
    *sep = '\0';
    if ( 0 == strcmp( name, "encoding" ) )
        strcpy( app_settings.encoding, value );
    else if ( 0 == strcmp( name, "show_hidden_files" ) )
        app_settings.show_hidden_files = atoi( value );
    else if ( 0 == strcmp( name, "show_side_pane" ) )
        app_settings.show_side_pane = atoi( value );
    else if ( 0 == strcmp( name, "side_pane_mode" ) )
        app_settings.side_pane_mode = atoi( value );
    else if ( 0 == strcmp( name, "show_thumbnail" ) )
        app_settings.show_thumbnail = atoi( value );
    else if ( 0 == strcmp( name, "max_thumb_size" ) )
        app_settings.max_thumb_size = atoi( value ) << 10;
    else if ( 0 == strcmp( name, "big_icon_size" ) )
    {
        app_settings.big_icon_size = atoi( value );
        if( app_settings.big_icon_size <= 0 || app_settings.big_icon_size > 128 )
            app_settings.big_icon_size = big_icon_size_default;
    }
    else if ( 0 == strcmp( name, "small_icon_size" ) )
    {
        app_settings.small_icon_size = atoi( value );
        if( app_settings.small_icon_size <= 0 || app_settings.small_icon_size > 128 )
            app_settings.small_icon_size = small_icon_size_default;
    }
    /* FIXME: temporarily disable trash since it's not finished */
#if 0
    else if ( 0 == strcmp( name, "use_trash_can" ) )
        app_settings.use_trash_can = atoi(value);
#endif
    else if ( 0 == strcmp( name, "single_click" ) )
        app_settings.single_click = atoi(value);
    else if ( 0 == strcmp( name, "view_mode" ) )
        app_settings.view_mode = atoi( value );
    else if ( 0 == strcmp( name, "sort_order" ) )
        app_settings.sort_order = atoi( value );
    else if ( 0 == strcmp( name, "sort_type" ) )
        app_settings.sort_type = atoi( value );
    else if ( 0 == strcmp( name, "open_bookmark_method" ) )
        app_settings.open_bookmark_method = atoi( value );
/*
    else if ( 0 == strcmp( name, "iconTheme" ) )
    {
        if ( value && *value )
            app_settings.iconTheme = strdup( value );
    }
*/
    else if ( 0 == strcmp( name, "terminal" ) )
    {
        if ( value && *value )
            app_settings.terminal = strdup( value );
    }
    else if ( 0 == strcmp( name, "use_si_prefix" ) )
        app_settings.use_si_prefix = atoi( value );
    /*
    else if ( 0 == strcmp( name, "singleInstance" ) )
        app_settings.singleInstance = atoi( value );
    */
    else if ( 0 == strcmp( name, "show_location_bar" ) )
        app_settings.show_location_bar = atoi( value );
}

static void color_from_str( GdkColor* ret, const char* value )
{
    sscanf( value, "%hu,%hu,%hu",
            &ret->red, &ret->green, &ret->blue );
}

static void save_color( FILE* file, const char* name, GdkColor* color )
{
    fprintf( file, "%s=%d,%d,%d\n", name,
             color->red, color->green, color->blue );
}

static void parse_window_state( char* line )
{
    char * sep = strstr( line, "=" );
    char* name;
    char* value;
    int v;
    if ( !sep )
        return ;
    name = line;
    value = sep + 1;
    *sep = '\0';
    if ( 0 == strcmp( name, "splitter_pos" ) )
    {
        v = atoi( value );
        app_settings.splitter_pos = ( v > 0 ? v : 160 );
    }
    if ( 0 == strcmp( name, "width" ) )
    {
        v = atoi( value );
        app_settings.width = ( v > 0 ? v : 640 );
    }
    if ( 0 == strcmp( name, "height" ) )
    {
        v = atoi( value );
        app_settings.height = ( v > 0 ? v : 480 );
    }
    if ( 0 == strcmp( name, "maximized" ) )
    {
        app_settings.maximized = atoi( value );
    }
}

static void parse_desktop_settings( char* line )
{
    char * sep = strstr( line, "=" );
    char* name;
    char* value;
    if ( !sep )
        return ;
    name = line;
    value = sep + 1;
    *sep = '\0';
    if ( 0 == strcmp( name, "show_desktop" ) )
        app_settings.show_desktop = atoi( value );
    else if ( 0 == strcmp( name, "show_wallpaper" ) )
        app_settings.show_wallpaper = atoi( value );
    else if ( 0 == strcmp( name, "wallpaper" ) )
        app_settings.wallpaper = g_strdup( value );
    else if ( 0 == strcmp( name, "wallpaper_mode" ) )
        app_settings.wallpaper_mode = atoi( value );
    else if ( 0 == strcmp( name, "bg1" ) )
        color_from_str( &app_settings.desktop_bg1, value );
    else if ( 0 == strcmp( name, "bg2" ) )
        color_from_str( &app_settings.desktop_bg2, value );
    else if ( 0 == strcmp( name, "text" ) )
        color_from_str( &app_settings.desktop_text, value );
    else if ( 0 == strcmp( name, "shadow" ) )
        color_from_str( &app_settings.desktop_shadow, value );
    else if ( 0 == strcmp( name, "sort_by" ) )
        app_settings.desktop_sort_by = atoi( value );
    else if ( 0 == strcmp( name, "sort_type" ) )
        app_settings.desktop_sort_type = atoi( value );
    else if ( 0 == strcmp( name, "show_wm_menu" ) )
        app_settings.show_wm_menu = atoi( value );
}

static void parse_interface_settings( char* line )
{
    char * sep = strstr( line, "=" );
    char* name;
    char* value;
    if ( !sep )
        return ;
    name = line;
    value = sep + 1;
    *sep = '\0';
    if ( 0 == strcmp( name, "always_show_tabs" ) )
        app_settings.always_show_tabs = atoi( value );
    else if ( 0 == strcmp( name, "show_close_tab_buttons" ) )
        app_settings.hide_close_tab_buttons = !atoi( value );
    else if ( 0 == strcmp( name, "hide_side_pane_buttons" ) )
        app_settings.hide_side_pane_buttons = atoi( value );
    else if ( 0 == strcmp( name, "hide_folder_content_border" ) )
        app_settings.hide_folder_content_border = atoi( value );
}

void load_settings()
{
    FILE * file;
    gchar* path;
    char line[ 1024 ];
    char* section_name;
    SettingsParseFunc func = NULL;

    /* set default value */

    /* General */
    /* app_settings.show_desktop = show_desktop_default; */
    app_settings.show_wallpaper = show_wallpaper_default;
    app_settings.wallpaper = NULL;
    app_settings.desktop_bg1 = desktop_bg1_default;
    app_settings.desktop_bg2 = desktop_bg2_default;
    app_settings.desktop_text = desktop_text_default;
    app_settings.desktop_sort_by = desktop_sort_by_default;
    app_settings.desktop_sort_type = desktop_sort_type_default;
    app_settings.show_wm_menu = show_wm_menu_default;

    app_settings.encoding[ 0 ] = '\0';
    app_settings.show_hidden_files = show_hidden_files_default;
    app_settings.show_side_pane = show_side_pane_default;
    app_settings.side_pane_mode = side_pane_mode_default;
    app_settings.show_thumbnail = show_thumbnail_default;
    app_settings.max_thumb_size = max_thumb_size_default;
    app_settings.big_icon_size = big_icon_size_default;
    app_settings.small_icon_size = small_icon_size_default;
    app_settings.use_trash_can = use_trash_can_default;
    app_settings.view_mode = view_mode_default;
    app_settings.open_bookmark_method = open_bookmark_method_default;
    /* app_settings.iconTheme = NULL; */
    app_settings.terminal = NULL;
    app_settings.use_si_prefix = use_si_prefix_default;
    app_settings.show_location_bar = show_location_bar_default;

    /* Interface */
    app_settings.always_show_tabs = always_show_tabs_default;
    app_settings.hide_close_tab_buttons = hide_close_tab_buttons_default;
    app_settings.hide_side_pane_buttons = hide_side_pane_buttons_default;
    app_settings.hide_folder_content_border = hide_folder_content_border_default;

    /* Window State */
    app_settings.splitter_pos = 160;
    app_settings.width = 640;
    app_settings.height = 480;

    /* load settings */

    /* Dirty hacks for LXDE */
    if( is_under_LXDE() )
    {
        show_desktop_default = app_settings.show_desktop = TRUE;   /* show the desktop by default */
        path = g_build_filename( g_get_user_config_dir(), "pcmanfm/main.lxde", NULL );
    }
    else
    {
        app_settings.show_desktop = show_desktop_default;
        path = g_build_filename( g_get_user_config_dir(), "pcmanfm/main", NULL );
    }

    file = fopen( path, "r" );
    g_free( path );
    if ( file )
    {
        while ( fgets( line, sizeof( line ), file ) )
        {
            strtok( line, "\r\n" );
            if ( ! line[ 0 ] )
                continue;
            if ( line[ 0 ] == '[' )
            {
                section_name = strtok( line, "]" );
                if ( 0 == strcmp( line + 1, "General" ) )
                    func = &parse_general_settings;
                else if ( 0 == strcmp( line + 1, "Window" ) )
                    func = &parse_window_state;
                else if ( 0 == strcmp( line + 1, "Interface" ) )
                    func = &parse_interface_settings;
                else if ( 0 == strcmp( line + 1, "Desktop" ) )
                    func = &parse_desktop_settings;
                else
                    func = NULL;
                continue;
            }
            if ( func )
                ( *func ) ( line );
        }
        fclose( file );
    }

    if ( app_settings.encoding[ 0 ] )
    {
        setenv( "G_FILENAME_ENCODING", app_settings.encoding, 1 );
    }

    /* Load bookmarks */
    /* Don't load bookmarks here since we won't use it in some cases */
    /* app_settings.bookmarks = ptk_bookmarks_get(); */
}


void save_settings()
{
    FILE * file;
    gchar* path;

    /* save settings */
    path = g_build_filename( g_get_user_config_dir(), "pcmanfm", NULL );

    if ( ! g_file_test( path, G_FILE_TEST_EXISTS ) )
        g_mkdir_with_parents( path, 0766 );

    chdir( path );
    g_free( path );

    /* Dirty hacks for LXDE */
    file = fopen( is_under_LXDE() ? "main.lxde" : "main", "w" );

    if ( file )
    {
        /* General */
        fputs( "[General]\n", file );
        /*
        if ( app_settings.singleInstance != singleInstance_default )
            fprintf( file, "singleInstance=%d\n", !!app_settings.singleInstance );
        */
        if ( app_settings.encoding[ 0 ] )
            fprintf( file, "encoding=%s\n", app_settings.encoding );
        if ( app_settings.show_hidden_files != show_hidden_files_default )
            fprintf( file, "show_hidden_files=%d\n", !!app_settings.show_hidden_files );
        if ( app_settings.show_side_pane != show_side_pane_default )
            fprintf( file, "show_side_pane=%d\n", app_settings.show_side_pane );
        if ( app_settings.side_pane_mode != side_pane_mode_default )
            fprintf( file, "side_pane_mode=%d\n", app_settings.side_pane_mode );
        if ( app_settings.show_thumbnail != show_thumbnail_default )
            fprintf( file, "show_thumbnail=%d\n", !!app_settings.show_thumbnail );
        if ( app_settings.max_thumb_size != max_thumb_size_default )
            fprintf( file, "max_thumb_size=%d\n", app_settings.max_thumb_size >> 10 );
        if ( app_settings.big_icon_size != big_icon_size_default )
            fprintf( file, "big_icon_size=%d\n", app_settings.big_icon_size );
        if ( app_settings.small_icon_size != small_icon_size_default )
            fprintf( file, "small_icon_size=%d\n", app_settings.small_icon_size );
        /* FIXME: temporarily disable trash since it's not finished */
#if 0
        if ( app_settings.use_trash_can != use_trash_can_default )
            fprintf( file, "use_trash_can=%d\n", app_settings.use_trash_can );
#endif
        if ( app_settings.single_click != single_click_default )
            fprintf( file, "single_click=%d\n", app_settings.single_click );
        if ( app_settings.view_mode != view_mode_default )
            fprintf( file, "view_mode=%d\n", app_settings.view_mode );
        if ( app_settings.sort_order != sort_order_default )
            fprintf( file, "sort_order=%d\n", app_settings.sort_order );
        if ( app_settings.sort_type != sort_type_default )
            fprintf( file, "sort_type=%d\n", app_settings.sort_type );
        if ( app_settings.open_bookmark_method != open_bookmark_method_default )
            fprintf( file, "open_bookmark_method=%d\n", app_settings.open_bookmark_method );
        /*
        if ( app_settings.iconTheme )
            fprintf( file, "iconTheme=%s\n", app_settings.iconTheme );
        */
        if ( app_settings.terminal )
            fprintf( file, "terminal=%s\n", app_settings.terminal );
        if ( app_settings.use_si_prefix != use_si_prefix_default )
            fprintf( file, "use_si_prefix=%d\n", !!app_settings.use_si_prefix );
        if ( app_settings.show_location_bar != show_location_bar_default )
            fprintf( file, "show_location_bar=%d\n", app_settings.show_location_bar );

        fputs( "\n[Window]\n", file );
        fprintf( file, "width=%d\n", app_settings.width );
        fprintf( file, "height=%d\n", app_settings.height );
        fprintf( file, "splitter_pos=%d\n", app_settings.splitter_pos );
        fprintf( file, "maximized=%d\n", app_settings.maximized );

        /* Desktop */
        fputs( "\n[Desktop]\n", file );
        if ( app_settings.show_desktop != show_desktop_default )
            fprintf( file, "show_desktop=%d\n", !!app_settings.show_desktop );
        if ( app_settings.show_wallpaper != show_wallpaper_default )
            fprintf( file, "show_wallpaper=%d\n", !!app_settings.show_wallpaper );
        if ( app_settings.wallpaper && app_settings.wallpaper[ 0 ] )
            fprintf( file, "wallpaper=%s\n", app_settings.wallpaper );
        if ( app_settings.wallpaper_mode != wallpaper_mode_default )
            fprintf( file, "wallpaper_mode=%d\n", app_settings.wallpaper_mode );
        if ( app_settings.desktop_sort_by != desktop_sort_by_default )
            fprintf( file, "sort_by=%d\n", app_settings.desktop_sort_by );
        if ( app_settings.desktop_sort_type != desktop_sort_type_default )
            fprintf( file, "sort_type=%d\n", app_settings.desktop_sort_type );
        if ( app_settings.show_wm_menu != show_wm_menu_default )
            fprintf( file, "show_wm_menu=%d\n", app_settings.show_wm_menu );
        if ( ! gdk_color_equal( &app_settings.desktop_bg1,
               &desktop_bg1_default ) )
            save_color( file, "bg1",
                        &app_settings.desktop_bg1 );
        if ( ! gdk_color_equal( &app_settings.desktop_bg2,
               &desktop_bg2_default ) )
            save_color( file, "bg2",
                        &app_settings.desktop_bg2 );
        if ( ! gdk_color_equal( &app_settings.desktop_text,
               &desktop_text_default ) )
            save_color( file, "text",
                        &app_settings.desktop_text );
        if ( ! gdk_color_equal( &app_settings.desktop_shadow,
               &desktop_shadow_default ) )
            save_color( file, "shadow",
                        &app_settings.desktop_shadow );

        /* Interface */
        fputs( "[Interface]\n", file );
        if ( app_settings.always_show_tabs != always_show_tabs_default )
            fprintf( file, "always_show_tabs=%d\n", app_settings.always_show_tabs );
        if ( app_settings.hide_close_tab_buttons != hide_close_tab_buttons_default )
            fprintf( file, "show_close_tab_buttons=%d\n", !app_settings.hide_close_tab_buttons );
        if ( app_settings.hide_side_pane_buttons != hide_side_pane_buttons_default )
            fprintf( file, "hide_side_pane_buttons=%d\n", app_settings.hide_side_pane_buttons );
        if ( app_settings.hide_folder_content_border != hide_folder_content_border_default )
            fprintf( file, "hide_folder_content_border=%d\n", app_settings.hide_folder_content_border );

        fclose( file );
    }

    /* Save bookmarks */
    ptk_bookmarks_save();
}

void free_settings()
{
/*
    if ( app_settings.iconTheme )
        g_free( app_settings.iconTheme );
*/
    g_free( app_settings.terminal );
    g_free( app_settings.wallpaper );

    ptk_bookmarks_unref();
}
