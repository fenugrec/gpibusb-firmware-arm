/*
* GPIBUSB Adapter
* firmware.c
**
* (c) fenugrec 2018-2021
* based on S. Casagrande's "gpibusb-arm" project.
*
* GPLv3
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
	pre_main();
	hw_setup();

	setControls(DINI);  //should be safe default

	// TODO: Load settings from EEPROM

	host_comms_init();
	cmd_parser_init();

	led_update(LEDPATTERN_IDLE);
	fwusb_init();

	while (1) {
		restart_wdt();
		cmd_poll();
		led_poll();
	}

	return 0;
}

