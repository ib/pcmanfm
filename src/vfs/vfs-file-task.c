/*
*  C Implementation: vfs-file-task
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2005
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "vfs-file-task.h"

#include <unistd.h>
#include <fcntl.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <glib.h>
#include "glib-mem.h"
#include "glib-utils.h"
#include <glib/gi18n.h>

#include <stdio.h>
#include <stdlib.h> /* for mkstemp */
#include <string.h>
#include <errno.h>

#include "vfs-dir.h"

const mode_t chmod_flags[] =
    {
        S_IRUSR, S_IWUSR, S_IXUSR,
        S_IRGRP, S_IWGRP, S_IXGRP,
        S_IROTH, S_IWOTH, S_IXOTH,
        S_ISUID, S_ISGID, S_ISVTX
    };

/*
* void get_total_size_of_dir( const char* path, off_t* size )
* Recursively count total size of all files in the specified directory.
* If the path specified is a file, the size of the file is directly returned.
* cancel is used to cancel the operation. This function will check the value
* pointed by cancel in every iteration. If cancel is set to TRUE, the
* calculation is cancelled.
* NOTE: *size should be set to zero before calling this function.
*/
static void get_total_size_of_dir( VFSFileTask* task,
                                   const char* path,
                                   off_t* size );

static gboolean
call_progress_callback( VFSFileTask* task )
{
    /* FIXME: This has some impact on performance */
    gdouble percent = ( ( gdouble ) task->progress ) / task->total_size;
    int ipercent = ( int ) ( percent * 100 );

    if ( ipercent != task->percent )
        task->percent = ipercent;

    if ( task->progress_cb )
    {
        task->progress_cb( task, task->percent,
                           task->current_file,
                           NULL,
                           task->progress_cb_data );
    }
    return FALSE;
}

static void call_state_callback( VFSFileTask* task,
                          VFSFileTaskState state )
{
    task->state = state;
    if ( task->state_cb )
    {
        if ( ! task->state_cb( task, state, NULL, task->state_cb_data ) )
            task->state = VFS_FILE_TASK_ABORTED;
        else
            task->state = VFS_FILE_TASK_RUNNING;
    }
}

static gboolean should_abort( VFSFileTask* task )
{
    if ( task->state == VFS_FILE_TASK_QUERY_ABORT )
    {
        call_state_callback( task, VFS_FILE_TASK_QUERY_ABORT );
        if ( task->state == VFS_FILE_TASK_ABORTED )
            return TRUE;
    }
    return ( task->state == VFS_FILE_TASK_ABORTED );
}


/*
* Check if the destination file exists.
* If the dest_file exists, let the user choose a new destination,
* skip this file, overwrite existing file, or cancel all file operation.
* The returned string is the new destination file choosed by the user
*/
static
gboolean check_overwrite( VFSFileTask* task,
                          const gchar* dest_file,
                          gboolean* dest_exists,
                          char** new_dest_file )
{
    char * new_dest;
    new_dest = *new_dest_file = NULL;
    struct stat dest_stat;

    if ( task->overwrite_mode == VFS_FILE_TASK_OVERWRITE_ALL )
    {
        *dest_exists = !lstat( dest_file, &dest_stat );
        return TRUE;
    }
    if ( task->overwrite_mode == VFS_FILE_TASK_SKIP_ALL )
    {
        *dest_exists = !lstat( dest_file, &dest_stat );
        return FALSE;
    }

    *dest_exists = FALSE;
    if ( task->state_cb )
    {
        while ( lstat( dest_file, &dest_stat ) != -1 )
        {
            *dest_exists = TRUE;
            /* destination file exists */
            task->state = VFS_FILE_TASK_QUERY_OVERWRITE;

            if ( ! task->state_cb( task, VFS_FILE_TASK_QUERY_OVERWRITE,
                                   &new_dest, task->state_cb_data ) )
            {
                task->state = VFS_FILE_TASK_ABORTED;
            }
            else
            {
                task->state = VFS_FILE_TASK_RUNNING;
            }

            if ( should_abort( task ) )
                return FALSE;
            if( task->overwrite_mode != VFS_FILE_TASK_RENAME )
            {
                g_free( new_dest );
                *new_dest_file = NULL;
                if( task->overwrite_mode == VFS_FILE_TASK_OVERWRITE ||
                    task->overwrite_mode == VFS_FILE_TASK_OVERWRITE_ALL )
                {
                    return TRUE;
                }
                else
                    return FALSE;
            }
            if ( new_dest )
                dest_file = new_dest;
        }
        *dest_exists = FALSE;
        *new_dest_file = new_dest;
    }
    return ! should_abort( task );
}

