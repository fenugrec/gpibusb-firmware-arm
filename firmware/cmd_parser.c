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

#include "printf_config.h"  //hax, just to get PRINTF_ALIAS_STANDARD_FUNCTION_NAMES...
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
#define MAX_TIMEOUT		  10*1000 //10 seconds, is there any reason to allow more than this

#define VERSION 5

char eos_string[3] = "";
unsigned eos_len = 0;
bool strip = 0;
bool listen_only = 0;
bool save_cfg = 1;

u8 status_byte = 0;

static void set_eos(enum eos_codes newcode) {
	gpib_cfg.eos_code = newcode;
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
	return !((bool)gpio_get(HCTRL2_CP, SRQ));
}


void cmd_parser_init(void) {
	// Handle the EEPROM stuff
	if (read_eeprom(0x00) == VALID_EEPROM_CODE)
	{
		gpib_cfg.controller_mode = read_eeprom(0x01);
		gpib_cfg.partnerAddress = read_eeprom(0x02);
		gpib_cfg.eot_char = read_eeprom(0x03);
		gpib_cfg.eot_enable = read_eeprom(0x04);
		gpib_cfg.eos_code = read_eeprom(0x05);
		set_eos(gpib_cfg.eos_code);
		gpib_cfg.eoiUse = read_eeprom(0x06);
		gpib_cfg.autoread = read_eeprom(0x07);
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
	if (gpib_cfg.controller_mode)
	{
		gpib_controller_assign();
	}

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
		printf("%i\n", gpib_cfg.partnerAddress);
	} else {
		gpib_cfg.partnerAddress = atoi(args);
	}
}
void do_readTimeout(const char *args) {
	// ++read_tmo_ms N
	if (*args == 0) {
		printf("%lu\n", (unsigned long) gpib_cfg.timeout);
		return;
	}
	u32 temp = (u32) atoi(args);

	/* bounds-check timeout value before saving */
	if (temp > MAX_TIMEOUT) temp=MAX_TIMEOUT;
	gpib_cfg.timeout = temp;
}
void do_readCmd2(const char *args) {
	// ++read [eoi|<char>]
	//XXX TODO : err msg when read error occurs
	if (!gpib_cfg.controller_mode) return;
	if (*args == 0) {
		gpib_read(GPIBREAD_TMO,0, gpib_cfg.eot_enable); // read until EOS condition
	} else if (strncmp(args, "eoi", 3) == 0) {
		gpib_read(GPIBREAD_EOI, 0, gpib_cfg.eot_enable); // read until EOI flagged
	} else {
		// read until specified character
		u8 tmp_eos = htoi(args);
		gpib_read(GPIBREAD_EOS, tmp_eos, gpib_cfg.eot_enable);
	}
}
void do_eos2(const char *args) {
	// ++eos {0|1|2|3}
	if (*args == 0) {
		printf("%i\n", gpib_cfg.eos_code);
	} else {
		gpib_cfg.eos_code = atoi(args);
		set_eos(gpib_cfg.eos_code);
	}
}
void do_eoi(const char *args) {
	// ++eoi {0|1}
	if (*args == 0) {
		printf("%i\n", gpib_cfg.eoiUse);
	} else {
		gpib_cfg.eoiUse = (bool) atoi(args);
	}
}
void do_strip(const char *args) {
	// ++strip {0|1}
	strip = (bool) atoi(args);
}
void do_version2(const char *args) {
	// ++ver
	(void) args;
	printf("Version %i.0\n", VERSION);
}
void do_trg(const char *args) {
	// ++trg [<PAD1> [<SAD1>] <PAD2> [SAD2] … <PAD15> [<SAD15>]]
	u8 writeError = 0;
	(void) args;
	if (!gpib_cfg.controller_mode) return;
	if (*args == 0) {
		writeError = writeError || gpib_address_target(gpib_cfg.partnerAddress);
		//XXX TODO : do something with writeError
		gpib_cmd(CMD_GET);
	} else {
		//TODO: Add support for specified addresses
	}
}
void do_autoRead(const char *args) {
	// ++auto {0|1}
	if (*args == 0) {
		printf("%i\n", gpib_cfg.autoread);
	} else {
		gpib_cfg.autoread = (bool) atoi(args);
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
		const char *pfx = gpib_cfg.debug? "en":"dis";
		printf("%sabled\n", pfx);
		sys_printstats();
	} else {
		gpib_cfg.debug = (bool) atoi(args);
	}
}
void do_clr(const char *args) {
	// ++clr
	u8 writeError = 0;
	(void) args;
	if (!gpib_cfg.controller_mode) return;
	//XXX TODO : do something with writeError
	// This command is special in that we must
	// address a specific instrument.
	writeError = writeError || gpib_address_target(gpib_cfg.partnerAddress);
	writeError = writeError || gpib_cmd(CMD_SDC);
}
void do_eotEnable(const char *args) {
	// ++eot_enable {0|1}
	if (*args == 0) {
		printf("%i\n", gpib_cfg.eot_enable);
	} else {
		gpib_cfg.eot_enable = (bool) atoi(args);
	}
}
void do_eotChar(const char *args) {
	// ++eot_char N
	if (*args == 0) {
		printf("%i\n", gpib_cfg.eot_char);
	} else {
		gpib_cfg.eot_char = atoi(args);
	}
}
void do_ifc(const char *args) {
	// ++ifc
	(void) args;
	if (!gpib_cfg.controller_mode) return;
	pulse_ifc();
}
void do_llo(const char *args) {
	// ++llo
	u8 writeError = 0;
	(void) args;
	//XXX TODO : do something with writeError
	if (!gpib_cfg.controller_mode) return;
	writeError = writeError || gpib_address_target(gpib_cfg.partnerAddress);
	writeError = writeError || gpib_cmd(CMD_LLO);
}
void do_loc(const char *args) {
	// ++loc
	u8 writeError = 0;
	(void) args;
	//XXX TODO : do something with writeError
	if (!gpib_cfg.controller_mode) return;

	writeError = writeError || gpib_address_target(gpib_cfg.partnerAddress);
	writeError = writeError || gpib_cmd(CMD_GTL);
}
void do_lon(const char *args) {
	// ++lon {0|1}
	if (*args == 0) {
		printf("%i\n", listen_only);
	} else {
		if (gpib_cfg.controller_mode) { return; }
		listen_only = (bool) atoi(args);
	}
}
void do_mode(const char *args) {
	// ++mode {0|1}
	if (*args == 0) {
		printf("%i\n", gpib_cfg.controller_mode);
		return;
	}
	gpib_cfg.controller_mode = (bool) atoi(args);
	if (gpib_cfg.controller_mode) {
		setControls(CINI);
		gpib_controller_assign();
	} else {
		setControls(DINI);
	}
}
void do_savecfg(const char *args) {
	// ++savecfg {0|1}
	if (*args == 0) {
		printf("%i\n", save_cfg);
	} else {
		save_cfg = (bool) atoi(args);
		if (save_cfg) {
			write_eeprom(0x01, gpib_cfg.controller_mode);
			write_eeprom(0x02, gpib_cfg.partnerAddress);
			write_eeprom(0x03, gpib_cfg.eot_char);
			write_eeprom(0x04, gpib_cfg.eot_enable);
			write_eeprom(0x05, gpib_cfg.eos_code);
			write_eeprom(0x06, gpib_cfg.eoiUse);
			write_eeprom(0x07, gpib_cfg.autoread);
			write_eeprom(0x08, gpib_cfg.listen_only);
			write_eeprom(0x09, save_cfg);
		}
	}
}
void do_srq(const char *args) {
	// ++srq
	(void) args;
	if (!gpib_cfg.controller_mode) return;
	printf("%i\n", srq_state());
}
void do_spoll(const char *args) {
	// ++spoll [pad [sad]]
	// TODO : support secodnary addr too
	if (!gpib_cfg.controller_mode) return;
	if (*args == 0) {
		if (!gpib_serial_poll(gpib_cfg.partnerAddress, &status_byte)) {
			printf("%u\n", (unsigned) status_byte);
		}
	} else {
		if (!gpib_serial_poll(atoi(args), &status_byte)) {
			printf("%u\n", (unsigned) status_byte);
		}
	}
}
void do_status(const char *args) {
	// ++status [n]
	if (gpib_cfg.controller_mode) return;
	if (*args == 0) {
		printf("%u\n", (unsigned) status_byte);
	} else {
		status_byte = (u8) atoi(args);
		if (status_byte & 0x40) {
			// prologix: " If the RQS bit (bit #6) of the status byte is set then the SRQ signal is asserted (low)
			// After a serial poll, SRQ line is de-asserted and status byte is set to 0 "
			assert_signal(HCTRL2_CP, SRQ);
		}
	}
}

