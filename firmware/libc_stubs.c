


#include <ctype.h>	//toupper
#include <errno.h>	//for _write()

#include "host_comms.h"
#include "libc_stubs.h"

/* htoi : copied from freediag but without octal
 * atoi pulls in strtol() which is very generic
 * and also very big
 */
int htoi(const char *buf) {
	/* Hex text to int */
	int rv = 0;
	int base = 10;
	int sign=0;	//1 = positive; 0 =neg

	if (*buf != '-') {
		//change sign
		sign=1;
	} else {
		buf++;
	}

	if (*buf == '$') {
		base = 16;
		buf++;
	} else if (*buf == '0') {
		buf++;
		if (tolower(*buf) == 'x') {
			base = 16;
			buf++;
		}
	}

	while (*buf) {
		char upp = toupper(*buf);
		int val;

		if ((upp >= '0') && (upp <= '9')) {
			val = ((*buf) - '0');
		} else if ((upp >= 'A') && (upp <= 'F')) {
			val = (upp - 'A' + 10);
		} else {
			return 0;
		}
		if (val >= base) { /* Value too big for this base */
			return 0;
		}
		rv *= base;
		rv += val;

		buf++;
	}
	return sign? rv:-rv ;
}


int _write(int file, char *ptr, int len) {
	if (file == 1) {
		host_tx_m((uint8_t *)ptr, len);
		return len;
	}

	errno = EIO;
	return -1;
}