static void
vfs_file_task_do_copy( VFSFileTask* task,
                       const char* src_file,
                       const char* dest_file )
{
    GDir * dir;
    const gchar* file_name;
    gchar* sub_src_file;
    gchar* sub_dest_file;
    struct stat file_stat;
    char buffer[ 4096 ];
    int rfd;
    int wfd;
    ssize_t rsize;
    char* new_dest_file = NULL;
    gboolean dest_exists;
    int result;

    if ( should_abort( task ) )
        return ;
    if ( lstat( src_file, &file_stat ) == -1 )
    {
        /* Error occurred */
        call_state_callback( task, VFS_FILE_TASK_ERROR );
        if ( should_abort( task ) )
            return ;
    }

    task->current_file = src_file;
    task->current_dest = dest_file;
    call_progress_callback( task );

    result = 0;
    if ( S_ISDIR( file_stat.st_mode ) )
    {
        if ( ! check_overwrite( task, dest_file,
                                &dest_exists, &new_dest_file ) )
            goto _return_;

        if ( new_dest_file )
            task->current_dest = dest_file = new_dest_file;

        if ( ! dest_exists )
            result = mkdir( dest_file, file_stat.st_mode | 0700 );

        if ( result == 0 )
        {
            struct utimbuf times;
            task->progress += file_stat.st_size;
            call_progress_callback( task );

            dir = g_dir_open( src_file, 0, NULL );
            while ( (file_name = g_dir_read_name( dir )) )
            {
                if ( should_abort( task ) )
                    break;
                sub_src_file = g_build_filename( src_file, file_name, NULL );
                sub_dest_file = g_build_filename( dest_file, file_name, NULL );
                vfs_file_task_do_copy( task, sub_src_file, sub_dest_file );
                g_free( sub_dest_file );
                g_free( sub_src_file );
            }
            g_dir_close( dir );
            chmod( dest_file, file_stat.st_mode );
            times.actime = file_stat.st_atime;
            times.modtime = file_stat.st_mtime;
            utime( dest_file, &times );
            /* Move files to different device: Need to delete source files */
            if ( ( task->type == VFS_FILE_TASK_MOVE
                 || task->type == VFS_FILE_TASK_TRASH )
                 && !should_abort( task ) )
            {
                if ( (result = rmdir( src_file )) )
                {
                    task->error = errno;
                    call_state_callback( task, VFS_FILE_TASK_ERROR );
                    if ( should_abort( task ) )
                        goto _return_;
                }
            }
        }
        else
        {  /* result != 0, error occurred */
            task->error = errno;
            call_state_callback( task, VFS_FILE_TASK_ERROR );
        }
    }
    else if ( S_ISLNK( file_stat.st_mode ) )
    {
        if ( ( rfd = readlink( src_file, buffer, sizeof( buffer ) ) ) > 0 )
        {
            if ( ! check_overwrite( task, dest_file,
                                    &dest_exists, &new_dest_file ) )
                goto _return_;

            if ( new_dest_file )
                task->current_dest = dest_file = new_dest_file;

            if ( ( wfd = symlink( buffer, dest_file ) ) == 0 )
            {
                chmod( dest_file, file_stat.st_mode );
                /* Move files to different device: Need to delete source files */
                if( task->type == VFS_FILE_TASK_MOVE || task->type == VFS_FILE_TASK_TRASH )
                {
                    result = unlink( src_file );
                    if ( result )
                    {
                        task->error = errno;
                        call_state_callback( task, VFS_FILE_TASK_ERROR );
                    }
                }
                task->progress += file_stat.st_size;
                call_progress_callback( task );
            }
            else
            {
                task->error = errno;
                call_state_callback( task, VFS_FILE_TASK_ERROR );
            }
        }
        else
        {
            task->error = errno;
            call_state_callback( task, VFS_FILE_TASK_ERROR );
        }
    }
    else
    {
        if ( ( rfd = open( src_file, O_RDONLY ) ) >= 0 )
        {
            if ( ! check_overwrite( task, dest_file,
                                    &dest_exists, &new_dest_file ) )
                goto _return_;

            if ( new_dest_file )
                task->current_dest = dest_file = new_dest_file;

            if ( ( wfd = creat( dest_file,
                                file_stat.st_mode | S_IWUSR ) ) >= 0 )
            {
                struct utimbuf times;
                while ( ( rsize = read( rfd, buffer, sizeof( buffer ) ) ) > 0 )
                {
                    if ( should_abort( task ) )
                        break;

                    if ( write( wfd, buffer, rsize ) > 0 )
                        task->progress += rsize;
                    else
                    {
                        task->error = errno;
                        call_state_callback( task, VFS_FILE_TASK_ERROR );
                        close( wfd );
                    }
                    call_progress_callback( task );
                }
                close( wfd );
                chmod( dest_file, file_stat.st_mode );
                times.actime = file_stat.st_atime;
                times.modtime = file_stat.st_mtime;
                utime( dest_file, &times );

                /* Move files to different device: Need to delete source files */
                if ( (task->type == VFS_FILE_TASK_MOVE || task->type == VFS_FILE_TASK_TRASH)
                     && !should_abort( task ) )
                {
                    result = unlink( src_file );
                    if ( result )
                    {
                        task->error = errno;
                        call_state_callback( task, VFS_FILE_TASK_ERROR );
                    }
                }
            }
            else
            {
                task->error = errno;
                call_state_callback( task, VFS_FILE_TASK_ERROR );
            }
            close( rfd );
        }
        else
        {
            task->error = errno;
            call_state_callback( task, VFS_FILE_TASK_ERROR );
        }
    }
_return_:
    g_free( new_dest_file );
}

