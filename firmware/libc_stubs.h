#ifndef _LIBC_STUBS_H
#define _LIBC_STUBS_H

/* some replacements for standard C funcs that pull in too many dependencies.
 *
 * define USE_STDLIB to disable these alternates
 */

//#define USE_STDLIB

#ifdef USE_STDLIB
	#include <stdlib.h> //atoi
#endif // USE_STDLIB

#define atoi(x) htoi(x)

/** Decimal/Hex to integer routine
 *
 * formats:
 * [-]0x[0-9,A-F,a-f] : hex
 * [-]$[0-9,A-F,a-f] : hex
 * [-][0-9] : dec
 * Returns 0 if unable to decode.
 */
int htoi(const char *buf);


/** _write implementation to override newlib dummy stub.
 *
 * printf needs this. Currently no error checking
 */
int _write(int file, char *ptr, int len);


/** printf data as hexpairs
 *
 * format : "%02X %02X ...."
 */
void print_hex(const uint8_t *data, unsigned len);


#endif // _LIBC_STUBS_H
