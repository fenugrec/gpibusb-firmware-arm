/*
* GPIBUSB Adapter
* gpib.c
**
* © 2014 Steven Casagrande (scasagrande@galvant.ca).
* (c) 2018 fenugrec
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

#include <string.h>
#include <libopencm3/stm32/gpio.h>
#include "firmware.h"
#include "gpib.h"
#include "hw_conf.h"
#include "host_comms.h"
#include "hw_backend.h"
#include "stypes.h"

/* global vars */
bool mode = 1;
bool debug = 0; // enable or disable read&write error messages
char eot_char = '\r'; // default CR
uint32_t timeout = 1000;
// Variables for device mode
bool device_talk = false;
bool device_listen = false;
bool device_srq = false;


/* Some forward decls that don't need to be in the public gpib.h
*/
static uint32_t _gpib_write(uint8_t *bytes, uint32_t length, bool atn, bool use_eoi);


/** Write a GPIB command byte
*
* This will assert the GPIB ATN line.
*
* See _gpib_write for parameter information
*/
uint32_t gpib_cmd(uint8_t *bytes) {
	return _gpib_write(bytes, 1, 1, 0);
}

/** Write a GPIB data string to the GPIB bus.
*
* See _gpib_write for parameter information
*/
uint32_t gpib_write(uint8_t *bytes, uint32_t length, bool use_eoi) {
    return _gpib_write(bytes, length, 0, use_eoi);
}

/** Write a string of bytes to the GPIB bus
*
* bytes: Array of bytes to be written to the bus
* length: number of bytes to write to the bus. Leave 0 if not known and
*   this will then be computed based on C-string length
* atn: Whether the GPIB ATN line should be asserted or not. Set to 1 if byte
	is a GPIB command byte or 0 otherwise.
* use_eoi: Set whether the GPIB EOI line should be asserted or not on
	transmission of the last byte
*
* Returns 0 if everything went fine, or 1 if there was an error
*/
static uint32_t _gpib_write(uint8_t *bytes, uint32_t length, bool atn, bool use_eoi) {
    uint8_t byte; // Storage variable for the current character
    uint32_t i;

    // TODO: Set pin modes to output as required for writing

    gpio_set(FLOW_PORT, PE); // Enable power on the bus driver ICs

    if(atn) { gpio_clear(ATN_CP, ATN); }
    if(!length) { length = strlen((char*)bytes); }

	gpio_mode_setup(NRFD_CP, GPIO_MODE_INPUT, GPIO_PUPD_NONE, NRFD | NDAC);
    gpio_set(FLOW_PORT, TE); // Enable talking
    gpio_set(EOI_CP, EOI);
    gpio_set(DAV_CP, DAV);

    // Before we start transfering, we have to make sure that NRFD is high
    // and NDAC is low
    // TODO: timeouts here
    while(gpio_get(NDAC_CP, NDAC) || !gpio_get(NRFD_CP, NRFD)){}

    // Loop through each byte and write it to the GPIB bus
    for(i=0;i<length;i++) {
        byte = bytes[i];

        // TODO: debug message printf("Writing byte: %c %x %c", a, a, eot_char);

        // Wait for NDAC to go low, indicating previous bit is now done with
        // TODO: timeouts here
        while(gpio_get(NDAC_CP, NDAC)){}

        // Put the byte on the data lines
        WRITE_DIO(byte);

        // Assert EOI if on last byte and using EOI
        if((i==length-1) && (use_eoi)) {gpio_clear(EOI_CP, EOI);}

        // Wait for NRFD to go high, indicating listeners are ready for data
        // TODO: timeouts here
        while(!gpio_get(NRFD_CP, NRFD)){}

        // Assert DAV, informing listeners that the data is ready to be read
        gpio_clear(DAV_CP, DAV);

        // Wait for NDAC to go high, all listeners have accepted the byte
        // TODO: timeouts here
        while(!gpio_get(NDAC_CP, NDAC)){}

        // Release DAV, indicating byte is no longer valid
        gpio_set(DAV_CP, DAV);
    } // Finished outputting all bytes to the listeners

    gpio_mode_setup(DIO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, 0xFF);
    gpio_clear(FLOW_PORT, TE); // Disable talking

    // If the byte was a GPIB command byte, release ATN line
    if(atn) { gpio_set(ATN_CP, ATN); }

	gpio_set(NDAC_CP, NDAC | NRFD);
    gpio_mode_setup(EOI_CP, GPIO_MODE_INPUT, GPIO_PUPD_NONE, DAV | EOI);
    gpio_mode_setup(NRFD_CP, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, NRFD | NDAC);
    gpio_clear(FLOW_PORT, PE);

    return 0;
}

