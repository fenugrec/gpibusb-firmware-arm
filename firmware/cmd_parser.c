/*
* GPIBUSB Adapter
* cmd_parser.c
*
* This waits on input from host and acts on received commands.
*
* © 2013-2014 Steven Casagrande (scasagrande@galvant.ca)
* (c) 2018 fenugrec
*
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
**
*/

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <libopencm3/stm32/gpio.h>

#include "libc_stubs.h"
#include "hw_conf.h"
#include "cmd_parser.h"
#include "firmware.h"
#include "gpib.h"
#include "host_comms.h"
#include "ring.h"
#include "hw_backend.h"
#include "cmd_hashtable.h"
#include "cmd_handlers.h"

#include "stypes.h"


#define VERSION 5

u8 cmd_buf[1];
int partnerAddress = 1;
int myAddress;
char eos = 10; // Default end of string character.
char eos_string[3] = "";
char eos_code = EOS_NUL;
bool eoiUse = 1; // By default, we are using EOI to signal end of
// msg from instrument
bool debug = 0; // enable or disable read&write error messages
bool strip = 0;
bool autoread = 1;
bool eot_enable = 1;
char eot_char = 13; // default CR
bool listen_only = 0;
bool save_cfg = 1;
u8 status_byte = 0;
u32 timeout = 1000;
u32 seconds = 0;
// Variables for device mode
bool device_talk = false;
bool device_listen = false;
bool device_srq = false;

#define VALID_EEPROM_CODE 0xAA
#define WITH_TIMEOUT
#define WITH_WDT
//#define VERBOSE_DEBUG

static void set_eos(enum eos_codes newcode) {
	eos_code = newcode;
	switch (newcode) {
	case EOS_CRLF:
		eos_string[0] = 13;
		eos_string[1] = 10;
		eos_string[2] = 0x00;
		eos = 10;
		break;
	case EOS_LF:
		eos_string[0] = 13;
		eos_string[1] = 0x00;
		eos = 13;
		break;
	case EOS_CR:
		eos_string[0] = 10;
		eos_string[1] = 0x00;
		eos = 10;
		break;
	default:
		eos_string[0] = 0x00;
		eos = 0;
		break;
	}
}

static bool srq_state(void) {
    return !((bool)gpio_get(CONTROL_PORT, SRQ));
}


/** initialize command parser
 *
 */
void cmd_parser_init(void) {
	// Handle the EEPROM stuff
    if (read_eeprom(0x00) == VALID_EEPROM_CODE)
    {
        mode = read_eeprom(0x01);
        partnerAddress = read_eeprom(0x02);
        eot_char = read_eeprom(0x03);
        eot_enable = read_eeprom(0x04);
        eos_code = read_eeprom(0x05);
        set_eos(eos_code);
        eoiUse = read_eeprom(0x06);
        autoread = read_eeprom(0x07);
        listen_only = read_eeprom(0x08);
        save_cfg = read_eeprom(0x09);
    }
    else
    {
        write_eeprom(0x00, VALID_EEPROM_CODE);
        write_eeprom(0x01, 1); // mode
        write_eeprom(0x02, 1); // partnerAddress
        write_eeprom(0x03, 13); // eot_char
        write_eeprom(0x04, 1); // eot_enable
        write_eeprom(0x05, EOS_NUL); // eos_code
        write_eeprom(0x06, 1); // eoiUse
        write_eeprom(0x07, 1); // autoread
        write_eeprom(0x08, 0); // listen_only
        write_eeprom(0x09, 1); // save_cfg
    }
    if (mode)
    {
        gpib_controller_assign();
    }

}

/*** command handlers
 ***
 *** To add / remove commands, cmd_handlers.h and cmd_hashtable.gen
 *** must be updated
 */
