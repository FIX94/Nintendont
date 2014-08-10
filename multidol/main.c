/*
	TinyLoad - a simple region free (original) game launcher in 4k

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

/* This code comes from HBC's stub which was based on dhewg's geckoloader stub */
// Copyright 2008-2009  Andre Heider  <dhewg@wiibrew.org>
// Copyright 2008-2009  Hector Martin  <marcan@marcansoft.com>

#include "cache.h"
#include "dip.h"
#include "utils.h"
#include "global.h"
#include "apploader.h"

u32 entrypoint = 0;
void _main(void)
{
	RAMInit();
	entrypoint = Apploader_Run();

	asm volatile (
		"lis %r3, entrypoint@h\n"
		"ori %r3, %r3, entrypoint@l\n"
		"lwz %r3, 0(%r3)\n"
		"mtlr %r3\n"
		"blr\n"
	);
}
