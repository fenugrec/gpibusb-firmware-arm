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

#include <stdbool.h>

#include "printf_config.h"  //hax, just to get PRINTF_ALIAS_STANDARD_FUNCTION_NAMES...
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


const char *gpib_states_s[GPIBSTATE_MAX] = {
	[CINI] = "CINI",
	[CIDS] = "CIDS,",
	[CCMS] = "CCMS,",
	[CTAS] = "CTAS,",
	[CLAS] = "CLAS,",
	[DINI] = "DINI,",
	[DIDS] = "DIDS,",
	[DLAS] = "DLAS,",
	[DTAS] = "DTAS,",
};

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
static enum errcodes _gpib_write(const uint8_t *bytes, uint32_t length, bool use_eoi);


/** Write a GPIB command byte
*
* This will assert the GPIB ATN line.
*
*/
enum errcodes gpib_cmd(const uint8_t x) {
	const uint8_t b = x;
	return gpib_cmd_m(&b, 1);
}

enum errcodes gpib_cmd_m(const uint8_t *byte, unsigned len) {
	enum errcodes rv;
	setControls(CCMS);
	rv = _gpib_write(byte, len, 0);
	setControls(CIDS);
	return rv;
}

/** Write a GPIB data string to the GPIB bus.
*
* See _gpib_write for parameter information
*/
enum errcodes gpib_write(const uint8_t *bytes, uint32_t length, bool use_eoi) {
	enum errcodes rv;
	enum gpib_states ws, next_state;
	if (gpib_cfg.controller_mode) {
		ws = CTAS;
		next_state= CIDS;
	} else {
		ws = DTAS;
		next_state = DIDS;
	}
	setControls(ws);
	rv = _gpib_write(bytes, length, use_eoi);
	setControls(next_state);
	return rv;
}

/** Write a string of bytes to the GPIB bus
*
* bytes: Array of bytes to be written to the bus
* length: number of bytes to write to the bus.
* use_eoi: Set whether the GPIB EOI line should be asserted or not on
	transmission of the last byte
*
* Returns E_OK if complete, or E_TIMEOUT
*/
static enum errcodes _gpib_write(const uint8_t *bytes, uint32_t length, bool use_eoi) {
	uint8_t byte; // Storage variable for the current character
	uint32_t i;
	u32 t0;
	u32 tdelta = gpib_cfg.timeout;

	assert_basic(length);

	dio_output();

	// wait NRFD high
	t0 = get_ms();
	while (!gpio_get(HCTRL1_CP, NRFD)) {
		restart_wdt();
		u32 now = get_ms();
		if (TS_ELAPSED(now,t0,tdelta)) {
			DEBUG_PRINTF("write: timeout: waiting for NRFD+\n");
			goto wt_exit;
		}
	}

