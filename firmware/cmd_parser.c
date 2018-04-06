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

    //TODO : this is just *asking* to be hashtable'd !
// Original Command Set
static const char addressBuf[4] = "+a:";
static const char timeoutBuf[4] = "+t:";
static const char eosBuf[6] = "+eos:";
static const char eoiBuf[6] = "+eoi:";
static const char testBuf[6] = "+test";
static const char readCmdBuf[6] = "+read";
static const char getCmdBuf[5] = "+get";
static const char stripBuf[8] = "+strip:";
static const char versionBuf[5] = "+ver";
static const char autoReadBuf[11] = "+autoread:";
static const char resetBuf[7] = "+reset";
static const char debugBuf[8] = "+debug:";
// Prologix Compatible Command Set
static const char addrBuf[7] = "++addr";
static const char autoBuf[7] = "++auto";
static const char clrBuf[6] = "++clr";
static const char eotEnableBuf[13] = "++eot_enable";
static const char eotCharBuf[11] = "++eot_char";
static const char ifcBuf[6] = "++ifc";
static const char lloBuf[6] = "++llo";
static const char locBuf[6] = "++loc";
static const char lonBuf[6] = "++lon"; //TODO: Listen mode
static const char modeBuf[7] = "++mode";
static const char readTimeoutBuf[14] = "++read_tmo_ms";
static const char rstBuf[6] = "++rst";
static const char savecfgBuf[10] = "++savecfg";
static const char spollBuf[8] = "++spoll";
static const char srqBuf[6] = "++srq";
static const char statusBuf[9] = "++status";
static const char trgBuf[6] = "++trg";
//static const char verBuf[6] = "++ver";	//matched with "+ver" string
//static const char helpBuf[7] = "++help"; //TODO

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

/** Parse command
 *
 * @param cmd 0-terminated command (starts with '+' or "++")
 */
