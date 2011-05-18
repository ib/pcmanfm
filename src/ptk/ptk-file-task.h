/*
*  C Interface: ptk-file-task
*
* Description: 
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifndef _PTK_FILE_TASK_
#define _PTK_FILE_TASK_

#include "vfs-file-task.h"
#include <gtk/gtk.h>

typedef struct _PtkFileTask PtkFileTask;

struct _PtkFileTask
{
    VFSFileTask* task;

    GtkWidget* progress_dlg;
    GtkWindow* parent_window;
    GtkLabel* from;
    GtkLabel* to;
    GtkLabel* current;
    GtkProgressBar* progress;

    /* <private> */
    guint timeout;
    GFunc complete_notify;
    gpointer user_data;
    const char* old_src_file;
    const char* old_dest_file;
    int old_percent;
};

PtkFileTask* ptk_file_task_new( VFSFileTaskType type,
                                GList* src_files,
                                const char* dest_dir,
                                GtkWindow* parent_window );

void ptk_file_task_destroy( PtkFileTask* task );

void ptk_file_task_set_complete_notify( PtkFileTask* task,
                                        GFunc callback,
                                        gpointer user_data );

void ptk_file_task_set_chmod( PtkFileTask* task,
                              guchar* chmod_actions );

void ptk_file_task_set_chown( PtkFileTask* task,
                              uid_t uid, gid_t gid );

void ptk_file_task_set_recursive( PtkFileTask* task, gboolean recursive );

void ptk_file_task_run( PtkFileTask* task );

void ptk_file_task_cancel( PtkFileTask* task );

#endif