static void
vfs_file_task_copy( char* src_file, VFSFileTask* task )
{
    gchar * file_name;
    gchar* dest_file;

    call_progress_callback( task );
    file_name = g_path_get_basename( src_file );
    dest_file = g_build_filename( task->dest_dir, file_name, NULL );
    g_free( file_name );
    vfs_file_task_do_copy( task, src_file, dest_file );
    g_free( dest_file );
}

static void
vfs_file_task_do_move ( VFSFileTask* task,
                        const char* src_file,
                        const char* dest_file )
{
    gchar* new_dest_file = NULL;
    gboolean dest_exists;
    struct stat file_stat;
    int result;

    if ( should_abort( task ) )
        return ;
    /* g_debug( "move \"%s\" to \"%s\"\n", src_file, dest_file ); */
    if ( lstat( src_file, &file_stat ) == -1 )
    {
        task->error = errno;    /* Error occurred */
        call_state_callback( task, VFS_FILE_TASK_ERROR );
        if ( should_abort( task ) )
            return ;
    }

    task->current_file = src_file;
    task->current_dest = dest_file;

    if ( should_abort( task ) )
        return ;

    call_progress_callback( task );

    if ( ! check_overwrite( task, dest_file,
           &dest_exists, &new_dest_file ) )
        return ;

    if ( new_dest_file )
        task->current_dest = dest_file = new_dest_file;

    result = rename( src_file, dest_file );

    if ( result != 0 )
    {
        task->error = errno;
        call_state_callback( task, VFS_FILE_TASK_ERROR );
        if ( should_abort( task ) )
        {
            g_free( new_dest_file );
            return ;
        }
    }
    else
        chmod( dest_file, file_stat.st_mode );

    task->progress += file_stat.st_size;
    call_progress_callback( task );

    g_free( new_dest_file );
}


