/*
*  C Interface: ptk-file-menu
*
* Description: Popup menu for files
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef _PTK_FILE_MENU_H_
#define _PTK_FILE_MENU_H_

#include <gtk/gtk.h>
#include "ptk-file-browser.h"
#include "vfs-file-info.h"

G_BEGIN_DECLS

/* sel_files is a list containing VFSFileInfo structures
  * The list will be freed in this function, so the caller mustn't
  * free the list after calling this function.
  */
GtkWidget* ptk_file_menu_new( const char* file_path,
                              VFSFileInfo* info, const char* cwd,
                              GList* sel_files, PtkFileBrowser* browser );
G_END_DECLS

#endif

