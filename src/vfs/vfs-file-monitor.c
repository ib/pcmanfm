/*
*  C Implementation: vfs-monitor
*
* Description: File alteration monitor
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
* Most of the inotify parts are taken from "menu-monitor-inotify.c" of
* gnome-menus, which is licensed under GNU Lesser General Public License.
*
* Copyright (C) 2005 Red Hat, Inc.
* Copyright (C) 2006 Mark McLoughlin
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "vfs-file-monitor.h"
#include "vfs-file-info.h"
#include <sys/types.h>  /* for stat */
#include <sys/stat.h>
#include <errno.h>

#include <stdlib.h>
#include <string.h>

#include "glib-mem.h"

typedef struct
{
    VFSFileMonitorCallback callback;
    gpointer user_data;
}
VFSFileMonitorCallbackEntry;

static GHashTable* monitor_hash = NULL;
static GIOChannel* fam_io_channel = NULL;
static guint fam_io_watch = 0;
#ifdef USE_INOTIFY
static int inotify_fd = -1;
#else
static FAMConnection fam;
#endif

/* event handler of all FAM events */
static gboolean on_fam_event( GIOChannel *channel,
                              GIOCondition cond,
                              gpointer user_data );


static gboolean connect_to_fam()
{
#ifdef USE_INOTIFY
    inotify_fd = inotify_init ();
    if ( inotify_fd < 0 )
    {
        g_warning( "failed to initialize inotify." );
        return FALSE;
    }
    fam_io_channel = g_io_channel_unix_new( inotify_fd );
#else /* use FAM|gamin */

    if ( FAMOpen( &fam ) )
    {
        fam_io_channel = NULL;
        fam.fd = -1;
        g_warning( "There is no FAM/gamin server\n" );
        return FALSE;
    }
#if HAVE_FAMNOEXISTS
    /*
    * Disable the initital directory content loading.
    * This can greatly speed up directory loading, but
    * unfortunately, it's not compatible with original FAM.
    */
    FAMNoExists( &fam );  /* This is an extension of gamin */
#endif

    fam_io_channel = g_io_channel_unix_new( FAMCONNECTION_GETFD( &fam ) );
#endif

    /* set fam socket to non-blocking */
    /* fcntl( FAMCONNECTION_GETFD( &fam ),F_SETFL,O_NONBLOCK); */

    g_io_channel_set_encoding( fam_io_channel, NULL, NULL );
    g_io_channel_set_buffered( fam_io_channel, FALSE );
    g_io_channel_set_flags( fam_io_channel, G_IO_FLAG_NONBLOCK, NULL );

    fam_io_watch = g_io_add_watch( fam_io_channel,
                                   G_IO_IN | G_IO_PRI | G_IO_HUP|G_IO_ERR,
                                   on_fam_event,
                                   NULL );
    return TRUE;
}

static void disconnect_from_fam()
{
    if ( fam_io_channel )
    {
        g_io_channel_unref( fam_io_channel );
        fam_io_channel = NULL;
        g_source_remove( fam_io_watch );
#ifdef USE_INOTIFY

        close( inotify_fd );
        inotify_fd = -1;
#else

        FAMClose( &fam );
#endif

    }
}

/* final cleanup */
void vfs_file_monitor_clean()
{
    disconnect_from_fam();
    if ( monitor_hash )
    {
        g_hash_table_destroy( monitor_hash );
        monitor_hash = NULL;
    }
}

/*
* Init monitor:
* Establish connection with gamin/fam.
*/
gboolean vfs_file_monitor_init()
{
    monitor_hash = g_hash_table_new( g_str_hash, g_str_equal );
    if ( ! connect_to_fam() )
        return FALSE;
    return TRUE;
}