	// Loop through each byte and write it to the GPIB bus
	for (i=0; i<length; i++) {
		byte = bytes[i];

		DEBUG_PRINTF("Writing byte: %c (%02X)\n", byte, byte);

		// Wait for NDAC to go low, indicating previous byte is done
		t0 = get_ms(); // inter-byte timeout
		while (gpio_get(HCTRL1_CP, NDAC)) {
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
		if ((i==length-1) && (use_eoi)) {
			assert_signal(EOI_CP, EOI);
		}

		// Wait for NRFD to go high, indicating listeners are ready for data
		while (!gpio_get(HCTRL1_CP, NRFD)) {
			restart_wdt();
			u32 now = get_ms();
			if (TS_ELAPSED(now,t0,tdelta)) {
				DEBUG_PRINTF("write timeout: Waiting for NRFD+\n");
				goto wt_exit;
			}
		}

		// Assert DAV, informing listeners that the data is ready to be read
		assert_signal(HCTRL1_CP, DAV);

		// Wait for NDAC to go high, all listeners have accepted the byte
		while (!gpio_get(HCTRL1_CP, NDAC)) {
			restart_wdt();
			u32 now = get_ms();
			if (TS_ELAPSED(now,t0,tdelta)) {
				DEBUG_PRINTF("write timeout: Waiting for NDAC+\n");
				goto wt_exit;
			}
		}

		// byte is no longer valid
		unassert_signal(HCTRL1_CP, DAV);
	} // Finished outputting all bytes to the listeners

	dio_float();

	return E_OK;
wt_exit:
	gpib_cfg.device_talk = false;
	gpib_cfg.device_srq = false;
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

	assert_signal(HCTRL1_CP, NDAC);

	// Raise NRFD, informing the talker we are ready for the byte
	unassert_signal(HCTRL1_CP, NRFD);

	// Wait for DAV to go low, informing us the byte is read to be read
	t0 = get_ms();
	while (gpio_get(HCTRL1_CP, DAV)) {
		restart_wdt();
		u32 now = get_ms();
		if (TS_ELAPSED(now,t0,tdelta)) {
			DEBUG_PRINTF("readbyte timeout: Waiting for DAV-\n");
			goto rt_exit;
		}
	}

	// informing the talker to not change the data lines
	assert_signal(HCTRL1_CP, NRFD);

	// Read the data on the port and read in the EOI line
	*byte = READ_DIO();
	*eoi_status = !gpio_get(EOI_CP, EOI);

	DEBUG_PRINTF("Got byte: (%02X)\n", *byte);

	// informing talker that we have accepted the byte
	unassert_signal(HCTRL1_CP, NDAC);

	// Wait for DAV to go high; the talkers knows that we have read the byte
	while (!gpio_get(HCTRL1_CP, DAV)) {
		restart_wdt();
		u32 now = get_ms();
		if (TS_ELAPSED(now,t0,tdelta)) {
			DEBUG_PRINTF("readbyte timeout: Waiting for DAV+\n");
			goto rt_exit;
		}
	}

	// Get ready for the next byte by asserting NDAC
	assert_signal(HCTRL1_CP, NDAC);

	return E_OK;
rt_exit:
	gpib_cfg.device_listen = false;
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
	enum gpib_states next_state;

	if (gpib_cfg.controller_mode) {
		setControls(CLAS);
		next_state = CIDS;
	} else {
		setControls(DLAS);
		next_state = DIDS;
	}

	// Beginning of GPIB read loop
	DEBUG_PRINTF("gpib_read loop start\n");

	dio_float();

	// TODO : what happens if device keeps sending data, or never sends EOI/EOS ?
	switch (readmode) {
	case GPIBREAD_EOI:
		do {
			if (gpib_read_byte(&byte, &eoi_status)) {
				// Read error
				DEBUG_PRINTF("gpr EOI:E\n");
				goto e_timeout;
			}
			host_tx(byte);
			if (eoi_status) {
				//all done
				break;
			}
			if (host_rx_datapresent()) {
				DEBUG_PRINTF("gpr interrupted\n");
				break;
			}
		} while (1);
		// TODO : "strip" for last byte ?
		break;
	case GPIBREAD_EOS:
		do {
			if (gpib_read_byte(&byte, &eoi_status)) {
				DEBUG_PRINTF("gpr EOS:E\n");
				goto e_timeout;
			}
			// Check to see if the byte we just read is the specified EOS byte
			if (byte == eos_char) {
				//all done
				break;
			}
			if (host_rx_datapresent()) {
				DEBUG_PRINTF("gpr interrupted\n");
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
			if (host_rx_datapresent()) {
				DEBUG_PRINTF("gpr interrupted\n");
				break;
			}
			// E_TIMEOUT or other errors (no such thing yet)
			break;
		} while (1);
		break;
	default:
		assert_failed();
		break;
	}

	if (eot_enable & eoi_status) {
		host_tx(gpib_cfg.eot_char);
	}

	DEBUG_PRINTF("gpib_read end\n");

	setControls(next_state);
	return error_found;

e_timeout:
	setControls(next_state);
	return E_TIMEOUT;
}



void gpib_unaddress(void) {
	const uint8_t cmdbuf[] = { CMD_UNT, CMD_UNL };
	(void) gpib_cmd_m(cmdbuf, sizeof(cmdbuf));
}

/** Address the specified GPIB address to listen
*
*/
enum errcodes gpib_address_target(uint32_t address, enum addr_dir dir) {
	u8 base;
	if (dir == CTRL_TALK) {
		base = CMD_LAD;
	} else {
		base = CMD_TAD;
	}
	u8 cmdbuf[] = {
		CMD_UNT,
		CMD_UNL,
		address + base,
	};
	return gpib_cmd_m(cmdbuf, sizeof(cmdbuf));
}


void pulse_ifc(void) {
	assert_signal(HCTRL2_CP, IFC);
	delay_ms(200);
	unassert_signal(HCTRL2_CP, IFC);
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
	setControls(CINI);

	// Put all connected devices into "remote" mode
	assert_signal(HCTRL2_CP, REN);

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

	dio_float();
	setControls(CLAS);

	error = gpib_read_byte(status_byte, &eoistat);
	setControls(CTAS);
	gpib_cmd(CMD_SPD);
	if (error) return -1;
	return 0;
}