static void
vfs_file_task_move( char* src_file, VFSFileTask* task )
{
    struct stat src_stat;
    struct stat dest_stat;
    gchar* file_name;
    gchar* dest_file;
    GKeyFile* kf;   /* for trash info */
    int tmpfd = -1;

    if ( should_abort( task ) )
        return ;

    task->current_file = src_file;

    file_name = g_path_get_basename( src_file );
    if( task->type == VFS_FILE_TASK_TRASH )
    {
        dest_file = g_strconcat( task->dest_dir, "/",  file_name, "XXXXXX", NULL );
        tmpfd = mkstemp( dest_file );
        if( tmpfd < 0 )
        {
            goto on_error;
        }
        g_debug( dest_file );
    }
    else
        dest_file = g_build_filename( task->dest_dir, file_name, NULL );

    g_free( file_name );

    if ( lstat( src_file, &src_stat ) == 0
            && lstat( task->dest_dir, &dest_stat ) == 0 )
    {
        /* Not on the same device */
        if ( src_stat.st_dev != dest_stat.st_dev )
        {
            /* g_print("not on the same dev: %s\n", src_file); */
            vfs_file_task_do_copy( task, src_file, dest_file );
        }
        else
        {
            /* g_print("on the same dev: %s\n", src_file); */
            vfs_file_task_do_move( task, src_file, dest_file );
        }
    }
    else
    {
        task->error = errno;
        call_state_callback( task, VFS_FILE_TASK_ERROR );
    }
on_error:
    if( tmpfd >= 0 )
    {
        close( tmpfd );
        unlink( dest_file );
    }
    g_free( dest_file );
}

static void
vfs_file_task_delete( char* src_file, VFSFileTask* task )
{
    GDir * dir;
    const gchar* file_name;
    gchar* sub_src_file;
    struct stat file_stat;
    int result;

    task->current_file = src_file;

    if ( should_abort( task ) )
        return ;

    if ( lstat( src_file, &file_stat ) == -1 )
    {
        task->error = errno;
        call_state_callback( task, VFS_FILE_TASK_ERROR );
        return ; /* Error occurred */
    }

    task->current_file = src_file;
    call_progress_callback( task );

    if ( S_ISDIR( file_stat.st_mode ) )
    {
        dir = g_dir_open( src_file, 0, NULL );
        while ( (file_name = g_dir_read_name( dir )) )
        {
            if ( should_abort( task ) )
                break;
            sub_src_file = g_build_filename( src_file, file_name, NULL );
            vfs_file_task_delete( sub_src_file, task );
            g_free( sub_src_file );
        }
        g_dir_close( dir );
        if ( should_abort( task ) )
            return ;
        result = rmdir( src_file );
        if ( result != 0 )
        {
            task->error = errno;
            call_state_callback( task, VFS_FILE_TASK_ERROR );
            return ;
        }

        task->progress += file_stat.st_size;
        call_progress_callback( task );
    }
    else
    {
        result = unlink( src_file );
        if ( result != 0 )
        {
            task->error = errno;
            call_state_callback( task, VFS_FILE_TASK_ERROR );
            return ;
        }

        task->progress += file_stat.st_size;
        call_progress_callback( task );
    }
}