VFSFileMonitor* vfs_file_monitor_add( char* path,
                                      gboolean is_dir,
                                      VFSFileMonitorCallback cb,
                                      gpointer user_data )
{
    VFSFileMonitor * monitor;
    VFSFileMonitorCallbackEntry cb_ent;
    struct stat file_stat;
    gchar* real_path = NULL;

    if ( ! monitor_hash )
        return NULL;

    monitor = ( VFSFileMonitor* ) g_hash_table_lookup ( monitor_hash, path );
    if ( ! monitor )
    {
        monitor = g_slice_new0( VFSFileMonitor );
        monitor->path = g_strdup( path );

        monitor->callbacks = g_array_new ( FALSE, FALSE, sizeof( VFSFileMonitorCallbackEntry ) );
        g_hash_table_insert ( monitor_hash,
                              monitor->path,
                              monitor );

        /* NOTE: Since gamin, FAM and inotify don't follow symlinks,
                 we need to do some special processing here. */
        if ( lstat( path, &file_stat ) == 0 )
        {
            const char* link_file = path;
            while( G_UNLIKELY( S_ISLNK(file_stat.st_mode) ) )
            {
                char* link = g_file_read_link( link_file, NULL );
                char* dirname = g_path_get_dirname( link_file );
                real_path = vfs_file_resolve_path( dirname, link );
                g_free( link );
                g_free( dirname );
                if( lstat( real_path, &file_stat ) == -1 )
                    break;
                link_file = real_path;
            }
        }

#ifdef USE_INOTIFY /* Linux inotify */
        monitor->wd = inotify_add_watch ( inotify_fd, real_path ? real_path : path,
                                            IN_MODIFY | IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MOVE | IN_MOVE_SELF | IN_UNMOUNT | IN_ATTRIB);
        if ( monitor->wd < 0 )
        {
            g_warning ( "Failed to add monitor on '%s': %s",
                        path,
                        g_strerror ( errno ) );
            return NULL;
        }
#else /* Use FAM|gamin */
        if ( is_dir )
        {
            FAMMonitorDirectory( &fam,
                                    real_path ? real_path : path,
                                    &monitor->request,
                                    monitor );
        }
        else
        {
            FAMMonitorFile( &fam,
                            real_path ? real_path : path,
                            &monitor->request,
                            monitor );
        }
#endif
        g_free( real_path );
    }

    if( G_LIKELY(monitor) )
    {
        /* g_debug( "monitor installed: %s, %p", path, monitor ); */
        if ( cb )
        { /* Install a callback */
            cb_ent.callback = cb;
            cb_ent.user_data = user_data;
            monitor->callbacks = g_array_append_val( monitor->callbacks, cb_ent );
        }
        g_atomic_int_inc( &monitor->n_ref );
    }
    return monitor;
}

void vfs_file_monitor_remove( VFSFileMonitor * fm,
                              VFSFileMonitorCallback cb,
                              gpointer user_data )
{
    int i;
    VFSFileMonitorCallbackEntry* callbacks;

    if ( cb && fm && fm->callbacks )
    {
        callbacks = ( VFSFileMonitorCallbackEntry* ) fm->callbacks->data;
        for ( i = 0; i < fm->callbacks->len; ++i )
        {
            if ( callbacks[ i ].callback == cb && callbacks[ i ].user_data == user_data )
            {
                fm->callbacks = g_array_remove_index_fast ( fm->callbacks, i );
                break;
            }
        }
    }

    if ( g_atomic_int_dec_and_test( &fm->n_ref ) )
    {
#ifdef USE_INOTIFY /* Linux inotify */
        inotify_rm_watch ( inotify_fd, fm->wd );
#else /*  Use FAM|gamin */

        FAMCancelMonitor( &fam, &fm->request );
#endif
        g_hash_table_remove( monitor_hash, fm->path );
        g_free( fm->path );
        g_array_free( fm->callbacks, TRUE );
        g_slice_free( VFSFileMonitor, fm );
    }
}

static void reconnect_fam( gpointer key,
                           gpointer value,
                           gpointer user_data )
{
    struct stat file_stat;
    VFSFileMonitor* monitor = ( VFSFileMonitor* ) value;
    const char* path = ( const char* ) key;
    if ( lstat( path, &file_stat ) != -1 )
    {
#ifdef USE_INOTIFY /* Linux inotify */
        monitor->wd = inotify_add_watch ( inotify_fd, path,
                                          IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVE );
        if ( monitor->wd < 0 )
        {
            /*
                  * FIXME: add monitor to an ancestor which does actually exist,
                  *        or do the equivalent of inotify-missing.c by maintaining
                  *        a list of monitors on non-existent files/directories
                  *        which you retry in a timeout.
            */
            g_warning ( "Failed to add monitor on '%s': %s",
                        path,
                        g_strerror ( errno ) );
            return ;
        }
#else
        if ( S_ISDIR( file_stat.st_mode ) )
        {
            FAMMonitorDirectory( &fam,
                                 path,
                                 &monitor->request,
                                 monitor );
        }
        else
        {
            FAMMonitorFile( &fam,
                            path,
                            &monitor->request,
                            monitor );
        }
#endif

    }
}

#ifdef USE_INOTIFY
static gboolean find_monitor( gpointer key,
                              gpointer value,
                              gpointer user_data )
{
    int wd = GPOINTER_TO_INT( user_data );
    VFSFileMonitor* monitor = ( VFSFileMonitor* ) value;
    return ( monitor->wd == wd );
}

