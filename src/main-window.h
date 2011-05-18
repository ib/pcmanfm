#ifndef _MAIN_WINDOW_H_
#define _MAIN_WINDOW_H_

#include <gtk/gtk.h>
#include "ptk-file-browser.h"

G_BEGIN_DECLS

#define FM_TYPE_MAIN_WINDOW             (fm_main_window_get_type())
#define FM_MAIN_WINDOW(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),  FM_TYPE_MAIN_WINDOW, FMMainWindow))
#define FM_MAIN_WINDOW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass),  FM_TYPE_MAIN_WINDOW, FMMainWindowClass))
#define FM_IS_MAIN_WINDOW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FM_TYPE_MAIN_WINDOW))
#define FM_IS_MAIN_WINDOW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass),  FM_TYPE_MAIN_WINDOW))
#define FM_MAIN_WINDOW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj),  FM_TYPE_MAIN_WINDOW, FMMainWindowClass))

typedef struct _FMMainWindow
{
  /* Private */
  GtkWindow parent;

  /* protected */
  GtkWidget *main_vbox;
  GtkWidget *menu_bar;
  GtkWidget* toolbar;
  GtkEntry* address_bar;
  GtkWidget *bookmarks;
  GtkWidget *status_bar;
  GtkNotebook* notebook;

  gint splitter_pos;

  /* Check menu items & tool items */
  GtkCheckMenuItem* open_side_pane_menu;
  GtkCheckMenuItem* show_location_menu;
  GtkCheckMenuItem* show_dir_tree_menu;
  GtkCheckMenuItem* show_location_bar_menu;
  GtkCheckMenuItem* show_hidden_files_menu;

  GtkCheckMenuItem* view_as_icon;
  GtkCheckMenuItem* view_as_compact_list;
  GtkCheckMenuItem* view_as_list;

  GtkCheckMenuItem* sort_by_name;
  GtkCheckMenuItem* sort_by_size;
  GtkCheckMenuItem* sort_by_mtime;
  GtkCheckMenuItem* sort_by_type;
  GtkCheckMenuItem* sort_by_perm;
  GtkCheckMenuItem* sort_by_owner;
  GtkCheckMenuItem* sort_ascending;
  GtkCheckMenuItem* sort_descending;

  GtkToggleToolButton* open_side_pane_btn;
  GtkWidget* back_btn;
  GtkWidget* forward_btn;

  GtkAccelGroup *accel_group;
  GtkTooltips *tooltips;

  GtkWindowGroup* wgroup;
  int n_busy_tasks;
}FMMainWindow;

typedef struct _FMMainWindowClass
{
  GtkWindowClass parent;

}FMMainWindowClass;

GType fm_main_window_get_type (void);

GtkWidget* fm_main_window_new();

/* Utility functions */
GtkWidget* fm_main_window_get_current_file_browser( FMMainWindow* mainWindow );

void fm_main_window_add_new_tab( FMMainWindow* mainWindow,
                                 const char* folder_path,
                                 gboolean open_dir_tree,
                                 PtkFBSidePaneMode side_pane_mode );
                                 
GtkWidget* fm_main_window_create_tab_label( FMMainWindow* main_window,
                                            PtkFileBrowser* file_browser );

void fm_main_window_update_tab_label( FMMainWindow* main_window,
                                      PtkFileBrowser* file_browser,
                                      const char * path );

void fm_main_window_preference( FMMainWindow* main_window );

/* get last active window */
FMMainWindow* fm_main_window_get_last_active();

/* get all windows
 * The returned GList is owned and used internally by FMMainWindow, and
 * should not be freed.
*/
const GList* fm_main_window_get_all();

void fm_main_window_open_terminal( GtkWindow* parent,
                                   const char* path );

G_END_DECLS

#endif