/** helper to print list of command names */
static void print_cmd(const struct cmd_entry *cmd) {
	printf("%s %s\n", cmd->name, cmd->helpstring);
	return;
}

void do_help(const char *args) {
	printf("some commands and functions not implemented.\n");
	cmd_walk_cmdlist(print_cmd);
	sys_printstats();
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
		cmd_find_run(cmd, cmd_len, &cmd[cmd_len + 0]);  //trailing 0 of command
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
	if (gpib_cfg.controller_mode)
	{
		writeError = writeError || gpib_address_target(gpib_cfg.partnerAddress);
		// Set the controller into talker mode
		u8 cmd = gpib_cfg.myAddress + CMD_TAD;
		writeError = writeError || gpib_cmd(cmd);
	}
	// Send out command to the bus
	DEBUG_PRINTF("gpib_write: %.*s\n", len, buf_pnt);

	if (gpib_cfg.controller_mode || gpib_cfg.device_talk)
	{
		if (gpib_cfg.eos_code != EOS_NUL)   // If have an EOS char, need to output
		{
			// termination byte to inst
			writeError = writeError || gpib_write((u8 *)buf_pnt, len, 0);
			if (!writeError) {
				DEBUG_PRINTF("gpib_write eos[%u] (%02X...)", eos_len, eos_string[0]);
				writeError = gpib_write((u8 *) eos_string, eos_len, gpib_cfg.eoiUse);
			}

		}
		else
		{
			writeError = writeError || gpib_write((u8 *)buf_pnt, len, 1);
		}
	}

	if (gpib_cfg.autoread && gpib_cfg.controller_mode) {
		//XXX TODO : with autoread, does prologix terminate by EOI ?
		writeError = writeError || gpib_read(GPIBREAD_EOI, 0, gpib_cfg.eot_enable);
	}
}


