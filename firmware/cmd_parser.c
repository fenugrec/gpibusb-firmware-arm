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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libopencm3/stm32/gpio.h>

#include "hw_conf.h"
#include "cmd_parser.h"
#include "firmware.h"
#include "gpib.h"
#include "ring.h"
#include "hw_backend.h"
#include "stypes.h"

const unsigned int version = 5;
#define buf_size 235
u8 cmd_buf[10], buf[buf_size+20];
unsigned int buf_out = 0;
unsigned int buf_in = 0;
int partnerAddress = 1;
int myAddress;
char eos = 10; // Default end of string character.
char eos_string[3] = "";
char eos_code = 3;	//TODO : enum this
char eoiUse = 1; // By default, we are using EOI to signal end of
// msg from instrument
char debug = 0; // enable or disable read&write error messages
u8 strip = 0;
char autoread = 1;
char eot_enable = 1;
char eot_char = 13; // default CR
char listen_only = 0;
char save_cfg = 1;
u8 status_byte = 0;	//XXX name ambiguity with status_byte in serial_poll()
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



/** protos **/
void cmd_poll(void);
void cmd_parser_init(void);

#if 0
char cac(bool read_until_eoi)
{
    char readCharacter,eoiStatus;
    char readBuf[100];
    char i = 0, j=0;
    char errorFound = 0;
    bool reading_done = false;
    char *bufPnt;
    bufPnt = &readBuf[0];
#ifdef VERBOSE_DEBUG
    printf("gpib_read start\n\r");
#endif
    if (mode)
    {
// Command all talkers and listeners to stop
        cmd_buf[0] = CMD_UNT;
        errorFound = errorFound || gpib_cmd(cmd_buf);
        cmd_buf[0] = CMD_UNL;
        errorFound = errorFound || gpib_cmd(cmd_buf);
        if(errorFound)
        {
            return 1;
        }
// Set the controller into listener mode
        cmd_buf[0] = myAddress + 0x20;
        errorFound = errorFound || gpib_cmd(cmd_buf);
        if(errorFound)
        {
            return 1;
        }
// Set target device into talker mode
        cmd_buf[0] = partnerAddress + 0x40;
        errorFound = gpib_cmd(cmd_buf);
        if(errorFound)
        {
            return 1;
        }
    }
    i = 0;
    bufPnt = &readBuf[0];
    /*
    * In this section you will notice that I buffer the received characters,
    * then manually iterate the pointer through the buffer, writing them to
    * UART. If I instead just tried to printf the entire 'string' it would
    * fail. (even if I add a null char at the end). This is because when
    * transfering binary data, some actual data points can be 0x00.
    *
    * The other option of going putc(readBuf[x]);x++; Is for some reason slower
    * than getting a pointer on the first element, then iterating that pointer
    * through the buffer (as I have done here).
    */
#ifdef VERBOSE_DEBUG
    printf("gpib_read loop start\n\r");
#endif
    if(read_until_eoi == 1)
    {
        do
        {
            eoiStatus = gpib_receive(&readCharacter); // eoiStatus is line lvl
            if(eoiStatus==0xff)
            {
                return 1;
            }
            if (eos_code != 0)
            {
                if((readCharacter != eos_string[0]) || (eoiStatus))  // Check for EOM char
                {
                    readBuf[i] = readCharacter; //Copy the read char into the buffer
                    i++;
                }
            }
            else
            {
                if((readCharacter == eos_string[1]) && (eoiStatus == 0))
                {
                    if (readBuf[i-1] == eos_string[0])
                    {
                        i--;
                    }
                }
                else
                {
                    readBuf[i] = readCharacter;
                    i++;
                }
            }
            if(i == 100)
            {
                for(j=0; j<100; ++j)
                {
                    putc(*bufPnt);
                    ++bufPnt;
                }
                i = 0;
                bufPnt = &readBuf[0];
#ifdef WITH_WDT
                restart_wdt();
#endif
            }
        }
        while (eoiStatus);
        for(j=0; j<i-strip; ++j)
        {
            putc(*bufPnt);
            ++bufPnt;
        }
    }
    else
    {
        do
        {
            eoiStatus = gpib_receive(&readCharacter);
            if(eoiStatus==0xff)
            {
                return 1;
            }
            if (eos_code != 0)
            {
                if(readCharacter != eos_string[0])  // Check for EOM char
                {
                    readBuf[i] = readCharacter; //Copy the read char into the buffer
                    i++;
                }
                else
                {
                    reading_done = true;
                }
            }
            else
            {
                if(readCharacter == eos_string[1])
                {
                    if (readBuf[i-1] == eos_string[0])
                    {
                        i--;
                        reading_done = true;
                    }
                }
                else
                {
                    readBuf[i] = readCharacter;
                    i++;
                }
            }
            if(i == 100)
            {
                for(j=0; j<100; ++j)
                {
                    putc(*bufPnt);
                    ++bufPnt;
                }
                i = 0;
                bufPnt = &readBuf[0];
#ifdef WITH_WDT
                restart_wdt();
#endif
            }
        }
        while (reading_done == false);
        reading_done = false;
        for(j=0; j<i-strip; ++j)
        {
            putc(*bufPnt);
            ++bufPnt;
        }
    }
    if (eot_enable == 1)
    {
        printf("%c", eot_char);
    }
#ifdef VERBOSE_DEBUG
    printf("gpib_read loop end\n\r");
#endif
    if (mode)
    {
        errorFound = 0;
// Command all talkers and listeners to stop
        cmd_buf[0] = CMD_UNT;
        errorFound = errorFound || gpib_cmd(cmd_buf);
        cmd_buf[0] = CMD_UNL;
        errorFound = errorFound || gpib_cmd(cmd_buf);
    }
#ifdef VERBOSE_DEBUG
    printf("gpib_read end\n\r");
#endif
    return errorFound;
}

