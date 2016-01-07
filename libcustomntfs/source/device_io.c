/*
 * device_io.c - Default device io operations. Originated from the Linux-NTFS project.
 *
 * Copyright (c) 2003 Anton Altaparmakov
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
 * along with this program (in the main directory of the NTFS-3G
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#ifndef GEKKO
#ifndef NO_NTFS_DEVICE_DEFAULT_IO_OPS

#ifndef __CYGWIN32__

/* Not on Cygwin; use standard Unix style low level device operations. */
#include "unix_io.c"

#else /* __CYGWIN32__ */

/* On Cygwin; use Win32 low level device operations. */
#include "win32_io.c"

#endif /* __CYGWIN32__ */

#endif /* NO_NTFS_DEVICE_DEFAULT_IO_OPS */
#endif /* GEKKO */
