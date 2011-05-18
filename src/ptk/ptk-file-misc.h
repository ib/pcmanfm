/*
*  C Interface: ptk-file-misc
*
* Description: Miscellaneous GUI-realated functions for files
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef _PTK_FILE_MISC_H_
#define _PTK_FILE_MISC_H_

#include <gtk/gtk.h>
#include "vfs-file-info.h"
#include "ptk-file-browser.h"

G_BEGIN_DECLS

void ptk_delete_files( GtkWindow* parent_win,
                       const char* cwd,
                       GList* sel_files );

gboolean  ptk_rename_file( GtkWindow* parent_win,
                      const char* cwd,
                      VFSFileInfo* file );

gboolean ptk_create_new_file( GtkWindow* parent_win,
                          const char* cwd,
                          gboolean create_folder,
                          VFSFileInfo** file );

void ptk_show_file_properties( GtkWindow* parent_win,
                               const char* cwd,
                               GList* sel_files );

/*
 * sel_files is a list of VFSFileInfo
 * app_desktop is the application used to open the files.
 * If app_desktop == NULL, each file will be opened with its
 * default application.
 */
void ptk_open_files_with_app( const char* cwd,
                              GList* sel_files,
                              char* app_desktop,
                              PtkFileBrowser* file_browser );

G_END_DECLS

#endif

