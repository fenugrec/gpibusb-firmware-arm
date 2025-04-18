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
#include "utils.h"

/* global vars. Not all of these are saved to EEPROM*/
struct gpib_config gpib_cfg = {
	.controller_mode = 0,
	.debug = 0,
	.partnerAddress = 1,
	.myAddress = 0,
	.eos_code = EOS_NUL,
	.eoiUse = 1,
	.eot_char = '\n',
	.eot_enable = 1,
	.autoread = 1,
	.timeout = 2000,
	.device_talk = false,
	.device_listen = false,
	.device_srq = false,
};

/* Some forward decls that don't need to be in the public gpib.h
*/
static enum errcodes _gpib_write(const uint8_t *bytes, uint32_t length, bool atn, bool use_eoi);


/** Write a GPIB command byte
*
* This will assert the GPIB ATN line.
*
* See _gpib_write for parameter information
*/
enum errcodes gpib_cmd(uint8_t byte) {
	return _gpib_write(&byte, 1, 1, 0);
}

/** Write a GPIB data string to the GPIB bus.
*
* See _gpib_write for parameter information
*/
enum errcodes gpib_write(const uint8_t *bytes, uint32_t length, bool use_eoi) {
	return _gpib_write(bytes, length, 0, use_eoi);
}

/** Write a string of bytes to the GPIB bus
*
* bytes: Array of bytes to be written to the bus
* length: number of bytes to write to the bus.
* atn: Whether the GPIB ATN line should be asserted or not. Set to 1 if byte
	is a GPIB command byte or 0 otherwise.
* use_eoi: Set whether the GPIB EOI line should be asserted or not on
	transmission of the last byte
*
* Returns E_OK if complete, or E_TIMEOUT
*/
static enum errcodes _gpib_write(const uint8_t *bytes, uint32_t length, bool atn, bool use_eoi) {
	uint8_t byte; // Storage variable for the current character
	uint32_t i;
	u32 t0;
	u32 tdelta = gpib_cfg.timeout;

	assert_basic(length);

	// TODO: Set pin modes to output as required for writing, and revert to input on exit/abort

	gpio_set(FLOW_PORT, PE); // Enable power on the bus driver ICs

	if(atn) { output_low(ATN_CP, ATN); }

	output_float(NRFD_CP, NRFD | NDAC);
	gpio_set(FLOW_PORT, TE); // Enable talking
	output_high(EOI_CP, EOI);
	output_high(DAV_CP, DAV);

	gpio_mode_setup(DIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, DIO_PORTMASK);

	// wait NRFD high

	t0 = get_ms();
	while(!gpio_get(NRFD_CP, NRFD)) {
		restart_wdt();
		u32 now = get_ms();
		if (TS_ELAPSED(now,t0,tdelta)) {
			DEBUG_PRINTF("write: timeout: waiting for NRFD+\n");
			goto wt_exit;
		}
	}

	// Loop through each byte and write it to the GPIB bus
	for(i=0;i<length;i++) {
		byte = bytes[i];

		DEBUG_PRINTF("Writing byte: %c (%02X)\n", byte, byte);

		// Wait for NDAC to go low, indicating previous byte is done
		t0 = get_ms(); // inter-byte timeout
		while(gpio_get(NDAC_CP, NDAC)) {
			restart_wdt();
			u32 now = get_ms();
			if (TS_ELAPSED(now,t0,tdelta)) {
				DEBUG_PRINTF("write timeout: waiting for NDAC-\n");
				goto wt_exit;
			}
		}

		// Put the byte on the data lines
		WRITE_DIO(byte);

		// Assert EOI if on last byte and using EOI
		if((i==length-1) && (use_eoi)) {output_low(EOI_CP, EOI);}

		// Wait for NRFD to go high, indicating listeners are ready for data
		while(!gpio_get(NRFD_CP, NRFD)) {
			restart_wdt();
			u32 now = get_ms();
			if (TS_ELAPSED(now,t0,tdelta)) {
				DEBUG_PRINTF("write timeout: Waiting for NRFD+\n");
				goto wt_exit;
			}
		}

		// Assert DAV, informing listeners that the data is ready to be read
		output_low(DAV_CP, DAV);

		// Wait for NDAC to go high, all listeners have accepted the byte
		while(!gpio_get(NDAC_CP, NDAC)) {
			restart_wdt();
			u32 now = get_ms();
			if (TS_ELAPSED(now,t0,tdelta)) {
				DEBUG_PRINTF("write timeout: Waiting for NDAC+\n");
				goto wt_exit;
			}
		}

		// Release DAV, indicating byte is no longer valid
		output_high(DAV_CP, DAV);
	} // Finished outputting all bytes to the listeners