static VFSFileMonitorEvent translate_inotify_event( int inotify_mask )
{
    if ( inotify_mask & ( IN_CREATE | IN_MOVED_TO ) )
        return VFS_FILE_MONITOR_CREATE;
    else if ( inotify_mask & ( IN_DELETE | IN_MOVED_FROM | IN_DELETE_SELF | IN_UNMOUNT ) )
        return VFS_FILE_MONITOR_DELETE;
    else if ( inotify_mask & ( IN_MODIFY | IN_ATTRIB ) )
        return VFS_FILE_MONITOR_CHANGE;
}
#endif

static void dispatch_event( VFSFileMonitor * monitor,
                            VFSFileMonitorEvent evt,
                            const char * file_name )
{
    VFSFileMonitorCallbackEntry * cb;
    VFSFileMonitorCallback func;
    int i;
    /* Call the callback functions */
    if ( monitor->callbacks && monitor->callbacks->len )
    {
        cb = ( VFSFileMonitorCallbackEntry* ) monitor->callbacks->data;
        for ( i = 0; i < monitor->callbacks->len; ++i )
        {
            func = cb[ i ].callback;
            func( monitor, evt, file_name, cb[ i ].user_data );
        }
    }
}

/* event handler of all FAM events */
static gboolean on_fam_event( GIOChannel * channel,
                              GIOCondition cond,
                              gpointer user_data )
{
#ifdef USE_INOTIFY /* Linux inootify */
#define BUF_LEN (1024 * (sizeof (struct inotify_event) + 16))
    char buf[ BUF_LEN ];
    int i, len;
#else /* FAM|gamin */
    FAMEvent evt;
#endif

    VFSFileMonitor* monitor = NULL;

    if ( cond & (G_IO_HUP | G_IO_ERR) )
    {
        disconnect_from_fam();
        if ( g_hash_table_size ( monitor_hash ) > 0 )
        {
            /*
              Disconnected from FAM server, but there are still monitors.
              This may be caused by crash of FAM server.
              So we have to reconnect to FAM server.
            */
            connect_to_fam();
            g_hash_table_foreach( monitor_hash, ( GHFunc ) reconnect_fam, NULL );
        }
        return TRUE; /* don't need to remove the event source since
                                    it has been removed by disconnect_from_fam(). */
    }

#ifdef USE_INOTIFY /* Linux inotify */
    while ( ( len = read ( inotify_fd, buf, BUF_LEN ) ) < 0
            && errno == EINTR );
    if ( len < 0 )
    {
        g_warning ( "Error reading inotify event: %s",
                    g_strerror ( errno ) );
        /* goto error_cancel; */
        return FALSE;
    }

    if ( len == 0 )
    {
        /*
        * FIXME: handle this better?
        */
        g_warning ( "Error reading inotify event: supplied buffer was too small" );
        /* goto error_cancel; */
        return FALSE;
    }
    i = 0;
    while ( i < len )
    {
        struct inotify_event * ievent = ( struct inotify_event * ) & buf [ i ];
        /* FIXME: 2 different paths can have the same wd because of link */
        monitor = ( VFSFileMonitor* ) g_hash_table_find(
                      monitor_hash,
                      find_monitor,
                      GINT_TO_POINTER( ievent->wd ) );
        if( G_LIKELY(monitor) )
        {
            const char* file_name;
            file_name = ievent->len > 0 ? ievent->name : monitor->path;
            g_debug("inotify (%d) :%s", ievent->mask, file_name);
            dispatch_event( monitor,
                            translate_inotify_event( ievent->mask ),
                            file_name );
        }
        i += sizeof ( struct inotify_event ) + ievent->len;
    }
#else /* FAM|gamin */
    while ( FAMPending( &fam ) )
    {
        if ( FAMNextEvent( &fam, &evt ) > 0 )
        {
            monitor = ( VFSFileMonitor* ) evt.userdata;
            switch ( evt.code )
            {
            case FAMCreated:
            case FAMDeleted:
            case FAMChanged:
                /* FIXME: There exists a possibility that a file can accidentally become
                          a directory, and a directory can become a file when using chmod.
                          Should we delete original request, and create a new one when this happens?
                */
                /* g_debug("FAM event(%d): %s", evt.code, evt.filename); */
                /* Call the callback functions */
                dispatch_event( monitor, evt.code, evt.filename );
                break;
                /* Other events are not supported */
            default:
                break;
            }
        }
    }
#endif
    return TRUE;
}

