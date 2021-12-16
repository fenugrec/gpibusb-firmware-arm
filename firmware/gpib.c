/*
* GPIBUSB Adapter
* gpib.c
**
* � 2014 Steven Casagrande (scasagrande@galvant.ca).
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

#include <stdbool.h>

#include "printf_config.h"	//hax, just to get PRINTF_ALIAS_STANDARD_FUNCTION_NAMES...
#include <printf/printf.h>

#include <string.h>

#include <libopencm3/stm32/gpio.h>

#include "firmware.h"
#include "gpib.h"
#include "hw_conf.h"
#include "host_comms.h"
#include "hw_backend.h"
#include "stypes.h"

/* global vars */
int partnerAddress = 1;
int myAddress = 0;
bool controller_mode = 1;
bool debug = 0; // enable or disable read&write error messages
char eot_char = '\r'; // default CR
uint32_t timeout = 1000;	//in ms
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
	u32 t0, tdelta = timeout;

	// TODO: Set pin modes to output as required for writing, and revert to input on exit/abort

	gpio_set(FLOW_PORT, PE); // Enable power on the bus driver ICs

	if(atn) { output_low(ATN_CP, ATN); }
	if(!length) { return 0; }

	gpio_mode_setup(NRFD_CP, GPIO_MODE_INPUT, GPIO_PUPD_NONE, NRFD | NDAC);
	gpio_set(FLOW_PORT, TE); // Enable talking
	output_high(EOI_CP, EOI);
	output_high(DAV_CP, DAV);


	// Before we start transfering, we have to make sure that NRFD is high
	// and NDAC is low

	t0 = get_ms();
	while(gpio_get(NDAC_CP, NDAC) || !gpio_get(NRFD_CP, NRFD)) {
#ifdef WITH_TIMEOUT
		restart_wdt();
		if ((get_ms() - t0) >= tdelta) {
			if (debug == 1) {
				printf("write: timeout: waiting for NRFD+ && NDAC-%c", eot_char);
			}
			device_talk = false;
			device_srq = false;
			prep_gpib_pins(controller_mode);
			return 1;
		}
#endif // WITH_TIMEOUT
	}

	// Loop through each byte and write it to the GPIB bus
	for(i=0;i<length;i++) {
		byte = bytes[i];

		#ifdef VERBOSE_DEBUG
		printf("Writing byte: %c (%02X)%c", byte, byte, eot_char);
		#endif

		// Wait for NDAC to go low, indicating previous bit is now done with
		t0 = get_ms();
		while(gpio_get(NDAC_CP, NDAC)) {
#ifdef WITH_TIMEOUT
			restart_wdt();
			if ((get_ms() - t0) >= tdelta) {
				if (debug == 1) {
					printf("write timeout: waiting for NDAC-%c", eot_char);
				}
				device_talk = false;
				device_srq = false;
				prep_gpib_pins(controller_mode);
				return 1;
			}
#endif // WITH_TIMEOUT
		}

        // Put the byte on the data lines
        gpio_mode_setup(DIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, DIO_PORTMASK);
        WRITE_DIO(byte);

        // Assert EOI if on last byte and using EOI
        if((i==length-1) && (use_eoi)) {output_low(EOI_CP, EOI);}

        // Wait for NRFD to go high, indicating listeners are ready for data
		t0 = get_ms();
		while(!gpio_get(NRFD_CP, NRFD)) {
#ifdef WITH_TIMEOUT
			restart_wdt();
			if ((get_ms() - t0) >= tdelta) {
				if (debug == 1) {
					 printf("write timeout: Waiting for NRFD+%c", eot_char);
				}
				device_talk = false;
				device_srq = false;
				prep_gpib_pins(controller_mode);
				return 1;
			}
#endif // WITH_TIMEOUT
		}

		// Assert DAV, informing listeners that the data is ready to be read
		output_low(DAV_CP, DAV);

		// Wait for NDAC to go high, all listeners have accepted the byte
		t0 = get_ms();
		while(!gpio_get(NDAC_CP, NDAC)) {
#ifdef WITH_TIMEOUT
			restart_wdt();
			if ((get_ms() - t0) >= tdelta) {
				if (debug == 1) {
					 printf("write timeout: Waiting for NDAC+%c", eot_char);
				}
				device_talk = false;
				device_srq = false;
				prep_gpib_pins(controller_mode);
				return 1;
			}
#endif // WITH_TIMEOUT
		}

		// Release DAV, indicating byte is no longer valid
		output_high(DAV_CP, DAV);
    } // Finished outputting all bytes to the listeners

    gpio_mode_setup(DIO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, DIO_PORTMASK);
    gpio_clear(FLOW_PORT, TE); // Disable talking

    // If the byte was a GPIB command byte, release ATN line
    if(atn) { output_high(ATN_CP, ATN); }

    gpio_mode_setup(EOI_CP, GPIO_MODE_INPUT, GPIO_PUPD_NONE, DAV | EOI);
    output_high(NRFD_CP, NRFD | NDAC);
    gpio_clear(FLOW_PORT, PE);

    return 0;
}

