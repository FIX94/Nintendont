/**
 * gekko_io.c - Gekko style disk io functions.
 *
 * Copyright (c) 2009 Rhys "Shareese" Koedijk
 * Copyright (c) 2010 Dimok
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
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include "ntfs.h"
#include "types.h"
#include "logging.h"
#include "device_io.h"
#include "gekko_io.h"
#include "cache.h"
#include "device.h"
#include "bootsect.h"

#define DEV_FD(dev) ((gekko_fd *)dev->d_private)

/* Prototypes */
static s64 ntfs_device_gekko_io_readbytes(struct ntfs_device *dev, s64 offset, s64 count, void *buf);
static bool ntfs_device_gekko_io_readsectors(struct ntfs_device *dev, sec_t sector, sec_t numSectors, void* buffer);
static s64 ntfs_device_gekko_io_writebytes(struct ntfs_device *dev, s64 offset, s64 count, const void *buf);
static bool ntfs_device_gekko_io_writesectors(struct ntfs_device *dev, sec_t sector, sec_t numSectors, const void* buffer);

/**
 *
 */
static int ntfs_device_gekko_io_open(struct ntfs_device *dev, int flags)
{
    ntfs_log_trace("dev %p, flags %i\n", dev, flags);

    // Get the device driver descriptor
    gekko_fd *fd = DEV_FD(dev);
    if (!fd) {
        errno = EBADF;
        return -1;
    }

    // Get the device interface
    const DISC_INTERFACE* interface = fd->interface;
    if (!interface) {
        errno = ENODEV;
        return -1;
    }

    // Start the device interface and ensure that it is inserted
    if (!interface->startup()) {
        ntfs_log_perror("device failed to start\n");
        errno = EIO;
        return -1;
    }
    if (!interface->isInserted()) {
        ntfs_log_perror("device media is not inserted\n");
        errno = EIO;
        return -1;
    }

    // Check that the device isn't already open (used by another volume?)
    if (NDevOpen(dev)) {
        ntfs_log_perror("device is busy (already open)\n");
        errno = EBUSY;
        return -1;
    }

    // Check that there is a valid NTFS boot sector at the start of the device
    NTFS_BOOT_SECTOR *boot = (NTFS_BOOT_SECTOR *) ntfs_alloc(MAX_SECTOR_SIZE);
    if(boot == NULL) {
        errno = ENOMEM;
        return -1;
    }

    if (!interface->readSectors(fd->startSector, 1, boot)) {
        ntfs_log_perror("read failure @ sector %d\n", fd->startSector);
        errno = EIO;
        ntfs_free(boot);
        return -1;
    }

    if (!ntfs_boot_sector_is_ntfs(boot)) {
        errno = EINVALPART;
        ntfs_free(boot);
        return -1;
    }

    // Parse the boot sector
    fd->hiddenSectors = le32_to_cpu(boot->bpb.hidden_sectors);
    fd->sectorSize = le16_to_cpu(boot->bpb.bytes_per_sector);
    fd->sectorCount = sle64_to_cpu(boot->number_of_sectors);
    fd->pos = 0;
    fd->len = (fd->sectorCount * fd->sectorSize);
    fd->ino = le64_to_cpu(boot->volume_serial_number);

    // Free memory for boot sector
    ntfs_free(boot);

    // Mark the device as read-only (if required)
    if (flags & O_RDONLY) {
        NDevSetReadOnly(dev);
    }

    // Create the cache
    fd->cache = _NTFS_cache_constructor(fd->cachePageCount, fd->cachePageSize, interface, fd->startSector + fd->sectorCount, fd->sectorSize);

    // Mark the device as open
    NDevSetBlock(dev);
    NDevSetOpen(dev);

    return 0;
}

/**
 *
 */
static int ntfs_device_gekko_io_close(struct ntfs_device *dev)
{
    ntfs_log_trace("dev %p\n", dev);

    // Get the device driver descriptor
    gekko_fd *fd = DEV_FD(dev);
    if (!fd) {
        errno = EBADF;
        return -1;
    }

    // Check that the device is actually open
    if (!NDevOpen(dev)) {
        ntfs_log_perror("device is not open\n");
        errno = EIO;
        return -1;
    }

    // Mark the device as closed
    NDevClearOpen(dev);
    NDevClearBlock(dev);

    // Flush the device (if dirty and not read-only)
    if (NDevDirty(dev) && !NDevReadOnly(dev)) {
        ntfs_log_debug("device is dirty, will now sync\n");

        // ...?

        // Mark the device as clean
        NDevClearDirty(dev);

    }

    // Flush and destroy the cache (if required)
    if (fd->cache) {
        _NTFS_cache_flush(fd->cache);
        _NTFS_cache_destructor(fd->cache);
    }

    // Shutdown the device interface
    /*const DISC_INTERFACE* interface = fd->interface;
    if (interface) {
        interface->shutdown();
    }*/

    // Free the device driver private data
    ntfs_free(dev->d_private);
    dev->d_private = NULL;

    return 0;
}

