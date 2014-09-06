#include "global.h"
#include "string.h"
#include "bt.h"
#include "btmemb.h"

void btmemb_init(struct memb_blks *blk)
{
	MEMSET(blk->mem,0,(MEM_ALIGN_SIZE(blk->size)+sizeof(u32))*blk->num);
}

void* btmemb_alloc(struct memb_blks *blk)
{
	s32 i;
	u32 *ptr;
	void *p;

	ptr = (u32*)blk->mem;
	for(i=0;i<blk->num;i++) {
		if(*ptr==0) {
			++(*ptr);
			p = (ptr+1);
			return p;
		}
		ptr = (u32*)((u8*)ptr+(MEM_ALIGN_SIZE(blk->size)+sizeof(u32)));
	}
	return NULL;
}

u8 btmemb_free(struct memb_blks *blk,void *ptr)
{
	u8 ref;
	s32 i;
	u32 *ptr2,*ptr1;

	ptr1 = ptr;
	ptr2 = (u32*)blk->mem;
	for(i=0;i<blk->num;i++) {
		if(ptr2==(ptr1 - 1)) {
			ref = --(*ptr2);
			return ref;
		}
		ptr2 = (u32*)((u8*)ptr2+(MEM_ALIGN_SIZE(blk->size)+sizeof(u32)));
	}
	return -1;
}

u8 btmemb_ref(struct memb_blks *blk,void *ptr)
{
	u8 ref;
	u32 *pref;

	pref = ptr-sizeof(u32);
	ref = ++(*pref);
	return ref;
}
