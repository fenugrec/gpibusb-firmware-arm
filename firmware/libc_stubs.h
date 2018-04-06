#ifndef _LIBC_STUBS_H
#define _LIBC_STUBS_H

/* some replacements for standard C funcs that pull in too many dependencies.
 *
 * define USE_STDLIB to disable these alternates
 */

//#define USE_STDLIB

#ifdef USE_STDLIB
	#include <stdlib.h>	//atoi
#endif // USE_STDLIB

#define atoi(x) htoi(x)

int htoi(const char *buf);

#endif // _LIBC_STUBS_H