void do_address(const char *args) {
	// +a:N
	partnerAddress = atoi(args);
}
void do_addr(const char *args) {
	// ++addr N
	if (*args == '\n') {
		printf("%i%c", partnerAddress, eot_char);
	} else {
		partnerAddress = atoi(args);
	}
}
void do_timeout(const char *args) {
	// +t:N
	timeout = (u32) atoi(args);
}
void do_readTimeout(const char *args) {
	// ++read_tmo_ms N
	if (*args == '\n') {
		printf("%lu%c", (unsigned long) timeout, eot_char);
	} else {
		timeout = (u32) atoi(args);
	}
}
void do_readCmd(const char *args) {
	// +read
	(void) args;
	if (!mode) return;
	if (gpib_read(eoiUse, eos_code, eos_string, eot_enable, eot_char)) {
		if (debug == 1) {
			printf("Read error occured.%c", eot_char);
		}
//delay_ms(1);
//reset_cpu();
	}
}
void do_readCmd2(const char *args) {
	// ++read
	//XXX TODO : merge with do_readCmd ?
	if (!mode) return;
	if (*args == '\n') {
		gpib_read(false, eos_code, eos_string, eot_enable, eot_char); // read until EOS condition
	} else if (*args == 'e') {
		gpib_read(true, eos_code, eos_string, eot_enable, eot_char); // read until EOI flagged
	}
	/*else if (*(buf_pnt+6) == 32) {
		// read until specified character
		}*/
}
void do_test(const char *args) {
	// +test
	(void) args;
	printf("testing%c", eot_char);
}
void do_eos(const char *args) {
	// +eos:N
	eos = atoi(args); // Parse out the end of string byte
	eos_string[0] = eos;
	eos_string[1] = 0x00;
	eos_code = EOS_CUSTOM;
}
void do_eos2(const char *args) {
	//XXX TODO : merge with +eos
	// ++eos {0|1|2|3}
	if (*args == '\n') {
		printf("%i%c", eos_code, eot_char);
	} else {
		eos_code = atoi(args);
		set_eos(eos_code);
	}
}
void do_eoi(const char *args) {
	// ++eoi {0|1}
	// +eoi:{0|1}
	if (*args == '\n') {
		printf("%i%c", eoiUse, eot_char);
	} else {
		eoiUse = (bool) atoi(args);
	}
}
void do_strip(const char *args) {
	// +strip:{0|1}
	strip = (bool) atoi(args);
}
void do_version(const char *args) {
	// +ver	XXX TODO : merge with ++ver ?
	(void) args;
	printf("%i%c", VERSION, eot_char);
}
void do_version2(const char *args) {
	// ++ver
	(void) args;
	printf("Version %i.0%c", VERSION, eot_char);
}
void do_getCmd(const char *args) {
	// +get
	u8 writeError = 0;
	(void) args;
	if (!mode) return;

	if (*args == '\n') {
		writeError = writeError || gpib_address_target(partnerAddress);
		//XXX TODO : do something with writeError
		cmd_buf[0] = CMD_GET;
		gpib_cmd(cmd_buf);
	}
	/*else if (*(buf_pnt+5) == 32) {
	TODO: Add support for specified addresses
	}*/
}
void do_trg(const char *args) {
	// ++trg , XXX suspiciously similar to +get
	u8 writeError = 0;
	(void) args;
	if (!mode) return;
	if (*args == '\n') {
			writeError = writeError || gpib_address_target(partnerAddress);
			//XXX TODO : do something with writeError
			cmd_buf[0] = CMD_GET;
			gpib_cmd(cmd_buf);
	}
	/*else if (*(buf_pnt+5) == 32) {
	TODO: Add support for specified addresses
	}*/
}
void do_autoRead(const char *args) {
	// +autoread:{0|1}
	// ++auto {0|1}
	if (*args == '\n') {
		printf("%i%c", autoread, eot_char);
	} else {
		autoread = (bool) atoi(args);
	}
}
void do_reset(const char *args) {
	// +reset
	(void) args;
	delay_ms(1);
	reset_cpu();
}
void do_debug(const char *args) {
// +debug:{0|1}
// ++debug {0|1}
	if (*args == '\n') {
		printf("%i%c", debug, eot_char);
	} else {
		debug = (bool) atoi(args);
	}
}
void do_clr(const char *args) {
	// ++clr
	u8 writeError = 0;
	(void) args;
	if (!mode) return;
	//XXX TODO : do something with writeError
	// This command is special in that we must
	// address a specific instrument.
	writeError = writeError || gpib_address_target(partnerAddress);
	cmd_buf[0] = CMD_SDC;
	writeError = writeError || gpib_cmd(cmd_buf);
}
void do_eotEnable(const char *args) {
	// ++eot_enable {0|1}
	if (*args == '\n') {
		printf("%i%c", eot_enable, eot_char);
	} else {
		eot_enable = (bool) atoi(args);
	}
}
void do_eotChar(const char *args) {
	// ++eot_char N
	if (*args == '\n') {
		printf("%i%c", eot_char, eot_char);
	} else {
		eot_char = atoi(args);
	}
}
void do_ifc(const char *args) {
	// ++ifc
	(void) args;
	if (!mode) return;
	gpio_clear(CONTROL_PORT, IFC);
	delay_ms(200);
	gpio_set(CONTROL_PORT, IFC);
}
void do_llo(const char *args) {
	// ++llo
	u8 writeError = 0;
	(void) args;
	//XXX TODO : do something with writeError
	if (!mode) return;
	writeError = writeError || gpib_address_target(partnerAddress);
	cmd_buf[0] = CMD_LLO;
	writeError = writeError || gpib_cmd(cmd_buf);
}
void do_loc(const char *args) {
	// ++loc
	u8 writeError = 0;
	(void) args;
	//XXX TODO : do something with writeError
	if (!mode) return;

	writeError = writeError || gpib_address_target(partnerAddress);
	cmd_buf[0] = CMD_GTL;
	writeError = writeError || gpib_cmd(cmd_buf);
}
void do_lon(const char *args) {
	// ++lon {0|1}
	//TODO : listen mode
	if (mode) return;
	if (*args == '\n') {
		printf("%i%c", listen_only, eot_char);
	} else {
		listen_only = (bool) atoi(args);
	}
}
void do_mode(const char *args) {
	// ++mode {0|1}
	if (*args == '\n') {
		printf("%i%c", mode, eot_char);
	} else {
		mode = (bool) atoi(args);
		prep_gpib_pins(mode);
		if (mode) {
			gpib_controller_assign();
		}
	}
}
void do_savecfg(const char *args) {
	// ++savecfg {0|1}
	if (*args == '\n') {
		printf("%i%c", save_cfg, eot_char);
	} else {
		save_cfg = (bool) atoi(args);
		if (save_cfg) {
			write_eeprom(0x01, mode);
			write_eeprom(0x02, partnerAddress);
			write_eeprom(0x03, eot_char);
			write_eeprom(0x04, eot_enable);
			write_eeprom(0x05, eos_code);
			write_eeprom(0x06, eoiUse);
			write_eeprom(0x07, autoread);
			write_eeprom(0x08, listen_only);
			write_eeprom(0x09, save_cfg);
		}
	}
}
void do_srq(const char *args) {
	// ++srq
	(void) args;
	if (!mode) return;
	printf("%i%c", srq_state(), eot_char);
}
void do_spoll(const char *args) {
	// ++spoll N
	if (!mode) return;
	if (*args == '\n') {
		if (!gpib_serial_poll(partnerAddress, &status_byte)) {
			printf("%u%c", (unsigned) status_byte, eot_char);
		}
	} else {
		if (!gpib_serial_poll(atoi(args), &status_byte)) {
			printf("%u%c", (unsigned) status_byte, eot_char);
		}
	}
}
void do_status(const char *args) {
	// ++status
	if (mode) return;
	if (*args == '\n') {
		printf("%u%c", (unsigned) status_byte, eot_char);
	} else {
		status_byte = (u8) atoi(args);
	}
}
void do_help(const char *args) {
	//XXX TODO
	(void) args;
}
void do_nothing(const char *args) {
	(void) args;
	if (debug) {
		printf("Unrecognized command.%c", eot_char);
	}
}

