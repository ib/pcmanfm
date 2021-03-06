/*
*  C Interface: ptk-clipboard
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef _PTK_CLIPBOARD_H_
#define _PTK_CLIPBOARD_H_

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

void ptk_clipboard_cut_or_copy_files( const char* working_dir,
                                      GList* files,
                                      gboolean copy );

void ptk_clipboard_paste_files( GtkWindow* parent_win,
                                const char* dest_dir );

G_END_DECLS

#endif