	output_float(DIO_PORT, DIO_PORTMASK);
	gpio_clear(FLOW_PORT, TE); // Disable talking

	// If the byte was a GPIB command byte, release ATN line
	if(atn) { output_high(ATN_CP, ATN); }

	output_float(EOI_CP, DAV | EOI);
	output_high(NRFD_CP, NRFD | NDAC);
	gpio_clear(FLOW_PORT, PE);

	return E_OK;
wt_exit:
	gpib_cfg.device_talk = false;
	gpib_cfg.device_srq = false;
	prep_gpib_pins(gpib_cfg.controller_mode);
	return E_TIMEOUT;
}

/** Receive a single byte from the GPIB bus, with timeout
* Assumes DIO, EOI, DAV, TE ports were setup properly
*
* @param byte: Pointer to where the received byte will be stored
* @param eoi_status: Pointer for storage of EOI status (1 = asserted (low))
*
* Returns E_OK or E_TIMEOUT
*/
enum errcodes gpib_read_byte(uint8_t *byte, bool *eoi_status) {
	u32 t0;
	u32 tdelta = gpib_cfg.timeout;

	// Assert NDAC, informing the talker we have not yet accepted the byte
	output_low(NDAC_CP, NDAC);

	// Raise NRFD, informing the talker we are ready for the byte
	output_high(NRFD_CP, NRFD);

	// Wait for DAV to go low, informing us the byte is read to be read
	t0 = get_ms();
	while(gpio_get(DAV_CP, DAV)) {
		restart_wdt();
		u32 now = get_ms();
		if (TS_ELAPSED(now,t0,tdelta)) {
			DEBUG_PRINTF("readbyte timeout: Waiting for DAV-\n");
			goto rt_exit;
		}
	}

	// Assert NRFD, informing the talker to not change the data lines
	output_low(NRFD_CP, NRFD);

	// Read the data on the port, flip the bits, and read in the EOI line
	*byte = READ_DIO();
	*eoi_status = !gpio_get(EOI_CP, EOI);

	DEBUG_PRINTF("Got byte: (%02X)\n", *byte);

	// Un-assert NDAC, informing talker that we have accepted the byte
	output_float(NDAC_CP, NDAC);

	// Wait for DAV to go high; the talkers knows that we have read the byte
	while(!gpio_get(DAV_CP, DAV)) {
		restart_wdt();
		u32 now = get_ms();
		if (TS_ELAPSED(now,t0,tdelta)) {
			DEBUG_PRINTF("readbyte timeout: Waiting for DAV+\n");
			goto rt_exit;
		}
	}

	// Get ready for the next byte by asserting NDAC
	output_low(NDAC_CP, NDAC);

	return E_OK;
rt_exit:
	gpib_cfg.device_listen = false;
	prep_gpib_pins(gpib_cfg.controller_mode);
	return E_TIMEOUT;
}