/** Parse command
 *
 * @param cmd 0-split command plus args (starts with '+' or "++"), e.g. {"+a:","31"}
 * @param cmd_len : len (excluding 0) of command token, e.g. strlen("+a:")
 *
 */
static void chunk_cmd(char *cmd, unsigned cmd_len) {
    cmd_find_run(cmd, cmd_len, &cmd[cmd_len + 2]);
}

/** parse data
 * @param rawdata chunk of (unescaped) data to send on GPIB bus
 */
static void chunk_data(char *rawdata, unsigned len) {
	char *buf_pnt = rawdata;
    char writeError = 0;

	// Not an internal command, send to bus
	// Command all talkers and listeners to stop
	// and tell target to listen.
	if (mode)
	{
		writeError = writeError || gpib_address_target(partnerAddress);
	// Set the controller into talker mode
		cmd_buf[0] = myAddress + 0x40;
		writeError = writeError || gpib_cmd(cmd_buf);
	}
	// Send out command to the bus
#ifdef VERBOSE_DEBUG
	printf("gpib_write: %s%c",buf_pnt, eot_char);
#endif
	if (mode || device_talk)
	{
		if(eos_code != EOS_NUL)   // If have an EOS char, need to output
		{
	// termination byte to inst
			writeError = writeError || gpib_write((u8 *)buf_pnt, len, 0);
			if (!writeError)
				writeError = gpib_write((u8 *) eos_string, 0, eoiUse);
#ifdef VERBOSE_DEBUG
			printf("eos_string: %s",eos_string);
#endif
		}
		else
		{
			writeError = writeError || gpib_write((u8 *)buf_pnt, len, 1);
		}
	}
	// If cmd contains a question mark -> is a query
	if(autoread && mode)
	{
		if ((strchr((char*)buf_pnt, '?') != NULL) && !(writeError))
		{
			gpib_read(eoiUse, eos_code, eos_string, eot_enable, eot_char);
		}
		else if(writeError)
		{
			writeError = 0;
		}
	}
}

