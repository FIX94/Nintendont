/*
	TinyLoad - a simple region free (original) game launcher in 4k

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

/* This code comes from HBC's stub which was based on dhewg's geckoloader stub */
// Copyright 2008-2009  Andre Heider  <dhewg@wiibrew.org>

typedef unsigned int u32;
#include "usb.h"

#ifdef DEBUG

static char int2hex[] = "0123456789abcdef";

void debug_uint (u32 i) {
        int j;

        usb_sendbuffersafe ("0x", 2);
        for (j = 0; j < 8; ++j) {
                usb_sendbuffersafe (&int2hex[(i >> 28) & 0xf], 1);
                i <<= 4;
        }
}

void debug_string (const char *s) {
        u32 i = 0;

        while (s[i])
                i++;

        usb_sendbuffersafe (s, i);
}

#endif

