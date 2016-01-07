/**
 * ntfsinternal.h - Internal support routines for NTFS-based devices.
 *
 * Copyright (c) 2010 Dimok
 * Copyright (c) 2009 Rhys "Shareese" Koedijk
 * Copyright (c) 2006 Michael "Chishm" Chisholm
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "ntfsinternal.h"
#include "ntfsdir.h"
#include "ntfsfile.h"

#if defined(__wii__)
#include <sdcard/wiisd_io.h>
#include <sdcard/gcsd.h>
#include <ogc/usbstorage.h>

extern DISC_INTERFACE __io_custom_usbstorage;
const INTERFACE_ID ntfs_disc_interfaces[] = {
    { "sd", &__io_wiisd },
    { "usb", &__io_custom_usbstorage },
    { "carda", &__io_gcsda },
    { "cardb", &__io_gcsdb },
    { NULL, NULL }
};

#elif defined(__gamecube__)
#include <sdcard/gcsd.h>

const INTERFACE_ID ntfs_disc_interfaces[] = {
    { "carda", &__io_gcsda },
    { "cardb", &__io_gcsdb },
    { NULL, NULL }
};

#endif

int ntfsAddDevice (const char *name, void *deviceData)
{
    const devoptab_t *devoptab_ntfs = ntfsGetDevOpTab();
    devoptab_t *dev = NULL;
    char *devname = NULL;
    int i;

    // Sanity check
    if (!name || !deviceData || !devoptab_ntfs) {
        errno = EINVAL;
        return -1;
    }

    // Allocate a devoptab for this device
    dev = (devoptab_t *) ntfs_alloc(sizeof(devoptab_t) + strlen(name) + 1);
    if (!dev) {
        errno = ENOMEM;
        return false;
    }

    // Use the space allocated at the end of the devoptab for storing the device name
    devname = (char*)(dev + 1);
    strcpy(devname, name);

    // Setup the devoptab
    memcpy(dev, devoptab_ntfs, sizeof(devoptab_t));
    dev->name = devname;
    dev->deviceData = deviceData;

    // Add the device to the devoptab table (if there is a free slot)
    for (i = 0; i < STD_MAX; i++) {
        if (devoptab_list[i] == devoptab_list[0] && i != 0) {
            devoptab_list[i] = dev;
            return 0;
        }
    }

    // If we reach here then there are no free slots in the devoptab table for this device
    errno = EADDRNOTAVAIL;
    return -1;
}

void ntfsRemoveDevice (const char *path)
{
    const devoptab_t *devoptab = NULL;
    char name[128] = {0};
    int i;

    // Get the device name from the path
    strncpy(name, path, 127);
    strtok(name, ":/");

    // Find and remove the specified device from the devoptab table
    // NOTE: We do this manually due to a 'bug' in RemoveDevice
    //       which ignores names with suffixes and causes names
    //       like "ntfs" and "ntfs1" to be seen as equals
    for (i = 0; i < STD_MAX; i++) {
        devoptab = devoptab_list[i];
        if (devoptab && devoptab->name) {
            if (strcmp(name, devoptab->name) == 0) {
                devoptab_list[i] = devoptab_list[0];
                ntfs_free((devoptab_t*)devoptab);
                break;
            }
        }
    }

    return;
}

const devoptab_t *ntfsGetDevice (const char *path, bool useDefaultDevice)
{
    const devoptab_t *devoptab = NULL;
    char name[128] = {0};
    int i;

    // Get the device name from the path
    strncpy(name, path, 127);
    strtok(name, ":/");

    // Search the devoptab table for the specified device name
    // NOTE: We do this manually due to a 'bug' in GetDeviceOpTab
    //       which ignores names with suffixes and causes names
    //       like "ntfs" and "ntfs1" to be seen as equals
    for (i = 0; i < STD_MAX; i++) {
        devoptab = devoptab_list[i];
        if (devoptab && devoptab->name) {
            if (strcmp(name, devoptab->name) == 0) {
                return devoptab;
            }
        }
    }

    // If we reach here then we couldn't find the device name,
    // chances are that this path has no device name in it.
    // Call GetDeviceOpTab to get our default device (chdir).
    if (useDefaultDevice)
        return GetDeviceOpTab("");

    return NULL;
}

const INTERFACE_ID *ntfsGetDiscInterfaces (void)
{
    // Get all know disc interfaces on the host system
    return ntfs_disc_interfaces;
}

ntfs_vd *ntfsGetVolume (const char *path)
{
    // Get the volume descriptor from the paths associated devoptab (if found)
    const devoptab_t *devoptab_ntfs = ntfsGetDevOpTab();
    const devoptab_t *devoptab = ntfsGetDevice(path, true);
    if (devoptab && devoptab_ntfs && (devoptab->open_r == devoptab_ntfs->open_r))
        return (ntfs_vd*)devoptab->deviceData;

    return NULL;
}

int ntfsInitVolume (ntfs_vd *vd)
{
    // Sanity check
    if (!vd) {
        errno = ENODEV;
        return -1;
    }

    // Initialise the volume lock
    LWP_MutexInit(&vd->lock, false);

    // Reset the volumes name cache
    vd->name[0] = '\0';

    // Reset the volumes current directory
    vd->cwd_ni = NULL;

    // Reset open directory and file stats
    vd->openDirCount = 0;
    vd->openFileCount = 0;
    vd->firstOpenDir = NULL;
    vd->firstOpenFile = NULL;

    return 0;
}

void ntfsDeinitVolume (ntfs_vd *vd)
{
    // Sanity check
    if (!vd) {
        errno = ENODEV;
        return;
    }

    // Lock
    ntfsLock(vd);

    // Close any directories which are still open (lazy programmers!)
    ntfs_dir_state *nextDir = vd->firstOpenDir;
    while (nextDir) {
        ntfs_log_warning("Cleaning up orphaned directory @ %p\n", nextDir);
        ntfsCloseDir(nextDir);
        nextDir = nextDir->nextOpenDir;
    }

    // Close any files which are still open (lazy programmers!)
    ntfs_file_state *nextFile = vd->firstOpenFile;
    while (nextFile) {
        ntfs_log_warning("Cleaning up orphaned file @ %p\n", nextFile);
        ntfsCloseFile(nextFile);
        nextFile = nextFile->nextOpenFile;
    }

    // Reset open directory and file stats
    vd->openDirCount = 0;
    vd->openFileCount = 0;
    vd->firstOpenDir = NULL;
    vd->firstOpenFile = NULL;

    // Close the volumes current directory (if any)
    //if (vd->cwd_ni) {
        //ntfsCloseEntry(vd, vd->cwd_ni);
        //vd->cwd_ni = NULL;
    //}

    // Force the underlying device to sync
    ntfs_device_sync(vd->dev);

    // Unlock
    ntfsUnlock(vd);

    // Deinitialise the volume lock
    LWP_MutexDestroy(vd->lock);

    return;
}

ntfs_inode *ntfsOpenEntry (ntfs_vd *vd, const char *path)
{
    return ntfsParseEntry(vd, path, 1);
}

ntfs_inode *ntfsParseEntry (ntfs_vd *vd, const char *path, int reparseLevel)
{
    ntfs_inode *ni = NULL;
    char *target = NULL;
    int attr_size;

    // Sanity check
    if (!vd) {
        errno = ENODEV;
        return NULL;
    }

    // Get the actual path of the entry
    path = ntfsRealPath(path);
    if (!path) {
        errno = EINVAL;
        return NULL;
    } else if (path[0] == '\0') {
        path = ".";
    }

    // Find the entry, taking into account our current directory (if any)
    if (path[0] != PATH_SEP)
        ni = ntfs_pathname_to_inode(vd->vol, vd->cwd_ni, path++);
    else
        ni = ntfs_pathname_to_inode(vd->vol, NULL, path);

    // If the entry was found and it has reparse data then parse its true path;
    // this resolves the true location of symbolic links and directory junctions
    if (ni && (ni->flags & FILE_ATTR_REPARSE_POINT)) {
        if (ntfs_possible_symlink(ni)) {

            // Sanity check, give up if we are parsing to deep
            if (reparseLevel > NTFS_MAX_SYMLINK_DEPTH) {
                ntfsCloseEntry(vd, ni);
                errno = ELOOP;
                return NULL;
            }

            // Get the target path of this entry
            target = ntfs_make_symlink(ni, path, &attr_size);
            if (!target) {
                ntfsCloseEntry(vd, ni);
                return NULL;
            }

            // Close the entry (we are no longer interested in it)
            ntfsCloseEntry(vd, ni);

            // Parse the entries target
            ni = ntfsParseEntry(vd, target, reparseLevel++);

            // Clean up
            // use free because the value was not allocated with ntfs_alloc
            free(target);

        }
    }

    return ni;
}

void ntfsCloseEntry (ntfs_vd *vd, ntfs_inode *ni)
{
    // Sanity check
    if (!vd) {
        errno = ENODEV;
        return;
    }

    // Lock
    ntfsLock(vd);

    // Sync the entry (if it is dirty)
    if (NInoDirty(ni))
        ntfsSync(vd, ni);

    // Close the entry
    ntfs_inode_close(ni);

    // Unlock
    ntfsUnlock(vd);

    return;
}


ntfs_inode *ntfsCreate (ntfs_vd *vd, const char *path, mode_t type, const char *target)
{
    ntfs_inode *dir_ni = NULL, *ni = NULL;
    char *dir = NULL;
    char *name = NULL;
    ntfschar *uname = NULL, *utarget = NULL;
    int uname_len, utarget_len;

    // Sanity check
    if (!vd) {
        errno = ENODEV;
        return NULL;
    }

    // You cannot link between devices
    if(target) {
        if(vd != ntfsGetVolume(target)) {
            errno = EXDEV;
            return NULL;
        }
    }

    // Get the actual paths of the entry
    path = ntfsRealPath(path);
    target = ntfsRealPath(target);
    if (!path) {
        errno = EINVAL;
        return NULL;
    }

    // Lock
    ntfsLock(vd);

    // Get the unicode name for the entry and find its parent directory
    // TODO: This looks horrible, clean it up
    dir = strdup(path);
    if (!dir) {
        errno = EINVAL;
        goto cleanup;
    }
    name = strrchr(dir, '/');
    if (name)
        name++;
    else
        name = dir;
    uname_len = ntfsLocalToUnicode(name, &uname);
    if (uname_len < 0) {
        errno = EINVAL;
        goto cleanup;
    }
    name = strrchr(dir, '/');
    if(name)
    {
        name++;
        name[0] = 0;
    }

    // Open the entries parent directory
    dir_ni = ntfsOpenEntry(vd, dir);
    if (!dir_ni) {
        goto cleanup;
    }

    // Create the entry
    switch (type) {

        // Symbolic link
        case S_IFLNK:
            if (!target) {
                errno = EINVAL;
                goto cleanup;
            }
            utarget_len = ntfsLocalToUnicode(target, &utarget);
            if (utarget_len < 0) {
                errno = EINVAL;
                goto cleanup;
            }
            ni = ntfs_create_symlink(dir_ni, 0, uname, uname_len,  utarget, utarget_len);
            break;

        // Directory or file
        case S_IFDIR:
        case S_IFREG:
            ni = ntfs_create(dir_ni, 0, uname, uname_len, type);
            break;

    }

    // If the entry was created
    if (ni) {

        // Mark the entry for archiving
        ni->flags |= FILE_ATTR_ARCHIVE;

        // Mark the entry as dirty
        NInoSetDirty(ni);

        // Sync the entry to disc
        ntfsSync(vd, ni);

        // Update parent directories times
        ntfsUpdateTimes(vd, dir_ni, NTFS_UPDATE_MCTIME);

    }

cleanup:

    if(dir_ni)
        ntfsCloseEntry(vd, dir_ni);

    // use free because the value was not allocated with ntfs_alloc
    if(utarget)
        free(utarget);

    if(uname)
        free(uname);

    if(dir)
        ntfs_free(dir);

    // Unlock
    ntfsUnlock(vd);

    return ni;
}

int ntfsLink (ntfs_vd *vd, const char *old_path, const char *new_path)
{
    ntfs_inode *dir_ni = NULL, *ni = NULL;
    char *dir = NULL;
    char *name = NULL;
    ntfschar *uname = NULL;
    int uname_len;
    int res = 0;

    // Sanity check
    if (!vd) {
        errno = ENODEV;
        return -1;
    }

    // You cannot link between devices
    if(vd != ntfsGetVolume(new_path)) {
        errno = EXDEV;
        return -1;
    }

    // Get the actual paths of the entry
    old_path = ntfsRealPath(old_path);
    new_path = ntfsRealPath(new_path);
    if (!old_path || !new_path) {
        errno = EINVAL;
        return -1;
    }

    // Lock
    ntfsLock(vd);

    // Get the unicode name for the entry and find its parent directory
    // TODO: This looks horrible, clean it up
    dir = strdup(new_path);
    if (!dir) {
        errno = EINVAL;
        goto cleanup;
    }
    name = strrchr(dir, '/');
    if (name)
        name++;
    else
        name = dir;
    uname_len = ntfsLocalToUnicode(name, &uname);
    if (uname_len < 0) {
        errno = EINVAL;
        goto cleanup;
    }
    *name = 0;

    // Find the entry
    ni = ntfsOpenEntry(vd, old_path);
    if (!ni) {
        errno = ENOENT;
        res = -1;
        goto cleanup;
    }

    // Open the entries new parent directory
    dir_ni = ntfsOpenEntry(vd, dir);
    if (!dir_ni) {
        errno = ENOENT;
        res = -1;
        goto cleanup;
    }

    // Link the entry to its new parent
    if (ntfs_link(ni, dir_ni, uname, uname_len)) {
        res = -1;
        goto cleanup;
    }

    // Update entry times
    ntfsUpdateTimes(vd, dir_ni, NTFS_UPDATE_MCTIME);

    // Sync the entry to disc
    ntfsSync(vd, ni);

cleanup:

    if(dir_ni)
        ntfsCloseEntry(vd, dir_ni);

    if(ni)
        ntfsCloseEntry(vd, ni);

    // use free because the value was not allocated with ntfs_alloc
    if(uname)
        free(uname);

    if(dir)
        ntfs_free(dir);

    // Unlock
    ntfsUnlock(vd);

    return res;
}

int ntfsUnlink (ntfs_vd *vd, const char *path)
{
    ntfs_inode *dir_ni = NULL, *ni = NULL;
    char *dir = NULL;
    char *name = NULL;
    ntfschar *uname = NULL;
    int uname_len;
    int res = 0;

    // Sanity check
    if (!vd) {
        errno = ENODEV;
        return -1;
    }

    // Get the actual path of the entry
    path = ntfsRealPath(path);
    if (!path) {
        errno = EINVAL;
        return -1;
    }

    // Lock
    ntfsLock(vd);

    // Get the unicode name for the entry and find its parent directory
    // TODO: This looks horrible
    dir = strdup(path);
    if (!dir) {
        errno = EINVAL;
        goto cleanup;
    }
    name = strrchr(dir, '/');
    if (name)
        name++;
    else
        name = dir;
    uname_len = ntfsLocalToUnicode(name, &uname);
    if (uname_len < 0) {
        errno = EINVAL;
        goto cleanup;
    }
    name = strrchr(dir, '/');
    if(name)
    {
        name++;
        name[0] = 0;
    }

    // Find the entry
    ni = ntfsOpenEntry(vd, path);
    if (!ni) {
        errno = ENOENT;
        res = -1;
        goto cleanup;
    }

    // Open the entries parent directory
    dir_ni = ntfsOpenEntry(vd, dir);
    if (!dir_ni) {
        errno = ENOENT;
        res = -1;
        goto cleanup;
    }

    // Unlink the entry from its parent
    if (ntfs_delete(vd->vol, path, ni, dir_ni, uname, uname_len)) {
        res = -1;
    }

    // Force the underlying device to sync
    ntfs_device_sync(vd->dev);

    // ntfs_delete() ALWAYS closes ni and dir_ni; so no need for us to anymore
    dir_ni = ni = NULL;

cleanup:

    if(dir_ni)
        ntfsCloseEntry(vd, dir_ni);

    if(ni)
        ntfsCloseEntry(vd, ni);

    // use free because the value was not allocated with ntfs_alloc
    if(uname)
        free(uname);

    if(dir)
        ntfs_free(dir);

    // Unlock
    ntfsUnlock(vd);

    return res;
}

int ntfsSync (ntfs_vd *vd, ntfs_inode *ni)
{
    int res = 0;

    // Sanity check
    if (!vd) {
        errno = ENODEV;
        return -1;
    }

    // Sanity check
    if (!ni) {
        errno = ENOENT;
        return -1;
    }

    // Lock
    ntfsLock(vd);

    // Sync the entry
    res = ntfs_inode_sync(ni);

    // Force the underlying device to sync
    ntfs_device_sync(vd->dev);

    // Unlock
    ntfsUnlock(vd);

    return res;

}

int ntfsStat (ntfs_vd *vd, ntfs_inode *ni, struct stat *st)
{
    ntfs_attr *na = NULL;
    int res = 0;

    // Sanity check
    if (!vd) {
        errno = ENODEV;
        return -1;
    }

    // Sanity check
    if (!ni) {
        errno = ENOENT;
        return -1;
    }

    // Short circuit cases were we don't actually have to do anything
    if (!st)
        return 0;

    // Lock
    ntfsLock(vd);

    // Zero out the stat buffer
    memset(st, 0, sizeof(struct stat));

    // Is this entry a directory
    if (ni->mrec->flags & MFT_RECORD_IS_DIRECTORY) {
        st->st_mode = S_IFDIR | (0777 & ~vd->dmask);
        st->st_nlink = 1;

        // Open the directories index allocation table attribute
        na = ntfs_attr_open(ni, AT_INDEX_ALLOCATION, NTFS_INDEX_I30, 4);
        if (na) {
            st->st_size = na->data_size;
            st->st_blocks = na->allocated_size >> 9;
            ntfs_attr_close(na);
        }

    // Else it must be a file
    } else {
        st->st_mode = S_IFREG | (0777 & ~vd->fmask);
        st->st_size = ni->data_size;
        st->st_blocks = (ni->allocated_size + 511) >> 9;
        st->st_nlink = le16_to_cpu(ni->mrec->link_count);
    }

    // Fill in the generic entry stats
    st->st_dev = vd->id;
    st->st_uid = vd->uid;
    st->st_gid = vd->gid;
    st->st_ino = ni->mft_no;
    st->st_atime = ni->last_access_time;
    st->st_ctime = ni->last_mft_change_time;
    st->st_mtime = ni->last_data_change_time;

    // Update entry times
    ntfsUpdateTimes(vd, ni, NTFS_UPDATE_ATIME);

    // Unlock
    ntfsUnlock(vd);

    return res;
}

void ntfsUpdateTimes (ntfs_vd *vd, ntfs_inode *ni, ntfs_time_update_flags mask)
{
    // Run the access time update strategy against the device driver settings first
    if (vd && vd->atime == ATIME_DISABLED)
        mask &= ~NTFS_UPDATE_ATIME;

    // Update entry times
    if (ni && mask)
        ntfs_inode_update_times(ni, mask);

    return;
}

const char *ntfsRealPath (const char *path)
{
    // Sanity check
    if (!path)
        return NULL;

    // Move the path pointer to the start of the actual path
    if (strchr(path, ':') != NULL) {
        path = strchr(path, ':') + 1;
    }
    if (strchr(path, ':') != NULL) {
        return NULL;
    }

    return path;
}

int ntfsUnicodeToLocal (const ntfschar *ins, const int ins_len, char **outs, int outs_len)
{
    int len = 0;
    int i;

    // Sanity check
    if (!ins || !ins_len || !outs)
        return 0;

    char * ucstombs_out = NULL;

    // Convert the unicode string to our current local
    len = ntfs_ucstombs(ins, ins_len, &ucstombs_out, outs_len);

    if(ucstombs_out)
    {
        //use proper allocation
        *outs = (char *) ntfs_alloc(strlen(ucstombs_out) + 1);
        if(!*outs)
        {
            errno = ENOMEM;
            return -1;
        }
        strcpy(*outs, ucstombs_out);
        free(ucstombs_out);
        ucstombs_out = NULL;
    }

    if (len == -1 && errno == EILSEQ)
    {
        // The string could not be converted to the current local,
        // do it manually by replacing non-ASCII characters with underscores
        if (!*outs || outs_len >= ins_len)
        {
            if (!*outs)
            {
                *outs = (char *) ntfs_alloc(ins_len + 1);
                if (!*outs) {
                    errno = ENOMEM;
                    return -1;
                }
            }
            for (i = 0; i < ins_len; i++) {
                ntfschar uc = le16_to_cpu(ins[i]);
                if (uc > 0xff)
                    uc = (ntfschar)'_';
                *outs[i] = (char)uc;
            }
            *outs[ins_len] = (ntfschar)'\0';
            len = ins_len;
        }
    }

    return len;
}

int ntfsLocalToUnicode (const char *ins, ntfschar **outs)
{
    // Sanity check
    if (!ins || !outs)
        return 0;

    // Convert the local string to unicode
    return ntfs_mbstoucs(ins, outs);
}