/** device mode poll */
static void device_poll(void) {
	bool eoi_status;
	// When in device mode we should be checking the status of the
	// ATN line to see what we should be doing
	if (!gpio_get(CONTROL_PORT, ATN))
	{
		if (!gpio_get(CONTROL_PORT, ATN))
		{
			gpio_clear(CONTROL_PORT, NDAC);
			// Get the CMD byte sent by the controller
			if (gpib_read_byte(cmd_buf, &eoi_status)) {
				//error (timeout ?)
				return;
			}
			gpio_set(CONTROL_PORT, NRFD);
			if (cmd_buf[0] == partnerAddress + 0x40)
			{
				device_talk = true;
#ifdef VERBOSE_DEBUG
				printf("Instructed to talk%c", eot_char);
#endif
			}
			else if (cmd_buf[0] == partnerAddress + 0x20)
			{
				device_listen = true;
#ifdef VERBOSE_DEBUG
				printf("Instructed to listen%c", eot_char);
#endif
			}
			else if (cmd_buf[0] == CMD_UNL)
			{
				device_listen = false;
#ifdef VERBOSE_DEBUG
				printf("Instructed to stop listen%c", eot_char);
#endif
			}
			else if (cmd_buf[0] == CMD_UNT)
			{
				device_talk = false;
#ifdef VERBOSE_DEBUG
				printf("Instructed to stop talk%c", eot_char);
#endif
			}
			else if (cmd_buf[0] == CMD_SPE)
			{
				device_srq = true;
#ifdef VERBOSE_DEBUG
				printf("SQR start%c", eot_char);
#endif
			}
			else if (cmd_buf[0] == CMD_SPD)
			{
				device_srq = false;
#ifdef VERBOSE_DEBUG
				printf("SQR end%c", eot_char);
#endif
			}
			else if (cmd_buf[0] == CMD_DCL)
			{
				printf("%c%c", CMD_DCL, eot_char);
				device_listen = false;
				device_talk = false;
				device_srq = false;
				status_byte = 0;
			}
			else if ((cmd_buf[0] == CMD_LLO) && (device_listen))
			{
				printf("%c%c", CMD_LLO, eot_char);
			}
			else if ((cmd_buf[0] == CMD_GTL) && (device_listen))
			{
				printf("%c%c", CMD_GTL, eot_char);
			}
			else if ((cmd_buf[0] == CMD_GET) && (device_listen))
			{
				printf("%c%c", CMD_GET, eot_char);
			}
			gpio_set(CONTROL_PORT, NDAC);
		}
	}
	else
	{
		delay_us(10);
		if(gpio_get(CONTROL_PORT, ATN))
		{
			if ((device_listen))
			{
				gpio_clear(CONTROL_PORT, NDAC);
#ifdef VERBOSE_DEBUG
				printf("Starting device mode gpib_read%c", eot_char);
#endif
				gpib_read(eoiUse, eos_code, eos_string, eot_enable, eot_char);
				device_listen = false;
			}
			else if (device_talk && device_srq)
			{
				gpib_write(&status_byte, 1, 0);
				device_srq = false;
				device_talk = false;
			}
		}
	}
}