#endif
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
        switch (eos_code)
        {
        case 0:
            eos_code = 0;
            eos_string[0] = 13;
            eos_string[1] = 10;
            eos_string[2] = 0x00;
            eos = 10;
            break;
        case 1:
            eos_code = 1;
            eos_string[0] = 13;
            eos_string[1] = 0x00;
            eos = 13;
            break;
        case 2:
            eos_code = 2;
            eos_string[0] = 10;
            eos_string[1] = 0x00;
            eos = 10;
            break;
        default:
            eos_code = 3;
            eos_string[0] = 0x00;
            eos = 0;
            break;
        }
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
        write_eeprom(0x05, 3); // eos_code
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

/** parse command chunk
 *
 * @param cmd 0-terminated string that includes command word and arguments
 */
static void cmd_parser(char *cmd) {
	char *buf_pnt = cmd;
    char writeError = 0;

#ifdef WITH_WDT
	restart_wdt();
#endif
	if(*buf_pnt == '+')   // Controller commands start with a +
	{
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
			if(gpib_read(eoiUse, eos_code, eot_enable, eot_char))
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
				gpib_read(false, eos_code, eot_enable, eot_char); // read until EOS condition
			}
			else if (*(buf_pnt+7) == 101)
			{
				gpib_read(true, eos_code, eot_enable, eot_char); // read until EOI flagged
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
			eos_code = 4;
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
				switch (eos_code)
				{
				case 0:
					eos_code = 0;
					eos_string[0] = 13;
					eos_string[1] = 10;
					eos_string[2] = 0x00;
					eos = 10;
					break;
				case 1:
					eos_code = 1;
					eos_string[0] = 13;
					eos_string[1] = 0x00;
					eos = 13;
					break;
				case 2:
					eos_code = 2;
					eos_string[0] = 10;
					eos_string[1] = 0x00;
					eos = 10;
					break;
				default:
					eos_code = 3;
					eos_string[0] = 0x00;
					eos = 0;
					break;
				}
			}
		}
// +eoi:{0|1}
		else if(strncmp((char*)buf_pnt,(char*)eoiBuf,5)==0)
		{
			eoiUse = atoi((char*)(buf_pnt+5)); // Parse out the end of string byte
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
				eoiUse = atoi((char*)(buf_pnt+6));
			}
		}
// +strip:{0|1}
		else if(strncmp((char*)buf_pnt,(char*)stripBuf,7)==0)
		{
			strip = atoi((char*)(buf_pnt+7)); // Parse out the end of string byte
		}
// +ver
		else if(strncmp((char*)buf_pnt,(char*)versionBuf,4)==0)
		{
			printf("%i%c", version, eot_char);
		}
// ++ver
		else if(strncmp((char*)buf_pnt+1,(char*)versionBuf,4)==0)
		{
			printf("Version %i.0%c", version, eot_char);
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
			autoread = atoi((char*)(buf_pnt+10));
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
				autoread = atoi((char*)(buf_pnt+7));
				if ((autoread != 0) && (autoread != 1))
				{
					autoread = 1; // If non-bool sent, set to enable
				}
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
			debug = atoi((char*)(buf_pnt+7));
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
				debug = atoi((char*)(buf_pnt+8));
				if ((debug != 0) && (debug != 1))
				{
					debug = 0; // If non-bool sent, set to disabled
				}
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
				eot_enable = atoi((char*)(buf_pnt+13));
				if ((eot_enable != 0) && (eot_enable != 1))
				{
					eot_enable = 1; // If non-bool sent, set to enable
				}
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
				listen_only = atoi((char*)(buf_pnt+6));
				if ((listen_only != 0) && (listen_only != 1))
				{
					listen_only = 0; // If non-bool sent, set to disable
				}
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
				mode = atoi((char*)(buf_pnt+7));
				if ((mode != 0) && (mode != 1))
				{
					mode = 1; // If non-bool sent, set to control mode
				}
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
				save_cfg = atoi((char*)(buf_pnt+10));
				if ((save_cfg != 0) && (save_cfg != 1))
				{
					save_cfg = 1; // If non-bool sent, set to enable
				}
				if (save_cfg == 1)
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
	else
	{
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
			if(eos_code != 3)   // If have an EOS char, need to output
			{
// termination byte to inst
				writeError = writeError || gpib_write((u8 *)buf_pnt, 0, 0);
				if (!writeError)
					writeError = gpib_write((u8 *) eos_string, 0, eoiUse);
#ifdef VERBOSE_DEBUG
				printf("eos_string: %s",eos_string);
#endif
			}
			else
			{
				writeError = writeError || gpib_write((u8 *)buf_pnt, 0, 1);
			}
		}
// If cmd contains a question mark -> is a query
		if(autoread && mode)
		{
			if ((strchr((char*)buf_pnt, '?') != NULL) && !(writeError))
			{
				gpib_read(eoiUse, eos_code, eot_enable, eot_char);
			}
			else if(writeError)
			{
				writeError = 0;
			}
		}
	} // end of sending internal command
}

/** device mode poll */
static void device_poll(void) {
	// When in device mode we should be checking the status of the
	// ATN line to see what we should be doing
	if (!gpio_get(CONTROL_PORT, ATN))
	{
		if (!gpio_get(CONTROL_PORT, ATN))
		{
			gpio_clear(CONTROL_PORT, NDAC);
			gpib_receive(cmd_buf); // Get the CMD byte sent by the controller
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
				gpib_read(eoiUse, eos_code, eot_enable, eot_char);
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

/** wait for command inputs, or GPIB traffic if in device mode */
void cmd_poll(void) {
	if (!mode) {
		device_poll();
	}

}