static void
vfs_file_task_link( char* src_file, VFSFileTask* task )
{
    struct stat src_stat;
    int result;
    gchar* dest_file;
    gchar* file_name = g_path_get_basename( src_file );

    if ( should_abort( task ) )
        return ;
    file_name = g_path_get_basename( src_file );
    dest_file = g_build_filename( task->dest_dir, file_name, NULL );
    g_free( file_name );
    call_progress_callback( task );
    if ( stat( src_file, &src_stat ) == -1 )
    {
        task->error = errno;
        call_state_callback( task, VFS_FILE_TASK_ERROR );
        if ( should_abort( task ) )
            return ;
    }

    /* FIXME: Check overwrite!! */
    result = symlink( src_file, dest_file );
    g_free( dest_file );
    if ( result )
    {
        task->error = errno;
        call_state_callback( task, VFS_FILE_TASK_ERROR );
        if ( should_abort( task ) )
            return ;
    }
    task->progress += src_stat.st_size;
    call_progress_callback( task );
}

static void
vfs_file_task_chown_chmod( char* src_file, VFSFileTask* task )
{
    struct stat src_stat;
    int i;
    GDir* dir;
    gchar* sub_src_file;
    const gchar* file_name;
    mode_t new_mode;

    int result;

    if( should_abort( task ) )
        return ;
    task->current_file = src_file;
    /* g_debug("chmod_chown: %s\n", src_file); */
    call_progress_callback( task );

    if ( lstat( src_file, &src_stat ) == 0 )
    {
        /* chown */
        if ( task->uid != -1 || task->gid != -1 )
        {
            result = chown( src_file, task->uid, task->gid );
            if ( result != 0 )
            {
                task->error = errno;
                call_state_callback( task, VFS_FILE_TASK_ERROR );
                if ( should_abort( task ) )
                    return ;
            }
        }

        /* chmod */
        if ( task->chmod_actions )
        {
            new_mode = src_stat.st_mode;
            for ( i = 0; i < N_CHMOD_ACTIONS; ++i )
            {
                if ( task->chmod_actions[ i ] == 2 )            /* Don't change */
                    continue;
                if ( task->chmod_actions[ i ] == 0 )            /* Remove this bit */
                    new_mode &= ~chmod_flags[ i ];
                else  /* Add this bit */
                    new_mode |= chmod_flags[ i ];
            }
            if ( new_mode != src_stat.st_mode )
            {
                result = chmod( src_file, new_mode );
                if ( result != 0 )
                {
                    task->error = errno;
                    call_state_callback( task, VFS_FILE_TASK_ERROR );
                    if ( should_abort( task ) )
                        return ;
                }
            }
        }

        task->progress += src_stat.st_size;
        call_progress_callback( task );

        if ( S_ISDIR( src_stat.st_mode ) && task->recursive )
        {
            if ( (dir = g_dir_open( src_file, 0, NULL )) )
            {
                while ( (file_name = g_dir_read_name( dir )) )
                {
                    if ( should_abort( task ) )
                        break;
                    sub_src_file = g_build_filename( src_file, file_name, NULL );
                    vfs_file_task_chown_chmod( sub_src_file, task );
                    g_free( sub_src_file );
                }
                g_dir_close( dir );
            }
            else
            {
                task->error = errno;
                call_state_callback( task, VFS_FILE_TASK_ERROR );
                if ( should_abort( task ) )
                    return ;
            }
        }
    }
}

