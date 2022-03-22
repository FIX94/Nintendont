#ifndef __NINTENDONT_VERSION_H__
#define __NINTENDONT_VERSION_H__

/* Minor version to be automatically filled in by GitHub CI. Any major version
 * change will have to update main.yml. Online updates fetch this file directly
 * from the repo and scanf these version numbers. */
#define NIN_MAJOR_VERSION			7
#define NIN_MINOR_VERSION			9

#define NIN_VERSION		((NIN_MAJOR_VERSION << 16) | NIN_MINOR_VERSION)

#define STRINGIZE2(s) #s
#define STRINGIZE(s) STRINGIZE2(s)
#define NIN_VERSION_STRING "$$Version:" STRINGIZE(NIN_MAJOR_VERSION) "." STRINGIZE(NIN_MINOR_VERSION)

// "Special" version.
// This should only be set in custom builds, i.e. not mainline.
//#define NIN_SPECIAL_VERSION			"-BBABeta13"

#endif
