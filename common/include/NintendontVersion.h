#ifndef __NINTENDONT_VERSION_H__
#define __NINTENDONT_VERSION_H__

#define NIN_MAJOR_VERSION			6
#define NIN_MINOR_VERSION			500

#define NIN_VERSION		((NIN_MAJOR_VERSION << 16) | NIN_MINOR_VERSION)

#define STRINGIZE2(s) #s
#define STRINGIZE(s) STRINGIZE2(s)
#define NIN_VERSION_STRING "$$Version:" STRINGIZE(NIN_MAJOR_VERSION) "." STRINGIZE(NIN_MINOR_VERSION)

#define LI_XBOX360
#define LI_NONUNCHUK
#define LI_SHOULDER
#define LI_ANALOG_SHOULDER_FULL
#define LI_GAMEPADASCCPRO
#define LI_NOSWAP
#define LI_NOEXIT
#define LI_NORESET
#define LI_CUSTOM_CONTROLS
#define LI_BASE64

#ifdef LI_SHOULDER
#ifdef LI_SHOULDER_DIRECT
#error LI_SHOULDER and LI_SHOULDER_DIRECT cannot be used together
#endif
#endif

#if defined(LI_XBOX360)
#define NIN_SPECIAL_VERSION			"-li-green"
#elif defined(LI_SHOULDER) && !defined(LI_NONUNCHUK)
#define NIN_SPECIAL_VERSION			"-li-navy"
#elif defined(LI_SHOULDER)
#define NIN_SPECIAL_VERSION			"-li-teal"
#elif defined(LI_SHOULDER_DIRECT) && !defined(LI_NONUNCHUK)
#define NIN_SPECIAL_VERSION			"-li-gray"
#endif

#endif
