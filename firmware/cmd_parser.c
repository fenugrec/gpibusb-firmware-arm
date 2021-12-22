/*
* GPIBUSB Adapter
* cmd_parser.c
*
* This waits on input from host and acts on received commands.
*
* © 2013-2014 Steven Casagrande (scasagrande@galvant.ca)
* (c) 2018-2021 fenugrec
* bits of Steve Matos' (steve1515) fork were used in here too
*
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
#include <string.h>

#include "printf_config.h"	//hax, just to get PRINTF_ALIAS_STANDARD_FUNCTION_NAMES...
#include <printf/printf.h>

#include <libopencm3/stm32/gpio.h>

#include "libc_stubs.h"
#include "hw_conf.h"
#include "cmd_parser.h"
#include "firmware.h"
#include "gpib.h"
#include "host_comms.h"
#include "ecbuff.h"
#include "hw_backend.h"
#include "cmd_hashtable.h"
#include "cmd_handlers.h"

#include "stypes.h"


#define VALID_EEPROM_CODE 0xAA
#define MAX_TIMEOUT 10*1000	//10 seconds, is there any reason to allow more than this

#define VERSION 5

u8 cmd_buf[1];
char eos_string[3] = "";
unsigned eos_len = 0;
char eos_code = EOS_NUL;
bool eoiUse = 1; // By default, we are using EOI to signal end of
// msg from instrument
bool strip = 0;
bool autoread = 1;
bool eot_enable = 1;
bool listen_only = 0;
bool save_cfg = 1;
u8 status_byte = 0;

static void set_eos(enum eos_codes newcode) {
	eos_code = newcode;
	switch (newcode) {
	case EOS_CRLF:
		eos_string[0] = 13;
		eos_string[1] = 10;
		eos_string[2] = 0x00;
		eos_len = 2;
		break;
	case EOS_LF:
		eos_string[0] = 13;
		eos_string[1] = 0x00;
		eos_len = 1;
		break;
	case EOS_CR:
		eos_string[0] = 10;
		eos_string[1] = 0x00;
		eos_len = 1;
		break;
	default:
		eos_string[0] = 0x00;
		eos_len = 0;
		break;
	}
}

static bool srq_state(void) {
	return !((bool)gpio_get(SRQ_CP, SRQ));
}

/** need this for most responses ?
 * as it is, we'll append the EOT char only if eot_enable = 1.
 * not sure what original prologix does. This is copied from steve1515's fork
 */
#define eot_printf(fmt, ...) do {\
	printf((fmt), ##__VA_ARGS__);\
	if (eot_enable) printf("%c", eot_char);\
} while (0)


