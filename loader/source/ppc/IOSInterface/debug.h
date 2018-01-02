/*
	TinyLoad - a simple region free (original) game launcher in 4k

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

/* This code comes from HBC's stub which was based on dhewg's geckoloader stub */
// Copyright 2008-2009  Andre Heider  <dhewg@wiibrew.org>

#ifndef _STUB_DEBUG_H_
#define _STUB_DEBUG_H_

#ifdef DEBUG

void debug_uint (u32 i);
void debug_string (const char *s);

#else

#define debug_uint(x)
#define debug_string(x)

#endif

#endif