static void chunk_cmd(char *cmd, unsigned len) {
	char *buf_pnt = cmd;
    char writeError = 0;
// +a:N
	if(strncmp((char*)buf_pnt,(char*)addressBuf,3)==0)
	{
		partnerAddress = atoi((char*)(buf_pnt+3)); // Parse out the GPIB address
	}
// ++addr N
	else if(strncmp((char*)buf_pnt,(char*)addrBuf,6)==0)
	{
		if (*(buf_pnt+6) == 0x00)
		{
			printf("%i%c", partnerAddress, eot_char);
		}
		else if (*(buf_pnt+6) == 32)
		{
			partnerAddress = atoi((char*)(buf_pnt+7));
		}
	}
// +t:N
	else if(strncmp((char*)buf_pnt,(char*)timeoutBuf,3)==0)
	{
		timeout = (u32) atoi((char*)(buf_pnt+3)); // Parse out the timeout period
	}
// ++read_tmo_ms N
	else if(strncmp((char*)buf_pnt,(char*)readTimeoutBuf,13)==0)
	{
		if (*(buf_pnt+13) == 0x00)
		{
			printf("%lu%c", (unsigned long) timeout, eot_char);
		}
		else if (*(buf_pnt+13) == 32)
		{
			timeout = (u32) atoi((char*)(buf_pnt+14));
		}
	}
// +read
	else if((strncmp((char*)buf_pnt,(char*)readCmdBuf,5)==0) && (mode))
	{
		if(gpib_read(eoiUse, eos_code, eos_string, eot_enable, eot_char))
		{
			if (debug == 1)
			{
				printf("Read error occured.%c", eot_char);
			}
//delay_ms(1);
//reset_cpu();
		}
	}
// ++read
	else if((strncmp((char*)buf_pnt+1,(char*)readCmdBuf,5)==0) && (mode))
	{
		if (*(buf_pnt+6) == 0x00)
		{
			gpib_read(false, eos_code, eos_string, eot_enable, eot_char); // read until EOS condition
		}
		else if (*(buf_pnt+7) == 101)
		{
			gpib_read(true, eos_code, eos_string, eot_enable, eot_char); // read until EOI flagged
		}
		/*else if (*(buf_pnt+6) == 32) {
		// read until specified character
		}*/
	}
// +test
	else if(strncmp((char*)buf_pnt,(char*)testBuf,5)==0)
	{
		printf("testing%c", eot_char);
	}
// +eos:N
	else if(strncmp((char*)buf_pnt,(char*)eosBuf,5)==0)
	{
		eos = atoi((char*)(buf_pnt+5)); // Parse out the end of string byte
		eos_string[0] = eos;
		eos_string[1] = 0x00;
		eos_code = EOS_CUSTOM;
	}
// ++eos {0|1|2|3}
	else if(strncmp((char*)buf_pnt+1,(char*)eosBuf,4)==0)
	{
		if (*(buf_pnt+5) == 0x00)
		{
			printf("%i%c", eos_code, eot_char);
		}
		else if (*(buf_pnt+5) == 32)
		{
			eos_code = atoi((char*)(buf_pnt+6));
			set_eos(eos_code);
		}
	}
// +eoi:{0|1}
	else if(strncmp((char*)buf_pnt,(char*)eoiBuf,5)==0)
	{
		eoiUse = (bool) atoi((char*)(buf_pnt+5)); // Parse out the end of string byte
	}
// ++eoi {0|1}
	else if(strncmp((char*)buf_pnt+1,(char*)eoiBuf,4)==0)
	{
		if (*(buf_pnt+5) == 0x00)
		{
			printf("%i%c", eoiUse, eot_char);
		}
		else if (*(buf_pnt+5) == 32)
		{
			eoiUse = (bool) atoi((char*)(buf_pnt+6));
		}
	}
// +strip:{0|1}
	else if(strncmp((char*)buf_pnt,(char*)stripBuf,7)==0)
	{
		strip = (bool) atoi((char*)(buf_pnt+7)); // Parse out the end of string byte
	}
// +ver
	else if(strncmp((char*)buf_pnt,(char*)versionBuf,4)==0)
	{
		printf("%i%c", VERSION, eot_char);
	}
// ++ver
	else if(strncmp((char*)buf_pnt+1,(char*)versionBuf,4)==0)
	{
		printf("Version %i.0%c", VERSION, eot_char);
	}
// +get
	else if((strncmp((char*)buf_pnt,(char*)getCmdBuf,4)==0) && (mode))
	{
		if (*(buf_pnt+5) == 0x00)
		{
			writeError = writeError || gpib_address_target(partnerAddress);
			cmd_buf[0] = CMD_GET;
			gpib_cmd(cmd_buf);
		}
		/*else if (*(buf_pnt+5) == 32) {
		TODO: Add support for specified addresses
		}*/
	}
// ++trg
	else if((strncmp((char*)buf_pnt,(char*)trgBuf,5)==0) && (mode))
	{
		if (*(buf_pnt+5) == 0x00)
		{
			writeError = writeError || gpib_address_target(partnerAddress);
			cmd_buf[0] = CMD_GET;
			gpib_cmd(cmd_buf);
		}
		/*else if (*(buf_pnt+5) == 32) {
		TODO: Add support for specified addresses
		}*/
	}
// +autoread:{0|1}
	else if(strncmp((char*)buf_pnt,(char*)autoReadBuf,10)==0)
	{
		autoread = (bool) atoi((char*)(buf_pnt+10));
	}
// ++auto {0|1}
	else if(strncmp((char*)buf_pnt,(char*)autoBuf,6)==0)
	{
		if (*(buf_pnt+6) == 0x00)
		{
			printf("%i%c", autoread, eot_char);
		}
		else if (*(buf_pnt+6) == 32)
		{
			autoread = (bool) atoi((char*)(buf_pnt+7));
		}
	}
// +reset
	else if(strncmp((char*)buf_pnt,(char*)resetBuf,6)==0)
	{
		delay_ms(1);
		reset_cpu();
	}
// ++rst
	else if(strncmp((char*)buf_pnt,(char*)rstBuf,5)==0)
	{
		delay_ms(1);
		reset_cpu();
	}
// +debug:{0|1}
	else if(strncmp((char*)buf_pnt,(char*)debugBuf,7)==0)
	{
		debug = (bool) atoi((char*)(buf_pnt+7));
	}
// ++debug {0|1}
	else if(strncmp((char*)buf_pnt+1,(char*)debugBuf,6)==0)
	{
		if (*(buf_pnt+7) == 0x00)
		{
			printf("%i%c", debug, eot_char);
		}
		else if (*(buf_pnt+7) == 32)
		{
			debug = (bool) atoi((char*)(buf_pnt+8));
		}
	}
// ++clr
	else if((strncmp((char*)buf_pnt,(char*)clrBuf,5)==0) && (mode))
	{
// This command is special in that we must
// address a specific instrument.
		writeError = writeError || gpib_address_target(partnerAddress);
		cmd_buf[0] = CMD_SDC;
		writeError = writeError || gpib_cmd(cmd_buf);
	}
// ++eot_enable {0|1}
	else if(strncmp((char*)buf_pnt,(char*)eotEnableBuf,12)==0)
	{
		if (*(buf_pnt+12) == 0x00)
		{
			printf("%i%c", eot_enable, eot_char);
		}
		else if (*(buf_pnt+12) == 32)
		{
			eot_enable = (bool) atoi((char*)(buf_pnt+13));
		}
	}
// ++eot_char N
	else if(strncmp((char*)buf_pnt,(char*)eotCharBuf,10)==0)
	{
		if (*(buf_pnt+10) == 0x00)
		{
			printf("%i%c", eot_char, eot_char);
		}
		else if (*(buf_pnt+10) == 32)
		{
			eot_char = atoi((char*)(buf_pnt+11));
		}
	}
// ++ifc
	else if((strncmp((char*)buf_pnt,(char*)ifcBuf,5)==0) && (mode))
	{
		gpio_clear(CONTROL_PORT, IFC);
		delay_ms(200);
		gpio_set(CONTROL_PORT, IFC);
	}
// ++llo
	else if((strncmp((char*)buf_pnt,(char*)lloBuf,5)==0) && (mode))
	{
		writeError = writeError || gpib_address_target(partnerAddress);
		cmd_buf[0] = CMD_LLO;
		writeError = writeError || gpib_cmd(cmd_buf);
	}
// ++loc
	else if((strncmp((char*)buf_pnt,(char*)locBuf,5)==0) && (mode))
	{
		writeError = writeError || gpib_address_target(partnerAddress);
		cmd_buf[0] = CMD_GTL;
		writeError = writeError || gpib_cmd(cmd_buf);
	}
// ++lon {0|1}
	else if((strncmp((char*)buf_pnt,(char*)lonBuf,5)==0) && (!mode))
	{
		if (*(buf_pnt+5) == 0x00)
		{
			printf("%i%c", listen_only, eot_char);
		}
		else if (*(buf_pnt+5) == 32)
		{
			listen_only = (bool) atoi((char*)(buf_pnt+6));
		}
	}
// ++mode {0|1}
	else if(strncmp((char*)buf_pnt,(char*)modeBuf,6)==0)
	{
		if (*(buf_pnt+6) == 0x00)
		{
			printf("%i%c", mode, eot_char);
		}
		else if (*(buf_pnt+6) == 32)
		{
			mode = (bool) atoi((char*)(buf_pnt+7));
			prep_gpib_pins(mode);
			if (mode)
			{
				gpib_controller_assign();
			}
		}
	}
// ++savecfg {0|1}
	else if(strncmp((char*)buf_pnt,(char*)savecfgBuf,9)==0)
	{
		if (*(buf_pnt+9) == 0x00)
		{
			printf("%i%c", save_cfg, eot_char);
		}
		else if (*(buf_pnt+9) == 32)
		{
			save_cfg = (bool) atoi((char*)(buf_pnt+10));
			if (save_cfg)
			{
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
// ++srq
	else if((strncmp((char*)buf_pnt,(char*)srqBuf,5)==0) && (mode))
	{
		printf("%i%c", srq_state(), eot_char);
	}
// ++spoll N
	else if((strncmp((char*)buf_pnt,(char*)spollBuf,7)==0) && (mode))
	{
		if (*(buf_pnt+7) == 0x00)
		{
			if (!gpib_serial_poll(partnerAddress, &status_byte)) {
				printf("%u%c", (unsigned) status_byte, eot_char);
			}
		}
		else if (*(buf_pnt+7) == 32)
		{
			if (!gpib_serial_poll(atoi((char*)(buf_pnt+8)), &status_byte)) {
				printf("%u%c", (unsigned) status_byte, eot_char);
			}
		}
	}
// ++status
	else if((strncmp((char*)buf_pnt,(char*)statusBuf,8)==0) && (!mode))
	{
		if (*(buf_pnt+8) == 0x00)
		{
			printf("%u%c", (unsigned) status_byte, eot_char);
		}
		else if (*(buf_pnt+8) == 32)
		{
			status_byte = (u8) atoi((char*)(buf_pnt+9));;
		}
	}
	else
	{
		if (debug == 1)
		{
			printf("Unrecognized command.%c", eot_char);
		}
	}
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
			in_cmd = 1;
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
				chunk_cmd((char *) input_buf, in_len);
			} else {
				chunk_data((char *) input_buf, in_len);
			}
			in_len = 0;
			continue;
		}

		if (in_cmd) {
			// For now, assume escaping is illegal within a command
			if ((rxb == '\r') || (rxb == '\n')) {
				//0-terminate.
				wait_guardbyte = 1;
				input_buf[in_len] = 0;
				continue;
			}
			input_buf[in_len++] = rxb;
			continue;
		}
		// if we made it here, we're dealing with data.
		if (!escape_next && (rxb == 27)) {
			escape_next = 1;
			continue;
		}
		escape_next = 0;
		if ((rxb == '\r') || (rxb == '\n')) {
			//terminate.
			wait_guardbyte = 1;
			continue;
		}
		input_buf[in_len++] = rxb;
	}	//while 1
}