static void listenonly(void) {
	dio_float();
	output_setmodes(TM_RECV);
	gpio_clear(FLOW_PORT, TE);

	assert_signal(HCTRL1_CP, NRFD);

	// if DAV=1 , not valid : return
	if (gpio_get(HCTRL1_CP, DAV)) {
		return;
	}
	bool atn_state = !gpio_get(HCTRL2_CP, ATN);

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

	if (!gpio_get(HCTRL2_CP, IFC)) {
		gpib_cfg.device_talk = 0;
		gpib_cfg.device_listen = 0;
		gpib_cfg.device_srq = 0;
		status_byte = 0;
		return;
	}

	if (!gpio_get(HCTRL2_CP, ATN)) {
		device_atn();
	} else {
		device_noatn();
	}
}

/** device poll with ATN asserted (==0) */
static void device_atn(void) {
	bool eoi_status;
	dio_float();
	setControls(DLAS);

	// Get the CMD byte sent by the controller
	u8 rxb;
	enum errcodes rv = gpib_read_byte(&rxb, &eoi_status);
	setControls(DIDS);
	if (rv) {
		//error (timeout ?)
		return;
	}
	if ((rxb & 0xE0) == CMD_TAD) {
		if (rxb == gpib_cfg.partnerAddress + CMD_TAD) {
			gpib_cfg.device_talk = true;
			gpib_cfg.device_listen = 0;
			DEBUG_PRINTF("Instructed to talk\n");
		} else {
			//somebody else is addressed to talk
			gpib_cfg.device_talk = 0;
		}
	} else if (rxb == gpib_cfg.partnerAddress + CMD_LAD) {
		gpib_cfg.device_talk = 0;
		gpib_cfg.device_listen = true;
		DEBUG_PRINTF("Instructed to listen\n");
	} else if (rxb == CMD_UNL) {
		gpib_cfg.device_listen = false;
		DEBUG_PRINTF("Instructed to stop listen\n");
	} else if (rxb == CMD_UNT) {
		gpib_cfg.device_talk = false;
		DEBUG_PRINTF("Instructed to stop talk\n");
	} else if (rxb == CMD_SPE) {
		gpib_cfg.device_srq = true;
		DEBUG_PRINTF("SQR start\n");
	} else if (rxb == CMD_SPD) {
		gpib_cfg.device_srq = false;
		DEBUG_PRINTF("SQR end\n");
	} else if (rxb == CMD_DCL) {
		DEBUG_PRINTF("DCL\n");
		gpib_cfg.device_listen = false;
		gpib_cfg.device_talk = false;
		gpib_cfg.device_srq = false;
		status_byte = 0;
	} else if ((rxb == CMD_LLO) && (gpib_cfg.device_listen)) {
		DEBUG_PRINTF("LLO\n");
	} else if ((rxb == CMD_GTL) && (gpib_cfg.device_listen)) {
		DEBUG_PRINTF("GTL\n");
	} else if ((rxb == CMD_GET) && (gpib_cfg.device_listen)) {
		DEBUG_PRINTF("GET\n");
	} else if ((rxb == CMD_SDC) && (gpib_cfg.device_listen)) {
		DEBUG_PRINTF("SDC\n");
		gpib_cfg.device_listen = false;
		gpib_cfg.device_talk = false;
		gpib_cfg.device_srq = false;
		status_byte = 0;
	}
}

/** device poll with ATN not asserted (==1) */
static void device_noatn(void) {
	if (gpib_cfg.device_listen) {
		dio_float();
		setControls(DLAS);

		DEBUG_PRINTF("device mode gpib_read\n");
		u8 rxb;
		bool eoi_status;
		gpib_read_byte(&rxb, &eoi_status);
		setControls(DIDS);
		DEBUG_PRINTF("devpoll noATN got %u", (unsigned) rxb);
	} else if (gpib_cfg.device_talk) {
		setControls(DTAS);
		if (gpib_cfg.device_srq) {
			gpib_write(&status_byte, 1, 0);
			unassert_signal(HCTRL2_CP, SRQ);
			gpib_cfg.device_srq = false;
			status_byte = 0;
		}
		setControls(DIDS);
	}
}


void cmd_poll(void) {
	u8 rxb;
	static u8 input_buf[HOST_IN_BUFSIZE];
	static unsigned in_len = 0;
	static unsigned cmd_len = 0;    //length of command token, excluding 0 termination.
	static unsigned arg_pos = 0;
	static bool in_cmd = 0;
	static bool escape_next = 0;
	static bool wait_guardbyte = 0;
	static bool has_args = 0;

	restart_wdt();

	if (listen_only) {
		listenonly();
	} else if (!gpib_cfg.controller_mode) {
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