/** Receive a single byte from the GPIB bus
* Assumes ports were setup properly
*
* byte: Pointer to where the received byte will be stored
* eoi_status: Pointer for storage of EOI line status
*
* Returns 0 if everything went fine
*/
uint32_t gpib_read_byte(uint8_t *byte, bool *eoi_status) {
    // Raise NRFD, informing the talker we are ready for the byte
    gpio_set(NRFD_CP, NRFD);

    // Assert NDAC, informing the talker we have not yet accepted the byte
    gpio_clear(NDAC_CP, NDAC);

    // Wait for DAV to go low, informing us the byte is read to be read
    // TODO: timeouts here
    while(gpio_get(DAV_CP, DAV)){}

    // Assert NRFD, informing the talker to not change the data lines
    gpio_clear(NRFD_CP, NRFD);

    // Read the data on the port, flip the bits, and read in the EOI line
    *byte = READ_DIO();
    *eoi_status = gpio_get(EOI_CP, EOI);

    // TODO: debug message printf("Got byte: %c %x ", a, a);

    // Un-assert NDAC, informing talker that we have accepted the byte
    gpio_set(NDAC_CP, NDAC);

    // Wait for DAV to go high; the talkers knows that we have read the byte
    // TODO: timeouts here
    while(!gpio_get(DAV_CP, DAV)){}

    // Get ready for the next byte by asserting NDAC
    gpio_clear(NDAC_CP, NDAC);

    // TODO: Add in our variable delay here

    return 0;
}

/** Read from the GPIB bus until the specified end condition is met
*
* use_eoi:
*
* Returns 0 if everything went fine, or 1 if there was an error
*/
uint32_t gpib_read(bool use_eoi,
					enum eos_codes eos_code,
					const char *eos_string,
					bool eot_enable) {
    uint8_t byte;
    bool eoi_status = 1;
    bool eos_checknext = 0;	//used to strip CR+LF eos (avoid sending the CR to host)
    uint8_t cmd_buf[3];
    uint32_t error_found = 0;

    if(mode) {
        // Command all talkers and listeners to stop
        cmd_buf[0] = CMD_UNT;
        error_found = error_found || gpib_cmd(cmd_buf);
        cmd_buf[0] = CMD_UNL;
        error_found = error_found || gpib_cmd(cmd_buf);
        if(error_found){return 1;}

        // Set the controller into listener mode
        cmd_buf[0] = 0 + 0x20; // FIXME: Should be my_address + 0x20
        error_found = error_found || gpib_cmd(cmd_buf);
        if(error_found){return 1;}

        // Set target device into talker mode
        cmd_buf[0] = 1 + 0x40; // FIXME: Should be address + 0x40
        error_found = gpib_cmd(cmd_buf);
        if(error_found){return 1;}
    }

    // Beginning of GPIB read loop
    // TODO: debug message printf("gpib_read loop start\n\r");
	// TODO: Make sure modes are set correctly
    gpio_clear(FLOW_PORT, TE);
    gpio_mode_setup(DAV_CP, GPIO_MODE_INPUT, GPIO_PUPD_NONE, DAV);
    gpio_mode_setup(NRFD_CP, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, NRFD | NDAC);

    if(use_eoi) { // Read until EOI
        do {
            if(gpib_read_byte(&byte, &eoi_status)){
				// Read error
                if(eot_enable) {
                    host_tx(eot_char);
                }
                return 1;
            }
            if (eoi_status) {
				//all done
				host_tx(byte);
				break;
            }
        } while (eoi_status);
    // TODO : "strip" for last byte
	} else { // Read until EOS char found
		do {
            if(gpib_read_byte(&byte, &eoi_status)){
				// Read error
                if(eot_enable) {
                    host_tx(eot_char);
                }
                return 1;
            }
            // Check to see if the byte we just read is the specified EOS byte
            if (eos_code == EOS_CRLF) {
            	if (eos_checknext) {
					if (byte == '\n') {
						//correct CR+LF termination : all done.
						break;
					}
					//not CRLF, so we'll send the previous CR too
					eos_checknext = 0;
					host_tx('\r');
					host_tx(byte);
					continue;
				}
				if((byte == '\r')) {
					//got a CR; we won't push to host now
					eos_checknext = 1;
					continue;
				}
            } else { // not CR+LF terminated
                if((byte == eos_string[0])) {
					//all done
					break;
                }
				host_tx(byte);
            }
        } while (1);
    }	//if !use_eoi

    if(eot_enable) {
		host_tx(eot_char);
    }

    // TODO: debug message printf("gpib_read loop end\n\r");

    if (mode) {
        // Command all talkers and listeners to stop
        error_found = 0;
        cmd_buf[0] = CMD_UNT;
        error_found = error_found || gpib_cmd(cmd_buf);
        cmd_buf[0] = CMD_UNL;
        error_found = error_found || gpib_cmd(cmd_buf);
    }

    // TODO: debug message printf("gpib_read end\n\r");

    return error_found;
}

