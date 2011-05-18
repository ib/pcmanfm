/*
*  C Implementation: vfs-volume
*
* Description:
*
*
* Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
*
* Copyright: See COPYING file that comes with this distribution
*
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "vfs-volume.h"
#include "glib-mem.h"
#include <glib/gi18n.h>
#include <string.h>

struct _VFSVolume
{
    char* disp_name;
};

gboolean vfs_volume_init()
{
    return TRUE;
}

gboolean vfs_volume_finalize()
{
    return TRUE;
}

const GList* vfs_volume_get_all_volumes()
{
    return NULL;
}

void vfs_volume_add_callback( VFSVolumeCallback cb, gpointer user_data )
{}

void vfs_volume_remove_callback( VFSVolumeCallback cb, gpointer user_data )
{}

gboolean vfs_volume_mount( VFSVolume* vol, GError** err )
{
    return TRUE;
}

gboolean vfs_volume_umount( VFSVolume *vol, GError** err )
{
    return TRUE;
}

gboolean vfs_volume_eject( VFSVolume *vol, GError** err )
{
    return TRUE;
}

const char* vfs_volume_get_disp_name( VFSVolume *vol )
{
    return NULL;
}

const char* vfs_volume_get_mount_point( VFSVolume *vol )
{
    return NULL;
}

const char* vfs_volume_get_device( VFSVolume *vol )
{
    return NULL;
}

const char* vfs_volume_get_fstype( VFSVolume *vol )
{
    return NULL;
}

const char* vfs_volume_get_icon( VFSVolume *vol )
{
    return NULL;
}

gboolean vfs_volume_is_removable( VFSVolume *vol )
{
    return FALSE;
}

gboolean vfs_volume_is_mounted( VFSVolume *vol )
{
    return FALSE;
}

gboolean vfs_volume_requires_eject( VFSVolume *vol )
{
    return FALSE;
}