/** Read from the GPIB bus until the specified end condition is met
*
* @param readmode termination by EOI , char, or timeout
* @param eos_char (valid if use_eos=1)
*
* Returns 0 if everything went fine, or 1 if there was an error
*/
enum errcodes gpib_read(enum gpib_readmode readmode,
					uint8_t eos_char,
					bool eot_enable) {
	uint8_t byte;
	bool eoi_status = 0;
	//bool eos_checknext = 0;	//used to strip CR+LF eos (avoid sending the CR to host)
	uint32_t error_found = 0;

	if(gpib_cfg.controller_mode) {
		// Command all talkers and listeners to stop
		error_found = error_found || gpib_cmd(CMD_UNT);
		error_found = error_found || gpib_cmd(CMD_UNL);
		if(error_found){return E_TIMEOUT;}

		// Set the controller into listener mode
		u8 cmd = gpib_cfg.myAddress + CMD_LAD;
		error_found = error_found || gpib_cmd(cmd);
		if(error_found){return E_TIMEOUT;}

		// Set target device into talker mode
		cmd = gpib_cfg.partnerAddress + CMD_TAD;
		error_found = gpib_cmd(cmd);
		if(error_found){return E_TIMEOUT;}
	}

	// Beginning of GPIB read loop
	DEBUG_PRINTF("gpib_read loop start\n");

	output_float(DIO_PORT, DIO_PORTMASK);
	output_float(EOI_CP, DAV | EOI);
	gpio_clear(FLOW_PORT, TE);

	// TODO : what happens if device keeps sending data, or never sends EOI/EOS ?
	switch (readmode) {
	case GPIBREAD_EOI:
		do {
			if (gpib_read_byte(&byte, &eoi_status)) {
				// Read error
				DEBUG_PRINTF("gpr EOI:E\n");
				return E_TIMEOUT;
			}
			host_tx(byte);
			if (eoi_status) {
				//all done
				break;
			}
		} while (1);
		// TODO : "strip" for last byte ?
		break;
	case GPIBREAD_EOS:
		do {
			if (gpib_read_byte(&byte, &eoi_status)) {
				DEBUG_PRINTF("gpr EOS:E\n");
				return E_TIMEOUT;
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
		break;
	case GPIBREAD_TMO:
		// TODO : large timeout incase device never stops writing ? do we care ?
		do {
			enum errcodes rv = gpib_read_byte(&byte, &eoi_status);
			if (rv == E_OK) {
				host_tx(byte);
				continue;
			}
			DEBUG_PRINTF("gpr TMO:E\n");
			// E_TIMEOUT or other errors (no such thing yet)
			break;
		} while (1);
		break;
	default:
		assert_failed();
		break;
	}

	if(eot_enable & eoi_status) {
		host_tx(gpib_cfg.eot_char);
	}

	DEBUG_PRINTF("gpib_read loop end\n");

	if (gpib_cfg.controller_mode) {
		// Command all talkers and listeners to stop
		error_found = 0;
		error_found = error_found || gpib_cmd(CMD_UNT);
		error_found = error_found || gpib_cmd(CMD_UNL);
	}

	DEBUG_PRINTF("gpib_read end\n");

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
	write_error = write_error || gpib_cmd(CMD_UNT);
	write_error = write_error || gpib_cmd(CMD_UNL);
	u8 cmd = address + CMD_LAD;
	write_error = write_error || gpib_cmd(cmd);
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
	// Assert interface clear. Resets bus and makes it controller in charge
	output_low(IFC_CP, IFC);
	delay_ms(200);
	output_high(IFC_CP, IFC);

	// Put all connected devices into "remote" mode
	output_low(REN_CP, REN);

	// Send GPIB DCL command, which clears all devices on the bus
	return gpib_cmd(CMD_DCL);
}

/** conduct serial poll
 *
 * @param status_byte poll result will be written there
 *
 * @return 0 if OK
 */
uint32_t gpib_serial_poll(int address, u8 *status_byte) {
	char error = 0;
	u8 cmd;
	bool eoistat = 0;

	error = error || gpib_cmd(CMD_SPE);
	cmd = address + CMD_TAD;
	error = error || gpib_cmd(cmd);
	if (error) return -1;

	output_float(DIO_PORT, DIO_PORTMASK);
	output_float(EOI_CP, DAV | EOI);
	gpio_clear(FLOW_PORT, TE);

	error = gpib_read_byte(status_byte, &eoistat);
	gpib_cmd(CMD_SPD);
	if (error) return -1;
	return 0;
}
