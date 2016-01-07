/**
 * ntfsinternal.h - Internal support routines for NTFS-based devices.
 *
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

#ifndef _NTFSINTERNAL_H
#define _NTFSINTERNAL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "types.h"
#include "compat.h"
#include "logging.h"
#include "layout.h"
#include "device.h"
#include "volume.h"
#include "dir.h"
#include "inode.h"
#include "attrib.h"
#include "reparse.h"
#include "security.h"
#include "efs.h"
#include "unistr.h"

#include <gccore.h>
#include <ogc/disc_io.h>
#include <sys/iosupport.h>

#define NTFS_MOUNT_PREFIX                   "ntfs" /* Device name prefix to use when auto-mounting */
#define NTFS_MAX_PARTITIONS                 32 /* Maximum number of partitions that can be found */
#define NTFS_MAX_MOUNTS                     10 /* Maximum number of mounts available at one time */
#define NTFS_MAX_SYMLINK_DEPTH              10 /* Maximum search depth when resolving symbolic links */

#define NTFS_OEM_ID                         cpu_to_le64(0x202020205346544eULL) /* "NTFS    " */

#define MBR_SIGNATURE                       cpu_to_le16(0xAA55)
#define EBR_SIGNATURE                       cpu_to_le16(0xAA55)

#define PARTITION_STATUS_NONBOOTABLE        0x00 /* Non-bootable */
#define PARTITION_STATUS_BOOTABLE           0x80 /* Bootable (active) */

#define PARTITION_TYPE_EMPTY                0x00 /* Empty */
#define PARTITION_TYPE_DOS33_EXTENDED       0x05 /* DOS 3.3+ extended partition */
#define PARTITION_TYPE_NTFS                 0x07 /* Windows NT NTFS */
#define PARTITION_TYPE_WIN95_EXTENDED       0x0F /* Windows 95 extended partition */

/* Forward declarations */
struct _ntfs_file_state;
struct _ntfs_dir_state;

/**
 * PRIMARY_PARTITION - Block device partition record
 */
typedef struct _PARTITION_RECORD {
    u8 status;                              /* Partition status; see above */
    u8 chs_start[3];                        /* Cylinder-head-sector address to first block of partition */
    u8 type;                                /* Partition type; see above */
    u8 chs_end[3];                          /* Cylinder-head-sector address to last block of partition */
    u32 lba_start;                          /* Local block address to first sector of partition */
    u32 block_count;                        /* Number of blocks in partition */
} __attribute__((__packed__)) PARTITION_RECORD;

/**
 * MASTER_BOOT_RECORD - Block device master boot record
 */
typedef struct _MASTER_BOOT_RECORD {
    u8 code_area[446];                      /* Code area; normally empty */
    PARTITION_RECORD partitions[4];         /* 4 primary partitions */
    u16 signature;                          /* MBR signature; 0xAA55 */
} __attribute__((__packed__)) MASTER_BOOT_RECORD;

/**
 * EXTENDED_PARTITION - Block device extended boot record
 */
typedef struct _EXTENDED_BOOT_RECORD {
    u8 code_area[446];                      /* Code area; normally empty */
    PARTITION_RECORD partition;             /* Primary partition */
    PARTITION_RECORD next_ebr;              /* Next extended boot record in the chain */
    u8 reserved[32];                        /* Normally empty */
    u16 signature;                          /* EBR signature; 0xAA55 */
} __attribute__((__packed__)) EXTENDED_BOOT_RECORD;

/**
 * INTERFACE_ID - Disc interface identifier
 */
typedef struct _INTERFACE_ID {
    const char *name;                       /* Interface name */
    const DISC_INTERFACE *interface;        /* Disc interface */
} INTERFACE_ID;

/**
 * ntfs_atime_t - File access time update strategies
 */
typedef enum {
    ATIME_ENABLED,                          /* Update access times */
    ATIME_DISABLED                          /* Don't update access times */
} ntfs_atime_t;

/**
 * ntfs_vd - NTFS volume descriptor
 */
typedef struct _ntfs_vd {
    struct ntfs_device *dev;                /* NTFS device handle */
    ntfs_volume *vol;                       /* NTFS volume handle */
    mutex_t lock;                           /* Volume lock mutex */
    s64 id;                                 /* Filesystem id */
    u32 flags;                              /* Mount flags */
    char name[128];                         /* Volume name (cached) */
    u16 uid;                                /* User id for entry creation */
    u16 gid;                                /* Group id for entry creation */
    u16 fmask;                              /* Unix style permission mask for file creation */
    u16 dmask;                              /* Unix style permission mask for directory creation */
    ntfs_atime_t atime;                     /* Entry access time update strategy */
    bool showHiddenFiles;                   /* If true, show hidden files when enumerating directories */
    bool showSystemFiles;                   /* If true, show system files when enumerating directories */
    ntfs_inode *cwd_ni;                     /* Current directory */
    struct _ntfs_dir_state *firstOpenDir;   /* The start of a FILO linked list of currently opened directories */
    struct _ntfs_file_state *firstOpenFile; /* The start of a FILO linked list of currently opened files */
    u16 openDirCount;                       /* The total number of directories currently open in this volume */
    u16 openFileCount;                      /* The total number of files currently open in this volume */
} ntfs_vd;

/* Lock volume */
static inline void ntfsLock (ntfs_vd *vd)
{
    LWP_MutexLock(vd->lock);
}

/* Unlock volume */
static inline void ntfsUnlock (ntfs_vd *vd)
{
    LWP_MutexUnlock(vd->lock);
}

/* Gekko device related routines */
int ntfsAddDevice (const char *name, void *deviceData);
void ntfsRemoveDevice (const char *path);
const devoptab_t *ntfsGetDevice (const char *path, bool useDefaultDevice);
const devoptab_t *ntfsGetDevOpTab (void);
const INTERFACE_ID* ntfsGetDiscInterfaces (void);

/* Miscellaneous helper/support routines */
int ntfsInitVolume (ntfs_vd *vd);
void ntfsDeinitVolume (ntfs_vd *vd);
ntfs_vd *ntfsGetVolume (const char *path);
ntfs_inode *ntfsOpenEntry (ntfs_vd *vd, const char *path);
ntfs_inode *ntfsParseEntry (ntfs_vd *vd, const char *path, int reparseLevel);
void ntfsCloseEntry (ntfs_vd *vd, ntfs_inode *ni);
ntfs_inode *ntfsCreate (ntfs_vd *vd, const char *path, mode_t type, const char *target);
int ntfsLink (ntfs_vd *vd, const char *old_path, const char *new_path);
int ntfsUnlink (ntfs_vd *vd, const char *path);
int ntfsSync (ntfs_vd *vd, ntfs_inode *ni);
int ntfsStat (ntfs_vd *vd, ntfs_inode *ni, struct stat *st);
void ntfsUpdateTimes (ntfs_vd *vd, ntfs_inode *ni, ntfs_time_update_flags mask);

const char *ntfsRealPath (const char *path);
int ntfsUnicodeToLocal (const ntfschar *ins, const int ins_len, char **outs, int outs_len);
int ntfsLocalToUnicode (const char *ins, ntfschar **outs);

#endif /* _NTFSINTERNAL_H */
