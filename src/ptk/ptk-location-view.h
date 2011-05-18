/*
*  C Interface: PtkLocationView
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef  _PTK_LOCATION_VIEW_H_
#define  _PTK_LOCATION_VIEW_H_

#include <gtk/gtk.h>
#include <sys/types.h>

#include "vfs-volume.h"

G_BEGIN_DECLS

/* Create a new location view */
GtkWidget* ptk_location_view_new();

gboolean ptk_location_view_chdir( GtkTreeView* location_view, const char* path );

char* ptk_location_view_get_selected_dir( GtkTreeView* location_view );

gboolean ptk_location_view_is_item_bookmark( GtkTreeView* location_view,
                                             GtkTreeIter* it );

void ptk_location_view_rename_selected_bookmark( GtkTreeView* location_view );

gboolean ptk_location_view_is_item_volume(  GtkTreeView* location_view, GtkTreeIter* it );

VFSVolume* ptk_location_view_get_volume(  GtkTreeView* location_view, GtkTreeIter* it );

void ptk_location_view_show_trash_can( gboolean show );

G_END_DECLS

#endif