/** Receive a single byte from the GPIB bus
* Assumes ports were setup properly
*
* @param byte: Pointer to where the received byte will be stored
* @param eoi_status: Pointer for storage of EOI line status
*
* Returns 0 if everything went fine
*/
uint32_t gpib_read_byte(uint8_t *byte, bool *eoi_status) {
	u32 t0, tdelta = timeout;

	// Raise NRFD, informing the talker we are ready for the byte
	output_high(NRFD_CP, NRFD);

	// Assert NDAC, informing the talker we have not yet accepted the byte
	output_low(NDAC_CP, NDAC);

	gpio_mode_setup(DAV_CP, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, DAV);

	// Wait for DAV to go low, informing us the byte is read to be read
	t0 = get_ms();
	while(gpio_get(DAV_CP, DAV)) {
#ifdef WITH_TIMEOUT
		restart_wdt();
		if ((get_ms() - t0) >= tdelta) {
			if (debug == 1) {
				 printf("readbyte timeout: Waiting for DAV-%c", eot_char);
			}
			device_listen = false;
			prep_gpib_pins(controller_mode);
			return 1;
		}
#endif // WITH_TIMEOUT
	}

	// Assert NRFD, informing the talker to not change the data lines
	output_low(NRFD_CP, NRFD);

	// Read the data on the port, flip the bits, and read in the EOI line
	*byte = READ_DIO();
	*eoi_status = gpio_get(EOI_CP, EOI);

	#ifdef VERBOSE_DEBUG
	printf("Got byte: %c (%02X)%c", *byte, *byte, eot_char);
	#endif

	// Un-assert NDAC, informing talker that we have accepted the byte
	gpio_mode_setup(NDAC_CP, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, NDAC);

	// Wait for DAV to go high; the talkers knows that we have read the byte
	t0 = get_ms();
	while(!gpio_get(DAV_CP, DAV)) {
#ifdef WITH_TIMEOUT
		restart_wdt();
		if ((get_ms() - t0) >= tdelta) {
			if (debug == 1) {
				 printf("readbyte timeout: Waiting for DAV+%c", eot_char);
			}
			device_listen = false;
			prep_gpib_pins(controller_mode);
			return 1;
		}
#endif // WITH_TIMEOUT
    }

    // Get ready for the next byte by asserting NDAC
    output_low(NDAC_CP, NDAC);

    // TODO: Add in our variable delay here

    return 0;
}