/**
 *
 */
static s64 ntfs_device_gekko_io_seek(struct ntfs_device *dev, s64 offset, int whence)
{
    ntfs_log_trace("dev %p, offset %Li, whence %i\n", dev, offset, whence);

    // Get the device driver descriptor
    gekko_fd *fd = DEV_FD(dev);
    if (!fd) {
        errno = EBADF;
        return -1;
    }

    // Set the current position on the device (in bytes)
    switch(whence) {
        case SEEK_SET: fd->pos = MIN(MAX(offset, 0), fd->len); break;
        case SEEK_CUR: fd->pos = MIN(MAX(fd->pos + offset, 0), fd->len); break;
        case SEEK_END: fd->pos = MIN(MAX(fd->len + offset, 0), fd->len); break;
    }

    return 0;
}

/**
 *
 */
static s64 ntfs_device_gekko_io_read(struct ntfs_device *dev, void *buf, s64 count)
{
    return ntfs_device_gekko_io_readbytes(dev, DEV_FD(dev)->pos, count, buf);
}

/**
 *
 */
static s64 ntfs_device_gekko_io_write(struct ntfs_device *dev, const void *buf, s64 count)
{
    return ntfs_device_gekko_io_writebytes(dev, DEV_FD(dev)->pos, count, buf);
}

/**
 *
 */
static s64 ntfs_device_gekko_io_pread(struct ntfs_device *dev, void *buf, s64 count, s64 offset)
{
    return ntfs_device_gekko_io_readbytes(dev, offset, count, buf);
}

/**
 *
 */
static s64 ntfs_device_gekko_io_pwrite(struct ntfs_device *dev, const void *buf, s64 count, s64 offset)
{
    return ntfs_device_gekko_io_writebytes(dev, offset, count, buf);
}

/**
 *
 */
static s64 ntfs_device_gekko_io_readbytes(struct ntfs_device *dev, s64 offset, s64 count, void *buf)
{
    ntfs_log_trace("dev %p, offset %Li, count %Li\n", dev, offset, count);

    // Get the device driver descriptor
    gekko_fd *fd = DEV_FD(dev);
    if (!fd) {
        errno = EBADF;
        return -1;
    }

    // Get the device interface
    const DISC_INTERFACE* interface = fd->interface;
    if (!interface) {
        errno = ENODEV;
        return -1;
    }

    if(offset < 0)
    {
        errno = EROFS;
        return -1;
    }

    if(!count)
        return 0;

    sec_t sec_start = (sec_t) fd->startSector;
    sec_t sec_count = 1;
    u32 buffer_offset = (u32) (offset % fd->sectorSize);
    u8 *buffer = NULL;

    // Determine the range of sectors required for this read
    if (offset > 0) {
        sec_start += (sec_t) floor((f64) offset / (f64) fd->sectorSize);
    }
    if (buffer_offset+count > fd->sectorSize) {
        sec_count = (sec_t) ceil((f64) (buffer_offset+count) / (f64) fd->sectorSize);
    }

    // If this read happens to be on the sector boundaries then do the read straight into the destination buffer

    if((buffer_offset == 0) && (count % fd->sectorSize == 0)) {

        // Read from the device
        ntfs_log_trace("direct read from sector %d (%d sector(s) long)\n", sec_start, sec_count);
        if (!ntfs_device_gekko_io_readsectors(dev, sec_start, sec_count, buf)) {
            ntfs_log_perror("direct read failure @ sector %d (%d sector(s) long)\n", sec_start, sec_count);
            errno = EIO;
            return -1;
        }

    // Else read into a buffer and copy over only what was requested
    }
    else
	{

        // Allocate a buffer to hold the read data
        buffer = (u8*)ntfs_alloc(sec_count * fd->sectorSize);
        if (!buffer) {
            errno = ENOMEM;
            return -1;
        }

        // Read from the device
        ntfs_log_trace("buffered read from sector %d (%d sector(s) long)\n", sec_start, sec_count);
        ntfs_log_trace("count: %d  sec_count:%d  fd->sectorSize: %d )\n", (u32)count, (u32)sec_count,(u32)fd->sectorSize);
        if (!ntfs_device_gekko_io_readsectors(dev, sec_start, sec_count, buffer)) {
            ntfs_log_perror("buffered read failure @ sector %d (%d sector(s) long)\n", sec_start, sec_count);
            ntfs_free(buffer);
            errno = EIO;
            return -1;
        }

        // Copy what was requested to the destination buffer
        memcpy(buf, buffer + buffer_offset, count);
        ntfs_free(buffer);

    }

    return count;
}

