/**
 * ntfs_dir.c - devoptab directory routines for NTFS-based devices.
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

#ifndef _NTFSDIR_H
#define _NTFSDIR_H

#include "ntfsinternal.h"
#include <sys/reent.h>

/**
 * ntfs_dir_entry - Directory entry
 */
typedef struct _ntfs_dir_entry {
    char *name;
    u64 mref;
    struct _ntfs_dir_entry *next;
} ntfs_dir_entry;

/**
 * ntfs_dir_state - Directory state
 */
typedef struct _ntfs_dir_state {
    ntfs_vd *vd;                            /* Volume this directory belongs to */
    ntfs_inode *ni;                         /* Directory descriptor */
    ntfs_dir_entry *first;                  /* The first entry in the directory */
    ntfs_dir_entry *current;                /* The current entry in the directory */
    struct _ntfs_dir_state *prevOpenDir;    /* The previous entry in a double-linked FILO list of open directories */
    struct _ntfs_dir_state *nextOpenDir;    /* The next entry in a double-linked FILO list of open directories */
} ntfs_dir_state;

/* Directory state routines */
void ntfsCloseDir (ntfs_dir_state *file);

/* Gekko devoptab directory routines for NTFS-based devices */
extern int ntfs_stat_r (struct _reent *r, const char *path, struct stat *st);
extern int ntfs_link_r (struct _reent *r, const char *existing, const char *newLink);
extern int ntfs_unlink_r (struct _reent *r, const char *name);
extern int ntfs_chdir_r (struct _reent *r, const char *name);
extern int ntfs_rename_r (struct _reent *r, const char *oldName, const char *newName);
extern int ntfs_mkdir_r (struct _reent *r, const char *path, int mode);
extern int ntfs_statvfs_r (struct _reent *r, const char *path, struct statvfs *buf);

/* Gekko devoptab directory walking routines for NTFS-based devices */
extern DIR_ITER *ntfs_diropen_r (struct _reent *r, DIR_ITER *dirState, const char *path);
extern int ntfs_dirreset_r (struct _reent *r, DIR_ITER *dirState);
extern int ntfs_dirnext_r (struct _reent *r, DIR_ITER *dirState, char *filename, struct stat *filestat);
extern int ntfs_dirclose_r (struct _reent *r, DIR_ITER *dirState);

#endif /* _NTFSDIR_H */

