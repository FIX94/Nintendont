# libogc 3 renamed the cache functions. The prebuilt extlibs/lib/libwiidrc.a
# still calls the libogc 1.x name, so point it at the new one.
#ifdef LIBOGC3
	.globl DCInvalidateRange
DCInvalidateRange:
	b	PPCDCacheInvalidate
#endif
