#ifndef _GPIB_H
#define _GPIB_H

/* GPIBUSB Adapter
* gpib.h
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

enum eos_codes {
	EOS_CRLF = 0,
	EOS_LF = 1,
	EOS_CR = 2,
	EOS_NUL = 3,
	EOS_CUSTOM = 4
	};


uint32_t gpib_cmd(uint8_t *bytes);
uint32_t gpib_write(uint8_t *bytes, uint32_t length, bool use_eoi);
uint32_t gpib_read_byte(uint8_t *byte, bool *eoi_status);
uint32_t gpib_read(bool use_eoi, enum eos_codes eos_code,
					const char *eos_string, bool eot_enable);
uint32_t gpib_address_target(uint32_t address);
uint32_t gpib_controller_assign(void);
uint32_t gpib_serial_poll(int address, uint8_t *status_byte);

#define CMD_DCL 0x14
#define CMD_UNL 0x3f
#define CMD_UNT 0x5f
#define CMD_GET 0x8
#define CMD_SDC 0x04
#define CMD_LLO 0x11
#define CMD_GTL 0x1
#define CMD_SPE 0x18
#define CMD_SPD 0x19

/* Global vars; cmd_parser needs to see this */
extern bool mode;		//1 if controller mode
extern bool debug;	// enable or disable read&write error messages
extern char eot_char;	//char to append to each string sent to host
extern uint32_t timeout;	//in milliseconds
// Variables for device mode
extern bool device_talk;
extern bool device_listen;
extern bool device_srq;


#endif // _GPIB_H/*
