#ifndef __EHCI_TYPES_H__
#define __EHCI_TYPES_H__
/* linux kernel types needed by our code */
#define __iomem

typedef unsigned long uint32_t;

#include "global.h"

#define __u32 u32
#define __le32 u32
#define dma_addr_t u32
#define __GNUG__
#define size_t u32
typedef u32 spinlock_t;

typedef enum
{
	GFP_KERNEL=1
} gfp_t;

struct timer_list
{
	int time;
};

//enum
//{
//	false=0,
//	true
//};

enum
{
	ENODEV =1,
	ETIMEDOUT,
	EINVAL,
	ENOMEM,   
};

#define jiffies 0
#define likely(x) (x)
#define unlikely(x) (x)
#define container_of(ptr, type, member) ({                      \
                        const typeof( ((type *)0)->member ) *__mptr = (ptr); \
                        (type *)( (char *)__mptr - offsetof(type,member) );})

#undef offsetof
#ifdef __compiler_offsetof
#define offsetof(TYPE,MEMBER) __compiler_offsetof(TYPE,MEMBER)
#else
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif


#endif
