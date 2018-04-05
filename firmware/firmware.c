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
#include <libopencm3/stm32/usart.h>
#include <libopencm3/cm3/nvic.h>

#include <stdio.h>
#include <errno.h>

#include "firmware.h"
#include "hw_conf.h"
#include "hw_backend.h"
#include "ring.h"
#include "gpib.h"

#include "stypes.h"


/** DEFINES *******************************************************************/

#define UART_BUFFER_SIZE 1024

/** GLOBALS *******************************************************************/

struct ring output_ring;
struct ring input_ring;

bool mode = 1;

/** These don't need external linkage */
static uint8_t output_ring_buffer[UART_BUFFER_SIZE];
static uint8_t input_ring_buffer[UART_BUFFER_SIZE];

/** PROTOTYPES ****************************************************************/

static void clock_setup(void);
static void usart_setup(void);
int main(void);

/** FUNCTIONS *****************************************************************/

static void clock_setup(void)
{
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOC);

	rcc_periph_clock_enable(RCC_USART2);
}

static void usart_setup(void)
{
	ring_init(&output_ring, output_ring_buffer, UART_BUFFER_SIZE);
	ring_init(&input_ring, input_ring_buffer, UART_BUFFER_SIZE);

	/* A2 = TX, A3 = RX */
	nvic_enable_irq(NVIC_USART2_IRQ);
	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO2);
	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO3);
	gpio_set_output_options(GPIOA, GPIO_OTYPE_OD, GPIO_OSPEED_25MHZ, GPIO3);
	gpio_set_af(GPIOA, GPIO_AF1, GPIO2);
	gpio_set_af(GPIOA, GPIO_AF1, GPIO3);

	usart_set_baudrate(USART2, 115200);
	usart_set_databits(USART2, 8);
	usart_set_stopbits(USART2, USART_STOPBITS_1);
	usart_set_mode(USART2, USART_MODE_TX_RX);
	usart_set_parity(USART2, USART_PARITY_NONE);
	usart_set_flow_control(USART2, USART_FLOWCONTROL_NONE);

	usart_enable_rx_interrupt(USART2);
	usart_enable(USART2);
}

void usart2_isr(void)
{
	u8 rxbyte;
	u8 txbyte;

	// TODO : check for framing / overrun errors too

	// Check if we were called because of RXNE.
	if (usart_get_flag(USART2, USART_ISR_RXNE)) {
		rxbyte = usart_recv(USART2);
		ring_write_ch(&input_ring, rxbyte);
	}

	// Check if we were called because of TXE.
	if (usart_get_flag(USART2, USART_ISR_TXE)) {
		if (ring_read_ch(&output_ring, &txbyte) == -1) {
			//no more data in buffer. TODO : add provisions
			//to re-enable the interrupt, or manually call the ISR,
			//when a new byte is queued.
			usart_disable_tx_interrupt(USART2);
		} else {
			//non-blocking : we'll end up back here anyway
			usart_send(USART2, txbyte);
		}
	}
}

int main(void)
{
	clock_setup();
	led_setup();
	prep_gpib_pins(mode);
	usart_setup();

	wdt_setup();
	init_timers();

	// TODO: Load settings from EEPROM

	// Initialize the GPIB bus
	if (mode) {
		gpib_controller_assign();
	}

	gpio_clear(LED_PORT, LED_ERROR);

	while (1) {
		delay_ms(300);
		gpio_toggle(LED_PORT, LED_STATUS);
	}

	return 0;
}