/**
 *
 */
static s64 ntfs_device_gekko_io_writebytes(struct ntfs_device *dev, s64 offset, s64 count, const void *buf)
{
    ntfs_log_trace("dev %p, offset %lli, count %lli\n", dev, offset, count);

    // Get the device driver descriptor
    gekko_fd *fd = DEV_FD(dev);
    if (!fd) {
        errno = EBADF;
        return -1;
    }

    // Get the device interface
    const DISC_INTERFACE* interface = fd->interface;
    if (!interface) {
        errno = ENODEV;
        return -1;
    }

    // Check that the device can be written to
    if (NDevReadOnly(dev)) {
        errno = EROFS;
        return -1;
    }

    if(count < 0 || offset < 0) {
        errno = EROFS;
        return -1;
    }

    if(count == 0)
        return 0;

    sec_t sec_start = (sec_t) fd->startSector;
    sec_t sec_count = 1;
    u32 buffer_offset = (u32) (offset % fd->sectorSize);
    u8 *buffer = NULL;

    // Determine the range of sectors required for this write
    if (offset > 0) {
        sec_start += (sec_t) floor((f64) offset / (f64) fd->sectorSize);
    }
    if ((buffer_offset+count) > fd->sectorSize) {
        sec_count = (sec_t) ceil((f64) (buffer_offset+count) / (f64) fd->sectorSize);
    }

    // If this write happens to be on the sector boundaries then do the write straight to disc
    if((buffer_offset == 0) && (count % fd->sectorSize == 0))
    {
        // Write to the device
        ntfs_log_trace("direct write to sector %d (%d sector(s) long)\n", sec_start, sec_count);
        if (!ntfs_device_gekko_io_writesectors(dev, sec_start, sec_count, buf)) {
            ntfs_log_perror("direct write failure @ sector %d (%d sector(s) long)\n", sec_start, sec_count);
            errno = EIO;
            return -1;
        }
    // Else write from a buffer aligned to the sector boundaries
    }
    else
    {
        // Allocate a buffer to hold the write data
        buffer = (u8 *) ntfs_alloc(sec_count * fd->sectorSize);
        if (!buffer) {
            errno = ENOMEM;
            return -1;
        }
        // Read the first and last sectors of the buffer from disc (if required)
        // NOTE: This is done because the data does not line up with the sector boundaries,
        //       we just read in the buffer edges where the data overlaps with the rest of the disc
        if(buffer_offset != 0)
        {
            if (!ntfs_device_gekko_io_readsectors(dev, sec_start, 1, buffer)) {
                ntfs_log_perror("read failure @ sector %d\n", sec_start);
                ntfs_free(buffer);
                errno = EIO;
                return -1;
            }
        }
        if((buffer_offset+count) % fd->sectorSize != 0)
        {
            if (!ntfs_device_gekko_io_readsectors(dev, sec_start + sec_count - 1, 1, buffer + ((sec_count-1) * fd->sectorSize))) {
                ntfs_log_perror("read failure @ sector %d\n", sec_start + sec_count - 1);
                ntfs_free(buffer);
                errno = EIO;
                return -1;
            }
        }

        // Copy the data into the write buffer
        memcpy(buffer + buffer_offset, buf, count);

        // Write to the device
        ntfs_log_trace("buffered write to sector %d (%d sector(s) long)\n", sec_start, sec_count);
        if (!ntfs_device_gekko_io_writesectors(dev, sec_start, sec_count, buffer)) {
            ntfs_log_perror("buffered write failure @ sector %d\n", sec_start);
            ntfs_free(buffer);
            errno = EIO;
            return -1;
        }

        // Free the buffer
        ntfs_free(buffer);
    }

    // Mark the device as dirty (if we actually wrote anything)
    NDevSetDirty(dev);

    return count;
}

static bool ntfs_device_gekko_io_readsectors(struct ntfs_device *dev, sec_t sector, sec_t numSectors, void* buffer)
{
    // Get the device driver descriptor
    gekko_fd *fd = DEV_FD(dev);
    if (!fd) {
        errno = EBADF;
        return false;
    }
    // Read the sectors from disc (or cache, if enabled)
    if (fd->cache)
        return _NTFS_cache_readSectors(fd->cache, sector, numSectors, buffer);
    else
        return fd->interface->readSectors(sector, numSectors, buffer);

    return false;
}