/** Address the specified GPIB address to listen
*
* address: GPIB bus address which should be listening
*
* Returns 0 if everything went fine, or 1 if there was an error
*/
uint32_t gpib_address_target(uint32_t address) {
    uint32_t write_error = 0;
    uint8_t cmd_buf[3];
    cmd_buf[0] = CMD_UNT;
    write_error = write_error || gpib_cmd(cmd_buf);
    cmd_buf[0] = CMD_UNL; // Everyone stop listening
    write_error = write_error || gpib_cmd(cmd_buf);
    cmd_buf[0] = address + 0x20;
    write_error = write_error || gpib_cmd(cmd_buf);
    return write_error;
}

/**
* Assigns our controller as the GPIB bus controller.
* The IFC line is toggled, REN line is asserted, and the DCL command byte
* is sent to all instruments on the bus.
*
* Returns 0 if everything went fine, or 1 if there was an error
*/
uint32_t gpib_controller_assign(void) {

    uint8_t cmd_buf[3];
    // Assert interface clear. Resets bus and makes it controller in charge
    gpio_mode_setup(IFC_CP, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, IFC);
    gpio_clear(IFC_CP, IFC);
    delay_ms(200);
    gpio_set(IFC_CP, IFC);

    // Put all connected devices into "remote" mode
    gpio_mode_setup(REN_CP, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, REN);
    gpio_clear(REN_CP, REN);

    // Send GPIB DCL command, which clears all devices on the bus
    cmd_buf[0] = CMD_DCL;
    return gpib_cmd(cmd_buf);
}

/** conduct serial poll
 *
 * @param status_byte poll result will be written there
 *
 * @return 0 if OK
 */
uint32_t gpib_serial_poll(int address, u8 *status_byte) {
    char error = 0;
	u8 cmd_buf[1];
	bool eoistat = 0;

    cmd_buf[0] = CMD_SPE; // enable serial poll
    error = error || gpib_cmd(cmd_buf);
    cmd_buf[0] = address + 0x40;
    error = error || gpib_cmd(cmd_buf);
    if (error) return -1;

    error = gpib_read_byte(status_byte, &eoistat);
    cmd_buf[0] = CMD_SPD; // disable serial poll
    gpib_cmd(cmd_buf);
    if (error) return -1;
    return 0;
}