static gpointer vfs_file_task_thread ( VFSFileTask* task )
{
    GList * l;
    struct stat file_stat;
    dev_t dest_dev = 0;
    GFunc funcs[] = {( GFunc ) vfs_file_task_move,
                     ( GFunc ) vfs_file_task_copy,
                     ( GFunc ) vfs_file_task_move,  /* trash */
                     ( GFunc ) vfs_file_task_delete,
                     ( GFunc ) vfs_file_task_link,
                     ( GFunc ) vfs_file_task_chown_chmod};

    if ( task->type < VFS_FILE_TASK_MOVE
            || task->type >= VFS_FILE_TASK_LAST )
        goto _exit_thread;

    task->state = VFS_FILE_TASK_RUNNING;
    task->current_file = ( char* ) task->src_paths->data;
    task->total_size = 0;

    if( task->type == VFS_FILE_TASK_TRASH )
    {
        task->dest_dir = g_build_filename( vfs_get_trash_dir(), "files", NULL );
        /* Create the trash dir if it doesn't exist */
        if( ! g_file_test( task->dest_dir, G_FILE_TEST_EXISTS ) )
            g_mkdir_with_parents( task->dest_dir, 0700 );
    }

    call_progress_callback( task );

    /* Calculate total size of all files */
    if ( task->recursive )
    {
        for ( l = task->src_paths; l; l = l->next )
        {
            if( task->dest_dir && g_str_has_prefix( task->dest_dir, (char*)l->data ) )
            {
                /* FIXME: This has serious problems */
                /* Check if source file is contained in destination dir */
                /* The src file is already in dest dir */
                if( lstat( (char*)l->data, &file_stat ) == 0
                    && S_ISDIR(file_stat.st_mode) )
                {
                    /* It's a dir */
                    if ( task->state_cb )
                    {
                        char* err;
                        char* disp_src;
                        char* disp_dest;
                        disp_src = g_filename_display_name( (char*)l->data );
                        disp_dest = g_filename_display_name( task->dest_dir );
                        err = g_strdup_printf( _("Destination directory \"%1$s\" is contained in source \"%2$s\""), disp_dest, disp_src );
                        if ( task->state_cb( task, VFS_FILE_TASK_ERROR,
                             err, task->state_cb_data ) )
                            task->state = VFS_FILE_TASK_RUNNING;
                        else
                            task->state = VFS_FILE_TASK_ABORTED;
                        g_free( disp_src );
                        g_free( disp_dest );
                        g_free( err );
                    }
                    else
                        task->state = VFS_FILE_TASK_ABORTED;
                    if ( should_abort( task ) )
                        goto _exit_thread;
                }
            }
            get_total_size_of_dir( task, ( char* ) l->data, &task->total_size );
        }
    }
    else
    {
        if ( task->type != VFS_FILE_TASK_CHMOD_CHOWN )
        {
            if ( task->dest_dir &&
                    lstat( task->dest_dir, &file_stat ) < 0 )
            {
                task->error = errno;
                call_state_callback( task, VFS_FILE_TASK_ERROR );
                if ( should_abort( task ) )
                    goto _exit_thread;
            }
            dest_dev = file_stat.st_dev;
        }

        for ( l = task->src_paths; l; l = l->next )
        {
            if ( lstat( ( char* ) l->data, &file_stat ) == -1 )
            {
                task->error = errno;
                call_state_callback( task, VFS_FILE_TASK_ERROR );
                if ( should_abort( task ) )
                    goto _exit_thread;
            }
            if ( S_ISLNK( file_stat.st_mode ) )      /* Don't do deep copy for symlinks */
                task->recursive = FALSE;
            else if ( task->type == VFS_FILE_TASK_MOVE || task->type == VFS_FILE_TASK_TRASH )
                task->recursive = ( file_stat.st_dev != dest_dev );

            if ( task->recursive )
            {
                get_total_size_of_dir( task, ( char* ) l->data,
                                       &task->total_size );
            }
            else
            {
                task->total_size += file_stat.st_size;
            }
        }
    }

    g_list_foreach( task->src_paths,
                    funcs[ task->type ],
                    task );

_exit_thread:
    if ( task->state_cb )
        call_state_callback( task, VFS_FILE_TASK_FINISH );
    else
    {
        /* FIXME: Will this cause any problem? */
        task->thread = NULL;
        vfs_file_task_free( task );
    }
    return NULL;
}

/*
* source_files sould be a newly allocated list, and it will be
* freed after file operation has been completed
*/
VFSFileTask* vfs_task_new ( VFSFileTaskType type,
                            GList* src_files,
                            const char* dest_dir )
{
    VFSFileTask * task;
    task = g_slice_new0( VFSFileTask );

    task->type = type;
    task->src_paths = src_files;
    if ( dest_dir )
        task->dest_dir = g_strdup( dest_dir );
    if ( task->type == VFS_FILE_TASK_COPY || task->type == VFS_FILE_TASK_DELETE )
        task->recursive = TRUE;

    return task;
}

