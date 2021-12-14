/*
* GPIBUSB Adapter
* firmware.C
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

/** INCLUDES ******************************************************************/

#include "firmware.h"
#include "hw_conf.h"
#include "hw_backend.h"
#include "cmd_parser.h"
#include "host_comms.h"
#include "usb_cdc.h"

#include "gpib.h"

#include "stypes.h"


/** FUNCTIONS *****************************************************************/

int main(void)
{
	hw_setup();

	prep_gpib_pins(mode);

	// TODO: Load settings from EEPROM

	host_comms_init();
//	cmd_parser_init();
//	hw_startcomms();

	led_update(LEDPATTERN_IDLE);
	fwusb_init();

	while (1) {
		restart_wdt();
//		cmd_poll();
		led_poll();
	}

	return 0;
}

