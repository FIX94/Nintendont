/**
 * ntfsfile.c - devoptab file routines for NTFS-based devices.
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
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "ntfsinternal.h"
#include "ntfsfile.h"

#define STATE(x)    ((ntfs_file_state*)x)

void ntfsCloseFile (ntfs_file_state *file)
{
    // Sanity check
    if (!file || !file->vd)
        return;

    // Special case fix ups for compressed and/or encrypted files
    if (file->compressed)
        ntfs_attr_pclose(file->data_na);
#ifdef HAVE_SETXATTR
    if (file->encrypted)
        ntfs_efs_fixup_attribute(NULL, file->data_na);
#endif
    // Close the file data attribute (if open)
    if (file->data_na)
        ntfs_attr_close(file->data_na);

    // Sync the file (and its attributes) to disc
    if(file->write)
    {
        ntfsUpdateTimes(file->vd, file->ni, NTFS_UPDATE_ATIME | NTFS_UPDATE_CTIME);
        ntfsSync(file->vd, file->ni);
    }

    if (file->read)
        ntfsUpdateTimes(file->vd, file->ni, NTFS_UPDATE_ATIME);

    // Close the file (if open)
    if (file->ni)
        ntfsCloseEntry(file->vd, file->ni);

    // Reset the file state
    file->ni = NULL;
    file->data_na = NULL;
    file->flags = 0;
    file->read = false;
    file->write = false;
    file->append = false;
    file->pos = 0;
    file->len = 0;

    return;
}

int ntfs_open_r (struct _reent *r, void *fileStruct, const char *path, int flags, int mode)
{
    ntfs_log_trace("fileStruct %p, path %s, flags %i, mode %i\n", (void *) fileStruct, path, flags, mode);

    ntfs_file_state* file = STATE(fileStruct);

    // Get the volume descriptor for this path
    file->vd = ntfsGetVolume(path);
    if (!file->vd) {
        r->_errno = ENODEV;
        return -1;
    }

    // Lock
    ntfsLock(file->vd);

    // Determine which mode the file is opened for
    file->flags = flags;
    if ((flags & 0x03) == O_RDONLY) {
        file->read = true;
        file->write = false;
        file->append = false;
    } else if ((flags & 0x03) == O_WRONLY) {
        file->read = false;
        file->write = true;
        file->append = (flags & O_APPEND);
    } else if ((flags & 0x03) == O_RDWR) {
        file->read = true;
        file->write = true;
        file->append = (flags & O_APPEND);
    } else {
        r->_errno = EACCES;
        ntfsUnlock(file->vd);
        return -1;
    }

    // Try and find the file and (if found) ensure that it is not a directory
    file->ni = ntfsOpenEntry(file->vd, path);
    if (file->ni && (file->ni->mrec->flags & MFT_RECORD_IS_DIRECTORY)) {
        ntfsCloseEntry(file->vd, file->ni);
        ntfsUnlock(file->vd);
        r->_errno = EISDIR;
        return -1;
    }

    // Are we creating this file?
    if ((flags & O_CREAT) && !file->ni) {

        // Create the file
        file->ni = ntfsCreate(file->vd, path, S_IFREG, NULL);
        if (!file->ni) {
            ntfsUnlock(file->vd);
            return -1;
        }

    }

    // Sanity check, the file should be open by now
    if (!file->ni) {
        ntfsUnlock(file->vd);
        r->_errno = ENOENT;
        return -1;
    }

    // Open the files data attribute
    file->data_na = ntfs_attr_open(file->ni, AT_DATA, AT_UNNAMED, 0);
    if(!file->data_na) {
        ntfsCloseEntry(file->vd, file->ni);
        ntfsUnlock(file->vd);
        return -1;
    }

    // Determine if this files data is compressed and/or encrypted
    file->compressed = NAttrCompressed(file->data_na) || (file->ni->flags & FILE_ATTR_COMPRESSED);
    file->encrypted = NAttrEncrypted(file->data_na) || (file->ni->flags & FILE_ATTR_ENCRYPTED);

    // We cannot read/write encrypted files
    if (file->encrypted) {
        ntfs_attr_close(file->data_na);
        ntfsCloseEntry(file->vd, file->ni);
        ntfsUnlock(file->vd);
        r->_errno = EACCES;
        return -1;
    }

    // Make sure we aren't trying to write to a read-only file
    if ((file->ni->flags & FILE_ATTR_READONLY) && file->write) {
        ntfs_attr_close(file->data_na);
        ntfsCloseEntry(file->vd, file->ni);
        ntfsUnlock(file->vd);
        r->_errno = EROFS;
        return -1;
    }

    // Truncate the file if requested
    if ((flags & O_TRUNC) && file->write) {
        if (ntfs_attr_truncate(file->data_na, 0)) {
            ntfs_attr_close(file->data_na);
            ntfsCloseEntry(file->vd, file->ni);
            ntfsUnlock(file->vd);
            r->_errno = errno;
            return -1;
        }
    }

    // Set the files current position and length
    file->pos = 0;
    file->len = file->data_na->data_size;

    ntfs_log_trace("file->len %llu\n", file->len);

    // Update file times
    ntfsUpdateTimes(file->vd, file->ni, NTFS_UPDATE_ATIME);

    // Insert the file into the double-linked FILO list of open files
    if (file->vd->firstOpenFile) {
        file->nextOpenFile = file->vd->firstOpenFile;
        file->vd->firstOpenFile->prevOpenFile = file;
    } else {
        file->nextOpenFile = NULL;
    }
    file->prevOpenFile = NULL;
    file->vd->firstOpenFile = file;
    file->vd->openFileCount++;

    // Unlock
    ntfsUnlock(file->vd);

    return (int)fileStruct;
}

int ntfs_close_r (struct _reent *r, int fd)
{
    ntfs_log_trace("fd %p\n", (void *) fd);

    ntfs_file_state* file = STATE(fd);

    // Sanity check
    if (!file || !file->vd) {
        r->_errno = EBADF;
        return -1;
    }

    // Lock
    ntfsLock(file->vd);

    // Close the file
    ntfsCloseFile(file);

    // Remove the file from the double-linked FILO list of open files
    file->vd->openFileCount--;
    if (file->nextOpenFile)
        file->nextOpenFile->prevOpenFile = file->prevOpenFile;
    if (file->prevOpenFile)
        file->prevOpenFile->nextOpenFile = file->nextOpenFile;
    else
        file->vd->firstOpenFile = file->nextOpenFile;

    // Unlock
    ntfsUnlock(file->vd);

    return 0;
}

ssize_t ntfs_write_r (struct _reent *r, int fd, const char *ptr, size_t len)
{
    ntfs_log_trace("fd %p, ptr %p, len %u\n", (void *) fd, ptr, len);

    ntfs_file_state* file = STATE(fd);
    ssize_t written = 0;
    off_t old_pos = 0;

    // Sanity check
    if (!file || !file->vd || !file->ni || !file->data_na) {
        r->_errno = EINVAL;
        return -1;
    }

    // Short circuit cases where we don't actually have to do anything
    if (!ptr || len <= 0) {
        return 0;
    }

    // Lock
    ntfsLock(file->vd);

    // Check that we are allowed to write to this file
    if (!file->write) {
        ntfsUnlock(file->vd);
        r->_errno = EACCES;
        return -1;
    }

    // If we are in append mode, backup the current position and move to the end of the file
    if (file->append) {
        old_pos = file->pos;
        file->pos = file->len;
    }

    // Write to the files data atrribute
    while (len) {
        ssize_t ret = ntfs_attr_pwrite(file->data_na, file->pos, len, ptr);
        if (ret <= 0) {
            ntfsUnlock(file->vd);
            r->_errno = errno;
            return -1;
        }
        len -= ret;
        file->pos += ret;
        written += ret;
    }

    // If we are in append mode, restore the current position to were it was prior to this write
    if (file->append) {
        file->pos = old_pos;
    }

    // Mark the file for archiving (if we actually wrote something)
    if (written)
        file->ni->flags |= FILE_ATTR_ARCHIVE;

    // Update the files data length
    file->len = file->data_na->data_size;

    // Unlock
    ntfsUnlock(file->vd);

    return written;
}

ssize_t ntfs_read_r (struct _reent *r, int fd, char *ptr, size_t len)
{
    ntfs_log_trace("fd %p, ptr %p, len %u\n", (void *) fd, ptr, len);

    ntfs_file_state* file = STATE(fd);
    ssize_t read = 0;

    // Sanity check
    if (!file || !file->vd || !file->ni || !file->data_na) {
        r->_errno = EINVAL;
        return -1;
    }

    // Short circuit cases where we don't actually have to do anything
    if (!ptr || len <= 0) {
        return 0;
    }

    // Lock
    ntfsLock(file->vd);

    // Check that we are allowed to read from this file
    if (!file->read) {
        ntfsUnlock(file->vd);
        r->_errno = EACCES;
        return -1;
    }

    // Don't read past the end of file
    if (file->pos + len > file->len) {
        r->_errno = EOVERFLOW;
        len = file->len - file->pos;
        ntfs_log_trace("EOVERFLOW");
    }

	ntfs_log_trace("file->pos:%d, len:%d, file->len:%d \n", (u32)file->pos, (u32)len, (u32)file->len);

    // Read from the files data attribute
    while (len) {
        ssize_t ret = ntfs_attr_pread(file->data_na, file->pos, len, ptr);
        if (ret <= 0 || ret > len) {
            ntfsUnlock(file->vd);
            r->_errno = errno;
            return -1;
        }
        ptr += ret;
        len -= ret;
        file->pos += ret;
        read += ret;
    }
    //ntfs_log_trace("file->pos: %d \n", (u32)file->pos);
    // Update file times (if we actually read something)

    // Unlock
    ntfsUnlock(file->vd);

    return read;
}

off_t ntfs_seek_r (struct _reent *r, int fd, off_t pos, int dir)
{
    ntfs_log_trace("fd %p, pos %llu, dir %i\n", (void *) fd, pos, dir);

    ntfs_file_state* file = STATE(fd);
    off_t position = 0;

    // Sanity check
    if (!file || !file->vd || !file->ni || !file->data_na) {
        r->_errno = EINVAL;
        return -1;
    }

    // Lock
    ntfsLock(file->vd);

    // Set the files current position
    switch(dir) {
        case SEEK_SET: position = file->pos = MIN(MAX(pos, 0), file->len); break;
        case SEEK_CUR: position = file->pos = MIN(MAX(file->pos + pos, 0), file->len); break;
        case SEEK_END: position = file->pos = MIN(MAX(file->len + pos, 0), file->len); break;
    }

    // Unlock
    ntfsUnlock(file->vd);

    return position;
}
int ntfs_fstat_r (struct _reent *r, int fd, struct stat *st)
{
    ntfs_log_trace("fd %p\n", (void *) fd);

    ntfs_file_state* file = STATE(fd);
    int ret = 0;

    // Sanity check
    if (!file || !file->vd || !file->ni || !file->data_na) {
        r->_errno = EINVAL;
        return -1;
    }

    // Short circuit cases were we don't actually have to do anything
    if (!st)
        return 0;

    // Get the file stats
    ret = ntfsStat(file->vd, file->ni, st);
    if (ret)
        r->_errno = errno;

    return ret;
}

int ntfs_ftruncate_r (struct _reent *r, int fd, off_t len)
{
    ntfs_log_trace("fd %p, len %llu\n", (void *) fd, (u64) len);

    ntfs_file_state* file = STATE(fd);

    // Sanity check
    if (!file || !file->vd || !file->ni || !file->data_na) {
        r->_errno = EINVAL;
        return -1;
    }

    // Lock
    ntfsLock(file->vd);

    // Check that we are allowed to write to this file
    if (!file->write) {
        ntfsUnlock(file->vd);
        r->_errno = EACCES;
        return -1;
    }

    // For compressed files, only deleting and expanding contents are implemented
    if (file->compressed &&
        len > 0 &&
        len < file->data_na->initialized_size) {
        ntfsUnlock(file->vd);
        r->_errno = EOPNOTSUPP;
        return -1;
    }

    // Resize the files data attribute, either by expanding or truncating
    if (file->compressed && len > file->data_na->initialized_size) {
        char zero = 0;
        if (ntfs_attr_pwrite(file->data_na, len - 1, 1, &zero) <= 0) {
            ntfsUnlock(file->vd);
            r->_errno = errno;
            return -1;
        }
    } else {
        if (ntfs_attr_truncate(file->data_na, len)) {
            ntfsUnlock(file->vd);
            r->_errno = errno;
            return -1;
        }
    }

    // Mark the file for archiving (if we actually changed something)
    if (file->len != file->data_na->data_size)
        file->ni->flags |= FILE_ATTR_ARCHIVE;

    // Update file times (if we actually changed something)
    if (file->len != file->data_na->data_size)
        ntfsUpdateTimes(file->vd, file->ni, NTFS_UPDATE_AMCTIME);

    // Update the files data length
    file->len = file->data_na->data_size;

    // Sync the file (and its attributes) to disc
    ntfsSync(file->vd, file->ni);

    // Unlock
    ntfsUnlock(file->vd);

    return 0;
}

int ntfs_fsync_r (struct _reent *r, int fd)
{
    ntfs_log_trace("fd %p\n", (void *) fd);

    ntfs_file_state* file = STATE(fd);
    int ret = 0;

    // Sanity check
    if (!file || !file->vd || !file->ni || !file->data_na) {
        r->_errno = EINVAL;
        return -1;
    }

    // Lock
    ntfsLock(file->vd);

    // Sync the file (and its attributes) to disc
    ret = ntfsSync(file->vd, file->ni);
    if (ret)
        r->_errno = errno;

    // Unlock
    ntfsUnlock(file->vd);

    return ret;
}