/* Set some actions for chmod, this array will be copied
* and stored in VFSFileTask */
void vfs_file_task_set_chmod( VFSFileTask* task,
                              guchar* chmod_actions )
{
    task->chmod_actions = g_slice_alloc( sizeof( guchar ) * N_CHMOD_ACTIONS );
    memcpy( ( void* ) task->chmod_actions, ( void* ) chmod_actions,
            sizeof( guchar ) * N_CHMOD_ACTIONS );
}

void vfs_file_task_set_chown( VFSFileTask* task,
                              uid_t uid, gid_t gid )
{
    task->uid = uid;
    task->gid = gid;
}

void vfs_file_task_run ( VFSFileTask* task )
{
    task->thread = g_thread_create( ( GThreadFunc ) vfs_file_task_thread,
                                    task, TRUE, NULL );
}

void vfs_file_task_try_abort ( VFSFileTask* task )
{
    task->state = VFS_FILE_TASK_QUERY_ABORT;
}

void vfs_file_task_abort ( VFSFileTask* task )
{
    task->state = VFS_FILE_TASK_ABORTED;
    /* Called from another thread */
    if ( g_thread_self() != task->thread )
    {
        g_thread_join( task->thread );
        task->thread = NULL;
    }
}

void vfs_file_task_free ( VFSFileTask* task )
{
    if ( task->src_paths )
    {
        g_list_foreach( task->src_paths, ( GFunc ) g_free, NULL );
        g_list_free( task->src_paths );
    }

    g_free( task->dest_dir );

    if ( task->chmod_actions )
        g_slice_free1( sizeof( guchar ) * N_CHMOD_ACTIONS,
                       task->chmod_actions );

    g_slice_free( VFSFileTask, task );
}

/*
* void get_total_size_of_dir( const char* path, off_t* size )
* Recursively count total size of all files in the specified directory.
* If the path specified is a file, the size of the file is directly returned.
* cancel is used to cancel the operation. This function will check the value
* pointed by cancel in every iteration. If cancel is set to TRUE, the
* calculation is cancelled.
* NOTE: *size should be set to zero before calling this function.
*/
void get_total_size_of_dir( VFSFileTask* task,
                            const char* path,
                            off_t* size )
{
    GDir * dir;
    const char* name;
    char* full_path;
    struct stat file_stat;

    if ( should_abort( task ) )
        return;

    lstat( path, &file_stat );

    *size += file_stat.st_size;
    if ( S_ISLNK( file_stat.st_mode ) )             /* Don't follow symlinks */
        return ;

    dir = g_dir_open( path, 0, NULL );
    if ( dir )
    {
        while ( (name = g_dir_read_name( dir )) )
        {
            if ( should_abort( task ) )
                break;
            full_path = g_build_filename( path, name, NULL );
            lstat( full_path, &file_stat );
            if ( S_ISDIR( file_stat.st_mode ) )
            {
                get_total_size_of_dir( task, full_path, size );
            }
            else
            {
                *size += file_stat.st_size;
            }
            g_free( full_path );
        }
        g_dir_close( dir );
    }
}

void vfs_file_task_set_recursive( VFSFileTask* task, gboolean recursive )
{
    task->recursive = recursive;
}

void vfs_file_task_set_overwrite_mode( VFSFileTask* task,
                                       VFSFileTaskOverwriteMode mode )
{
    task->overwrite_mode = mode;
}

void vfs_file_task_set_progress_callback( VFSFileTask* task,
                                          VFSFileTaskProgressCallback cb,
                                          gpointer user_data )
{
    task->progress_cb = cb;
    task->progress_cb_data = user_data;
}

void vfs_file_task_set_state_callback( VFSFileTask* task,
                                       VFSFileTaskStateCallback cb,
                                       gpointer user_data )
{
    task->state_cb = cb;
    task->state_cb_data = user_data;
}