/** wait for command inputs, or GPIB traffic if in device mode
 *
 * Assumes an interrupt-based process is feeding the input FIFO.
 * This func does not return
 */
void cmd_poll(void) {
	u8 rxb;
	u8 input_buf[HOST_IN_BUFSIZE];
	unsigned in_len = 0;
	unsigned cmd_len = 0;	//length of command token, excluding 0 termination. Args start at cmd_len+2
	bool in_cmd = 0;
	bool escape_next = 0;
	bool wait_guardbyte = 0;

	while (1) {
#ifdef WITH_WDT
		restart_wdt();
#endif
		if (!mode) {
			device_poll();
		}

		//build chunk from FIFO
		if (ring_read_ch(&input_ring, &rxb) == -1) {
			//no data
			continue;
		}
		if ((in_len == 0) && (rxb == '+')) {
			//start of new command chunk
			in_cmd = 1;
			cmd_len = 0;
		}

		if (wait_guardbyte) {
			//just finished a chunk; make sure it was valid
			wait_guardbyte = 0;
			if (rxb == CHUNK_INVALID) {
				//discard
				in_len = 0;
				continue;
			}
			if (in_cmd) {
				chunk_cmd((char *) input_buf, cmd_len);
			} else {
				chunk_data((char *) input_buf, in_len);
			}
			in_len = 0;
			continue;
		}

		if (!escape_next && (rxb == 27)) {
			//regardless of chunk type (command or data),
			//strip escape char
			escape_next = 1;
			continue;
		}

		if (in_cmd) {
			if (!escape_next && (rxb == '\n')) {
				//0-terminate.
				wait_guardbyte = 1;
				input_buf[in_len] = 0;
				continue;
			}
			//also, tokenize now instead of calling strtok later.
			if (rxb == ':') {
				//commands of form "+<cmd>:<args>" : split args after ':'
				input_buf[in_len++] = rxb;
				cmd_len = in_len;
				input_buf[in_len++] = 0;
			} else if (rxb == ' ') {
				cmd_len = in_len;
				//commands of form "++<cmd> <args>": split args on ' '
				input_buf[in_len++] = 0;
			} else {
				input_buf[in_len++] = rxb;
			}
			escape_next = 0;
			continue;
		}

		// if we made it here, we're dealing with data.
		if (!escape_next && (rxb == '\n')) {
			//terminate.
			wait_guardbyte = 1;
			continue;
		}
		input_buf[in_len++] = rxb;
		escape_next = 0;
	}	//while 1
}