void cmd_parser_init(void) {
	// Handle the EEPROM stuff
	if (read_eeprom(0x00) == VALID_EEPROM_CODE)
	{
		controller_mode = read_eeprom(0x01);
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
	if (controller_mode)
	{
		gpib_controller_assign();
	}

	printf("Command parser ready\n");

}

/*** command handlers
 ***
 *** To add / remove commands, cmd_handlers.h and cmd_hashtable.gen
 *** must be updated
 */

void do_addr(const char *args) {
	// ++addr N
	// TODO : addr [pad [sad]] for secondary address too
	if (*args == 0) {
		eot_printf("%i", partnerAddress);
	} else {
		partnerAddress = atoi(args);
	}
}
void do_readTimeout(const char *args) {
	// ++read_tmo_ms N
	if (*args == 0) {
		eot_printf("%lu", (unsigned long) timeout);
		return;
	}
	u32 temp = (u32) atoi(args);

	/* bounds-check timeout value before saving */
	if (temp > MAX_TIMEOUT) temp=MAX_TIMEOUT;
	timeout = temp;
}
void do_readCmd2(const char *args) {
	// ++read [eoi|<char>]
	//XXX TODO : err msg when read error occurs
	if (!controller_mode) return;
	if (*args == 0) {
		gpib_read(GPIBREAD_TMO,0, eot_enable); // read until EOS condition
	} else if (strncmp(args, "eoi", 3) == 0) {
		gpib_read(GPIBREAD_EOI, 0, eot_enable); // read until EOI flagged
	} else {
		// read until specified character
		u8 tmp_eos = htoi(args);
		gpib_read(GPIBREAD_EOS, tmp_eos, eot_enable);
	}
}
void do_eos2(const char *args) {
	// ++eos {0|1|2|3}
	if (*args == 0) {
		eot_printf("%i", eos_code);
	} else {
		eos_code = atoi(args);
		set_eos(eos_code);
	}
}
void do_eoi(const char *args) {
	// ++eoi {0|1}
	if (*args == 0) {
		eot_printf("%i", eoiUse);
	} else {
		eoiUse = (bool) atoi(args);
	}
}
void do_strip(const char *args) {
	// ++strip {0|1}
	strip = (bool) atoi(args);
}
void do_version2(const char *args) {
	// ++ver
	(void) args;
	eot_printf("Version %i.0", VERSION);
}
void do_trg(const char *args) {
	// ++trg [<PAD1> [<SAD1>] <PAD2> [SAD2] … <PAD15> [<SAD15>]]
	u8 writeError = 0;
	(void) args;
	if (!controller_mode) return;
	if (*args == 0) {
			writeError = writeError || gpib_address_target(partnerAddress);
			//XXX TODO : do something with writeError
			cmd_buf[0] = CMD_GET;
			gpib_cmd(cmd_buf);
	} else {
		//TODO: Add support for specified addresses
	}
}
void do_autoRead(const char *args) {
	// ++auto {0|1}
	if (*args == 0) {
		eot_printf("%i", autoread);
	} else {
		autoread = (bool) atoi(args);
	}
}
void do_reset(const char *args) {
	// ++rst
	(void) args;
	printf("Rebooting in 3 seconds !\n");
	delay_ms(3000);
	reset_cpu();
}

void do_reset_dfu(const char *args) {
	// ++dfu
	(void) args;
	printf("Rebooting in DFU mode in 3 seconds !\n");
	delay_ms(3000);
	reset_dfu();
}

void do_debug(const char *args) {
	// ++debug {0|1}
	if (*args == 0) {
		eot_printf("%i", debug);
	} else {
		debug = (bool) atoi(args);
	}
}
void do_clr(const char *args) {
	// ++clr
	u8 writeError = 0;
	(void) args;
	if (!controller_mode) return;
	//XXX TODO : do something with writeError
	// This command is special in that we must
	// address a specific instrument.
	writeError = writeError || gpib_address_target(partnerAddress);
	cmd_buf[0] = CMD_SDC;
	writeError = writeError || gpib_cmd(cmd_buf);
}
void do_eotEnable(const char *args) {
	// ++eot_enable {0|1}
	if (*args == 0) {
		eot_printf("%i", eot_enable);
	} else {
		eot_enable = (bool) atoi(args);
	}
}
void do_eotChar(const char *args) {
	// ++eot_char N
	if (*args == 0) {
		eot_printf("%i", eot_char);
	} else {
		eot_char = atoi(args);
	}
}
void do_ifc(const char *args) {
	// ++ifc
	(void) args;
	if (!controller_mode) return;
	output_low(IFC_CP, IFC);
	delay_ms(200);
	output_high(IFC_CP, IFC);	//XXX orig version just tristates IFC ? we're controller, who cares ?
}
void do_llo(const char *args) {
	// ++llo
	u8 writeError = 0;
	(void) args;
	//XXX TODO : do something with writeError
	if (!controller_mode) return;
	writeError = writeError || gpib_address_target(partnerAddress);
	cmd_buf[0] = CMD_LLO;
	writeError = writeError || gpib_cmd(cmd_buf);
}
void do_loc(const char *args) {
	// ++loc
	u8 writeError = 0;
	(void) args;
	//XXX TODO : do something with writeError
	if (!controller_mode) return;

	writeError = writeError || gpib_address_target(partnerAddress);
	cmd_buf[0] = CMD_GTL;
	writeError = writeError || gpib_cmd(cmd_buf);
}
void do_lon(const char *args) {
	// ++lon {0|1}
	if (*args == 0) {
		eot_printf("%i", listen_only);
	} else {
		if (controller_mode) { return; }
		listen_only = (bool) atoi(args);
	}
}
void do_mode(const char *args) {
	// ++mode {0|1}
	if (*args == 0) {
		eot_printf("%i", controller_mode);
	} else {
		controller_mode = (bool) atoi(args);
		prep_gpib_pins(controller_mode);
		if (controller_mode) {
			gpib_controller_assign();
		}
	}
}
void do_savecfg(const char *args) {
	// ++savecfg {0|1}
	if (*args == 0) {
		eot_printf("%i", save_cfg);
	} else {
		save_cfg = (bool) atoi(args);
		if (save_cfg) {
			write_eeprom(0x01, controller_mode);
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
	if (!controller_mode) return;
	eot_printf("%i", srq_state());
}
void do_spoll(const char *args) {
	// ++spoll [pad [sad]]
	// TODO : support secodnary addr too
	if (!controller_mode) return;
	if (*args == 0) {
		if (!gpib_serial_poll(partnerAddress, &status_byte)) {
			eot_printf("%u", (unsigned) status_byte);
		}
	} else {
		if (!gpib_serial_poll(atoi(args), &status_byte)) {
			eot_printf("%u", (unsigned) status_byte);
		}
	}
}
void do_status(const char *args) {
	// ++status [n]
	if (controller_mode) return;
	if (*args == 0) {
		eot_printf("%u", (unsigned) status_byte);
	} else {
		status_byte = (u8) atoi(args);
		if (status_byte & 0x40) {
			// prologix: " If the RQS bit (bit #6) of the status byte is set then the SRQ signal is asserted (low)
			// After a serial poll, SRQ line is de-asserted and status byte is set to 0 "
			output_low(SRQ_CP, SRQ);
		} // else { output_high ??? or assume the transition to device mode already did this}
	}
}
void do_help(const char *args) {
	//XXX TODO
	(void) args;
}
void do_nothing(const char *args) {
	(void) args;
	DEBUG_PRINTF("Unrecognized command.\n");
}

/** Parse command
 *
 * @param cmd 0-split command plus args (starts with '+' or "++"), e.g. {"+a","31"}
 * @param cmd_len : len (excluding 0) of command token, e.g. strlen("+a:")
 * @param has_args : if 0, *args will point to a 0x00
 *
 */
static void chunk_cmd(char *cmd, unsigned cmd_len, bool has_args) {
	if (cmd_len == 0) {
		//can happen if we receive a stray LF from host
		return;
	}
	if (has_args) {
		cmd_find_run(cmd, cmd_len, &cmd[cmd_len + 1]);
	} else {
		cmd_find_run(cmd, cmd_len, &cmd[cmd_len + 0]);	//trailing 0 of command
	}
}

/** parse data
 * @param rawdata chunk of (unescaped) data to send on GPIB bus
 */
static void chunk_data(char *rawdata, unsigned len) {
	char *buf_pnt = rawdata;
	char writeError = 0;

	if (len == 0) {
		//can happen if we receive a stray LF from host
		return;
	}

	// Not an internal command, send to bus
	// Command all talkers and listeners to stop
	// and tell target to listen.
	if (controller_mode)
	{
		writeError = writeError || gpib_address_target(partnerAddress);
	// Set the controller into talker mode
		cmd_buf[0] = myAddress + CMD_TAD;
		writeError = writeError || gpib_cmd(cmd_buf);
	}
	// Send out command to the bus
	DEBUG_PRINTF("gpib_write: %.*s\n", len, buf_pnt);

	if (controller_mode || device_talk)
	{
		if(eos_code != EOS_NUL)   // If have an EOS char, need to output
		{
	// termination byte to inst
			writeError = writeError || gpib_write((u8 *)buf_pnt, len, 0);
			if (!writeError) {
				DEBUG_PRINTF("gpib_write eos[%u] (%02X...)", eos_len, eos_string[0]);
				writeError = gpib_write((u8 *) eos_string, eos_len, eoiUse);
			}

		}
		else
		{
			writeError = writeError || gpib_write((u8 *)buf_pnt, len, 1);
		}
	}

	if(autoread && controller_mode) {
		//XXX TODO : with autoread, does prologix terminate by EOI ?
		writeError = writeError || gpib_read(GPIBREAD_EOI, 0, eot_enable);
	}
}


static void listenonly(void) {
	output_float(DIO_PORT, DIO_PORTMASK);
	output_float(EOI_CP, DAV | EOI);
	gpio_clear(FLOW_PORT, TE);
	output_low(NDAC_CP, NDAC);
	output_high(NRFD_CP, NRFD);

	// if DAV=1 , not valid : return
	if (gpio_get(DAV_CP, DAV)) {
		return;
	}
	bool atn_state = !gpio_get(ATN_CP, ATN);

	if (atn_state) {
		u8 rxb;
		bool eoi_status;
		gpib_read_byte(&rxb, &eoi_status);
		DEBUG_PRINTF("listenonly : CMD %02X\n", rxb);
	} else {
		gpib_read(GPIBREAD_EOI, 0, 0);
	}
}

//fwd decls for two helper functions
static void device_atn(void);
static void device_noatn(void);


/** device mode poll */
static void device_poll(void) {
	// When in device mode, need to monitor IFC and ATN.
	// but we certainly don't respect t2 <= 200ns for responding to ATN...
	// but we have t1 (100us) to respond to IFC ?

	if (!gpio_get(IFC_CP, IFC)) {
		device_talk = 0;
		device_listen = 0;
		device_srq = 0;
		status_byte = 0;
		return;
	}

	if (!gpio_get(ATN_CP, ATN)) {
		device_atn();
	} else {
		device_noatn();
	}
}

/** device poll with ATN asserted (==0) */
static void device_atn(void) {
	bool eoi_status;
	output_float(DIO_PORT, DIO_PORTMASK);
	output_float(EOI_CP, DAV | EOI);
	gpio_clear(FLOW_PORT, TE);

	output_low(NDAC_CP, NDAC);
	output_high(NRFD_CP, NRFD);
	// if DAV=1 not valid : return
	if (gpio_get(DAV_CP, DAV)) {
		return;
	}
	// Get the CMD byte sent by the controller
	u8 rxb;
	if (gpib_read_byte(&rxb, &eoi_status)) {
		//error (timeout ?)
		return;
	}
	if ((rxb & 0xE0) == CMD_TAD) {
		if (rxb == partnerAddress + CMD_TAD) {
			device_talk = true;
			device_listen = 0;
			DEBUG_PRINTF("Instructed to talk\n");
		} else {
			//somebody else is addressed to talk
			device_talk = 0;
		}
	} else if (rxb == partnerAddress + CMD_LAD) {
		device_talk = 0;
		device_listen = true;
		DEBUG_PRINTF("Instructed to listen\n");
	} else if (rxb == CMD_UNL) {
		device_listen = false;
		DEBUG_PRINTF("Instructed to stop listen\n");
	} else if (rxb == CMD_UNT) {
		device_talk = false;
		DEBUG_PRINTF("Instructed to stop talk\n");
	} else if (rxb == CMD_SPE) {
		device_srq = true;
		DEBUG_PRINTF("SQR start\n");
	} else if (rxb == CMD_SPD) {
		device_srq = false;
		DEBUG_PRINTF("SQR end\n");
	} else if (rxb == CMD_DCL) {
		eot_printf("DCL");
		device_listen = false;
		device_talk = false;
		device_srq = false;
		status_byte = 0;
	} else if ((rxb == CMD_LLO) && (device_listen)) {
		eot_printf("LLO");
	} else if ((rxb == CMD_GTL) && (device_listen)) {
		eot_printf("GTL");
	} else if ((rxb == CMD_GET) && (device_listen)) {
		eot_printf("GET");
	} else if ((rxb == CMD_SDC) && (device_listen)) {
		eot_printf("SDC");
		device_listen = false;
		device_talk = false;
		device_srq = false;
		status_byte = 0;
	}
	output_high(NDAC_CP, NDAC);
}

/** device poll with ATN not asserted (==1) */
static void device_noatn(void) {
	if (device_listen) {
		output_float(DIO_PORT, DIO_PORTMASK);
		output_float(EOI_CP, DAV | EOI);
		gpio_clear(FLOW_PORT, TE);
		output_low(NDAC_CP, NDAC);
		output_high(NRFD_CP, NRFD);

		// if DAV=1 , not valid : return
		if (gpio_get(DAV_CP, DAV)) {
			return;
		}
		DEBUG_PRINTF("device mode gpib_read\n");
		gpib_read(GPIBREAD_EOI, 0, eot_enable);
	} else if (device_talk) {
		output_float(NDAC_CP, NDAC | NRFD);
		gpio_set(FLOW_PORT, TE);
		output_high(EOI_CP, DAV | EOI);
		if (device_srq) {
			gpib_write(&status_byte, 1, 0);
			output_high(SRQ_CP, SRQ);
			device_srq = false;
			status_byte = 0;
		}
	}
}


void cmd_poll(void) {
	u8 rxb;
	static u8 input_buf[HOST_IN_BUFSIZE];
	static unsigned in_len = 0;
	static unsigned cmd_len = 0;	//length of command token, excluding 0 termination.
	static unsigned arg_pos = 0;
	static bool in_cmd = 0;
	static bool escape_next = 0;
	static bool wait_guardbyte = 0;
	static bool has_args = 0;

	restart_wdt();

	if (listen_only) {
		listenonly();
	} else if (!controller_mode) {
		device_poll();
	}

	//build chunk from FIFO
	if (!ecbuff_read(fifo_in, &rxb)) {
		//no data
		return;
	}
	if (in_len == 0) {
		if (rxb == '+') {
			//start of new command chunk
			in_cmd = 1;
			cmd_len = 0;
			has_args = 0;
			arg_pos = 0;
		} else {
			//data chunk
			in_cmd = 0;
		}
	}

	if (wait_guardbyte) {
		//just finished a chunk; make sure it was valid
		wait_guardbyte = 0;
		if (rxb != CHUNK_VALID) {
			//discard
			in_len = 0;
			return;
		}
		if (in_cmd) {
			if (arg_pos) {
				//non-empty args
				chunk_cmd((char *) input_buf, cmd_len, 1);
			} else {
				chunk_cmd((char *) input_buf, cmd_len, 0);
			}
		} else {
			chunk_data((char *) input_buf, in_len);
		}
		in_len = 0;
		return;
	}

	if (!escape_next && (rxb == 27)) {
		//regardless of chunk type (command or data),
		//strip escape char
		escape_next = 1;
		return;
	}

	if (in_cmd) {
		if (!escape_next && (rxb == '\n')) {
			//0-terminate.
			wait_guardbyte = 1;
			input_buf[in_len] = 0;
			return;
		}
		escape_next = 0;
		//also, tokenize now instead of calling strtok later.
		//Only split args once.
		if (has_args) {
			input_buf[in_len++] = rxb;
			return;
		}
		if (rxb == ' ') {
			cmd_len = in_len;
			//commands of form "++<cmd> <args>": split args on ' '
			input_buf[in_len++] = 0;
			has_args = 1;
			arg_pos = in_len;
		} else {
			input_buf[in_len++] = rxb;
			cmd_len++;
		}

		return;
	}

	// if we made it here, we're dealing with data.
	if (!escape_next && (rxb == '\n')) {
		//terminate.
		wait_guardbyte = 1;
		return;
	}
	input_buf[in_len++] = rxb;
	escape_next = 0;

	return;
}
