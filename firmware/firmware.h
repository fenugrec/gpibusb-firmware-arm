#ifndef _FIRMWARE_H
#define _FIRMWARE_H

/*
* GPIBUSB Adapter
* firmware.h
**
* © 2014 Steven Casagrande (scasagrande@galvant.ca).
*
* This file is a part of the GPIBUSB Adapter project.
* Licensed under the AGPL version 3.
**
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU Affero General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU Affero General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#define WITH_WDT
#define VERBOSE_DEBUG

#ifdef VERBOSE_DEBUG
//even with verbose debug, still honor debug flag
#define DEBUG_PRINTF(fmt, ...) \
	if (!gpib_cfg.debug) { \
	} else printf(fmt, ##__VA_ARGS__)
#else
#define DEBUG_PRINTF(fmt, ...) ((void) 0)
#endif

/* Error codes
*/

enum errcodes {
	E_OK,	// "no error"
	E_TIMEOUT,
	E_FIFO,	// buffer inconsistencies etc
};

#endif // _FIRMWARE_H
