/*
*  C Interface: ptk-file-archiver
*
* Description: 
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef _PTK_FILE_ARCHIVER_H_
#define _PTK_FILE_ARCHIVER_H_

#include <gtk/gtk.h>
#include <glib.h>

#include "vfs-mime-type.h"

G_BEGIN_DECLS

void ptk_file_archiver_create( GtkWindow* parent_win,
                               const char* working_dir,
                               GList* files );

void ptk_file_archiver_extract( GtkWindow* parent_win,
                                const char* working_dir,
                                GList* files,
                                const char* dest_dir );

gboolean ptk_file_archiver_is_format_supported( VFSMimeType* mime,
                                                gboolean extract );

G_END_DECLS
#endif

