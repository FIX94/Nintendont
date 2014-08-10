/*
apploader.h for Nintendont (Loader)

Copyright (C) 2014 FIX94

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#ifndef __APPLOADER_H__
#define __APPLOADER_H__

typedef struct _FST
{
	union
	{
		struct
		{
			unsigned int Type		:8;
			unsigned int NameOffset	:24;
		};
		unsigned int TypeName;
	};
	union
	{
		struct
		{
			unsigned int FileOffset;
			unsigned int FileLength;
		};
		struct
		{
			unsigned int ParentOffset;
			unsigned int NextOffset;
		};
		unsigned int entry[2];
	};
} FST;

struct _TGCInfo
{
	u32 tgcoffset;
	u32 doloffset;
	u32 fstoffset;
	u32 fstsize;
	u32 userpos;
	u32 fstupdate;
	u32 isTGC;
};
u32 Apploader_Run();

#endif
