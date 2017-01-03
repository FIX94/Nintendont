#ifndef _STDINT_WRAP_H
#define _STDINT_WRAP_H

#define int32_t dontcare_int32_t
#define uint32_t dontcare_uint32_t

#include_next <stdint.h>

#undef int32_t
#undef uint32_t

typedef signed int int32_t;
typedef unsigned int uint32_t;

#endif