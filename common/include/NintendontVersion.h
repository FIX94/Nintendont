#ifndef __NINTENDONT_VERSION_H__
#define __NINTENDONT_VERSION_H__

// make fills in the version using info from GitHub CI. This default is the developer's version.
#if !defined(NIN_MAJOR_VERSION)
#   define NIN_MAJOR_VERSION		7
#endif
#if !defined(NIN_MINOR_VERSION)
#   define NIN_MINOR_VERSION		9999
#endif
#define NIN_VERSION		((NIN_MAJOR_VERSION << 16) | NIN_MINOR_VERSION)

#define STRINGIZE2(s) #s
#define STRINGIZE(s) STRINGIZE2(s)
#define NIN_VERSION_STRING "$$Version:" STRINGIZE(NIN_MAJOR_VERSION) "." STRINGIZE(NIN_MINOR_VERSION)

// "Special" version.
// This should only be set in custom builds, i.e. not mainline.
//#define NIN_SPECIAL_VERSION			"-BBABeta13"

#endif