/** Read from the GPIB bus until the specified end condition is met
*
* @param readmode termination by EOI , char, or timeout
* @param eos_char (valid if use_eos=1)
*
* Returns 0 if everything went fine, or 1 if there was an error
*/
uint32_t gpib_read(enum gpib_readmode readmode,
					uint8_t eos_char,
					bool eot_enable) {
    uint8_t byte;
    bool eoi_status = 1;
    //bool eos_checknext = 0;	//used to strip CR+LF eos (avoid sending the CR to host)
    uint8_t cmd_buf[3];
    uint32_t error_found = 0;

    if(controller_mode) {
        // Command all talkers and listeners to stop
        cmd_buf[0] = CMD_UNT;
        error_found = error_found || gpib_cmd(cmd_buf);
        cmd_buf[0] = CMD_UNL;
        error_found = error_found || gpib_cmd(cmd_buf);
        if(error_found){return 1;}

        // Set the controller into listener mode
        cmd_buf[0] = myAddress + CMD_LAD;
        error_found = error_found || gpib_cmd(cmd_buf);
        if(error_found){return 1;}

        // Set target device into talker mode
        cmd_buf[0] = partnerAddress + CMD_TAD;
        error_found = gpib_cmd(cmd_buf);
        if(error_found){return 1;}
    }

    // Beginning of GPIB read loop
    #ifdef VERBOSE_DEBUG
    printf("gpib_read loop start%c", eot_char);
    #endif

	// TODO: Make sure modes are set correctly
    gpio_clear(FLOW_PORT, TE);
    gpio_mode_setup(DAV_CP, GPIO_MODE_INPUT, GPIO_PUPD_NONE, DAV);
    gpio_mode_setup(NRFD_CP, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, NRFD | NDAC);

    // TODO : what happens if device keeps sending data, or never sends EOI/EOS ?
    switch (readmode) {
	case GPIBREAD_EOI:
        do {
            if(gpib_read_byte(&byte, &eoi_status)){
				// Read error
                if(eot_enable) {
                    host_tx(eot_char);
                }
                return 1;
            }
            host_tx(byte);
            if (!eoi_status) {
				//all done
				break;
            }
        } while (eoi_status);
        // TODO : "strip" for last byte ?
		break;
    case GPIBREAD_EOS:
		do {
            if(gpib_read_byte(&byte, &eoi_status)){
				// Read error
                if(eot_enable) {
                    host_tx(eot_char);
                }
                return 1;
            }
            // Check to see if the byte we just read is the specified EOS byte
            if (byte == eos_char) {
				//all done
				break;
            }
            // XXX TODO : is it necessary to strip CR+LF if eos_char is CR (or LF) ?
            // prologix docs not obvious
			host_tx(byte);
        } while (1);
	case GPIBREAD_TMO:
		printf("read with timeout not implemented\n");
		//XXX TODO : implement read with timeout
		break;
	default:
		//XXX assert
		break;
    }	//if !use_eoi

    if(eot_enable) {
		host_tx(eot_char);
    }

    #ifdef VERBOSE_DEBUG
    printf("gpib_read loop end%c", eot_char);
    #endif

    if (controller_mode) {
        // Command all talkers and listeners to stop
        error_found = 0;
        cmd_buf[0] = CMD_UNT;
        error_found = error_found || gpib_cmd(cmd_buf);
        cmd_buf[0] = CMD_UNL;
        error_found = error_found || gpib_cmd(cmd_buf);
    }

    #ifdef VERBOSE_DEBUG
    printf("gpib_read end%c", eot_char);
    #endif

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
    cmd_buf[0] = address + CMD_LAD;
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
    output_low(IFC_CP, IFC);
    delay_ms(200);
    output_high(IFC_CP, IFC);

    // Put all connected devices into "remote" mode
    output_low(REN_CP, REN);

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
    cmd_buf[0] = address + CMD_TAD;
    error = error || gpib_cmd(cmd_buf);
    if (error) return -1;

    error = gpib_read_byte(status_byte, &eoistat);
    cmd_buf[0] = CMD_SPD; // disable serial poll
    gpib_cmd(cmd_buf);
    if (error) return -1;
    return 0;
}