static bool ntfs_device_gekko_io_writesectors(struct ntfs_device *dev, sec_t sector, sec_t numSectors, const void* buffer)
{
    // Get the device driver descriptor
    gekko_fd *fd = DEV_FD(dev);
    if (!fd) {
        errno = EBADF;
        return false;
    }

    // Write the sectors to disc (or cache, if enabled)
    if (fd->cache)
        return _NTFS_cache_writeSectors(fd->cache, sector, numSectors, buffer);
    else
        return fd->interface->writeSectors(sector, numSectors, buffer);

    return false;
}

/**
 *
 */
static int ntfs_device_gekko_io_sync(struct ntfs_device *dev)
{
	gekko_fd *fd = DEV_FD(dev);
    ntfs_log_trace("dev %p\n", dev);

    // Check that the device can be written to
    if (NDevReadOnly(dev)) {
        errno = EROFS;
        return -1;
    }

    // Mark the device as clean
    NDevClearDirty(dev);
    NDevClearSync(dev);

    // Flush any sectors in the disc cache (if required)
    if (fd->cache) {
        if (!_NTFS_cache_flush(fd->cache)) {
            errno = EIO;
            return -1;
        }
    }

    return 0;
}

/**
 *
 */
static int ntfs_device_gekko_io_stat(struct ntfs_device *dev, struct stat *buf)
{
    ntfs_log_trace("dev %p, buf %p\n", dev, buf);

    // Get the device driver descriptor
    gekko_fd *fd = DEV_FD(dev);
    if (!fd) {
        errno = EBADF;
        return -1;
    }

    // Short circuit cases were we don't actually have to do anything
    if (!buf)
        return 0;

    // Build the device mode
    mode_t mode = (S_IFBLK) |
                  (S_IRUSR | S_IRGRP | S_IROTH) |
                  ((!NDevReadOnly(dev)) ? (S_IWUSR | S_IWGRP | S_IWOTH) : 0);

    // Zero out the stat buffer
    memset(buf, 0, sizeof(struct stat));

    // Build the device stats
    buf->st_dev = fd->interface->ioType;
    buf->st_ino = fd->ino;
    buf->st_mode = mode;
    buf->st_rdev = fd->interface->ioType;
    buf->st_blksize = fd->sectorSize;
    buf->st_blocks = fd->sectorCount;

    return 0;
}

/**
 *
 */
static int ntfs_device_gekko_io_ioctl(struct ntfs_device *dev, int request, void *argp)
{
    ntfs_log_trace("dev %p, request %i, argp %p\n", dev, request, argp);

    // Get the device driver descriptor
    gekko_fd *fd = DEV_FD(dev);
    if (!fd) {
        errno = EBADF;
        return -1;
    }

    // Figure out which i/o control was requested
    switch (request) {

        // Get block device size (sectors)
        #if defined(BLKGETSIZE)
        case BLKGETSIZE: {
            *(u32*)argp = fd->sectorCount;
            return 0;
        }
        #endif

        // Get block device size (bytes)
        #if defined(BLKGETSIZE64)
        case BLKGETSIZE64: {
            *(u64*)argp = (fd->sectorCount * fd->sectorSize);
            return 0;
        }
        #endif

        // Get hard drive geometry
        #if defined(HDIO_GETGEO)
        case HDIO_GETGEO: {
            struct hd_geometry *geo = (struct hd_geometry*)argp;
            geo->sectors = 0;
            geo->heads = 0;
            geo->cylinders = 0;
            geo->start = fd->hiddenSectors;
            return -1;
        }
        #endif

        // Get block device sector size (bytes)
        #if defined(BLKSSZGET)
        case BLKSSZGET: {
            *(int*)argp = fd->sectorSize;
            return 0;
        }
        #endif

        // Set block device block size (bytes)
        #if defined(BLKBSZSET)
        case BLKBSZSET: {
            int sectorSize = *(int*)argp;
            fd->sectorSize = sectorSize;
            return 0;
        }
        #endif

        // Unimplemented ioctrl
        default: {
            ntfs_log_perror("Unimplemented ioctrl %i\n", request);
            errno = EOPNOTSUPP;
            return -1;
        }

    }

    return 0;
}

/**
 * Device operations for working with gekko style devices and files.
 */
struct ntfs_device_operations ntfs_device_gekko_io_ops = {
    .open       = ntfs_device_gekko_io_open,
    .close      = ntfs_device_gekko_io_close,
    .seek       = ntfs_device_gekko_io_seek,
    .read       = ntfs_device_gekko_io_read,
    .write      = ntfs_device_gekko_io_write,
    .pread      = ntfs_device_gekko_io_pread,
    .pwrite     = ntfs_device_gekko_io_pwrite,
    .sync       = ntfs_device_gekko_io_sync,
    .stat       = ntfs_device_gekko_io_stat,
    .ioctl      = ntfs_device_gekko_io_ioctl,
};
