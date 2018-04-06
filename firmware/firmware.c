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

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include <stdio.h>
#include <errno.h>

#include "firmware.h"
#include "hw_conf.h"
#include "hw_backend.h"
#include "host_comms.h"

#include "gpib.h"

#include "stypes.h"


/** DEFINES *******************************************************************/

/** GLOBALS *******************************************************************/

bool mode = 1;

/** PROTOTYPES ****************************************************************/

static void clock_setup(void);
int main(void);

/** FUNCTIONS *****************************************************************/

static void clock_setup(void)
{
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOC);
}

int main(void)
{
	clock_setup();
	led_setup();
	prep_gpib_pins(mode);

	wdt_setup();
	init_timers();

	// TODO: Load settings from EEPROM

	// Initialize the GPIB bus
	if (mode) {
		gpib_controller_assign();
	}

	gpio_clear(LED_PORT, LED_ERROR);

	host_comms_init();

	while (1) {
		delay_ms(300);
		gpio_toggle(LED_PORT, LED_STATUS);
	}

	return 0;
}

