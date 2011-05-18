/*
*  C Interface: vfs-file-task
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2005
*
* Copyright: See COPYING file that comes with this distribution
*
*/
#ifndef  _VFS_FILE_TASK_H
#define  _VFS_FILE_TASK_H

#include <glib.h>
#include <sys/types.h>

typedef enum
{
    VFS_FILE_TASK_MOVE = 0,
    VFS_FILE_TASK_COPY,
    VFS_FILE_TASK_TRASH,
    VFS_FILE_TASK_DELETE,
    VFS_FILE_TASK_LINK,             /* will be supported in the future */
    VFS_FILE_TASK_CHMOD_CHOWN,         /* These two kinds of operation have lots in common,
                                        * so put them together to reduce duplicated disk I/O */
    VFS_FILE_TASK_LAST
}VFSFileTaskType;

typedef enum {
    OWNER_R = 0,
    OWNER_W,
    OWNER_X,
    GROUP_R,
    GROUP_W,
    GROUP_X,
    OTHER_R,
    OTHER_W,
    OTHER_X,
    SET_UID,
    SET_GID,
    STICKY,
    N_CHMOD_ACTIONS
}ChmodActionType;

extern const mode_t chmod_flags[];

struct _VFSFileTask;

typedef enum
{
    VFS_FILE_TASK_RUNNING,
    VFS_FILE_TASK_QUERY_ABORT,
    VFS_FILE_TASK_QUERY_OVERWRITE,
    VFS_FILE_TASK_ERROR,
    VFS_FILE_TASK_ABORTED,
    VFS_FILE_TASK_FINISH
}VFSFileTaskState;

typedef enum
{
    VFS_FILE_TASK_OVERWRITE, /* Overwrite current dest file */
    VFS_FILE_TASK_OVERWRITE_ALL, /* Overwrite all existing files without prompt */
    VFS_FILE_TASK_SKIP, /* Don't overwrite current file */
    VFS_FILE_TASK_SKIP_ALL, /* Don't try to overwrite any files */
    VFS_FILE_TASK_RENAME /* Rename file */
}VFSFileTaskOverwriteMode;

typedef struct _VFSFileTask VFSFileTask;

typedef void ( *VFSFileTaskProgressCallback ) ( VFSFileTask* task,
                                                int percent,
                                                const char* src_file,
                                                const char* dest_file,
                                                gpointer user_data );

typedef gboolean ( *VFSFileTaskStateCallback ) ( VFSFileTask*,
                                                 VFSFileTaskState state,
                                                 gpointer state_data,
                                                 gpointer user_data );

struct _VFSFileTask
{
    VFSFileTaskType type;
    GList* src_paths; /* All source files. This list will be freed
                                 after file operation is completed. */
    char* dest_dir; /* Destinaton directory */

    VFSFileTaskOverwriteMode overwrite_mode ;
    gboolean recursive; /* Apply operation to all files under folders
        * recursively. This is default to copy/delete,
        * and should be set manually for chown/chmod. */

    /* For chown */
    uid_t uid;
    gid_t gid;

    /* For chmod */
    guchar *chmod_actions;  /* If chmod is not needed, this should be NULL */

    off_t total_size; /* Total size of the files to be processed, in bytes */
    off_t progress; /* Total size of current processed files, in btytes */
    int percent; /* progress (percentage) */

    const char* current_file; /* Current processed file */
    const char* current_dest; /* Current destination file */

    int error;

    GThread* thread;
    VFSFileTaskState state;

    VFSFileTaskProgressCallback progress_cb;
    gpointer progress_cb_data;

    VFSFileTaskStateCallback state_cb;
    gpointer state_cb_data;
};

/*
* source_files sould be a newly allocated list, and it will be
* freed after file operation has been completed
*/
VFSFileTask* vfs_task_new ( VFSFileTaskType task_type,
                            GList* src_files,
                            const char* dest_dir );

/* Set some actions for chmod, this array will be copied
* and stored in VFSFileTask */
void vfs_file_task_set_chmod( VFSFileTask* task,
                              guchar* chmod_actions );

void vfs_file_task_set_chown( VFSFileTask* task,
                              uid_t uid, gid_t gid );

void vfs_file_task_set_progress_callback( VFSFileTask* task,
                                          VFSFileTaskProgressCallback cb,
                                          gpointer user_data );

void vfs_file_task_set_state_callback( VFSFileTask* task,
                                       VFSFileTaskStateCallback cb,
                                       gpointer user_data );

void vfs_file_task_set_recursive( VFSFileTask* task, gboolean recursive );

void vfs_file_task_set_overwrite_mode( VFSFileTask* task,
                                       VFSFileTaskOverwriteMode mode );

void vfs_file_task_run ( VFSFileTask* task );


void vfs_file_task_try_abort ( VFSFileTask* task );

void vfs_file_task_abort ( VFSFileTask* task );

void vfs_file_task_free ( VFSFileTask* task );

#endif
