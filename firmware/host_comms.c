/* Host communications code
 *
 * (c) fenugrec 2018
 *
 * The RX channel (receiving from host) needs at least one FIFO buffer
 * since the USART is running without flow control.
 *
 * To simplify the command parser, we do some initial filtering
 * according to state and received length.
 *
 * Once a complete chunk (either command
 * or data) is received, it gets copied to a command buffer to be
 * parsed by the cmd_parser. XXXX
 */

#include <stdint.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>

#include "host_comms.h"
#include "ring.h"

#include "stypes.h"


/**** globals */
struct ring input_ring;

/**** private */
static struct ring output_ring;

/* Each incoming chunk is saved into the input FIFO.
 *
 * we do minimal filtering, except length check
 * and overflow recovery.
 */
#define UART_BUFFER_SIZE (HOST_IN_BUFSIZE * 3)

static uint8_t output_ring_buffer[HOST_IN_BUFSIZE];
static uint8_t input_ring_buffer[UART_BUFFER_SIZE];

static unsigned in_len;	//length of chunk being received from host

/** Host RX state machine */
enum e_hrx_state {HRX_RX, HRX_RESYNC};
static enum e_hrx_state hrx_state = HRX_RX;
	//HRX_RX while building a chunk
	//HRX_RESYNC after a buffer overflow


/***** funcs */
/** Parse one byte received from host
 *
 * Possibly called from a UART / USB interrupt
 */
static void host_comms_rx(uint8_t rxb);


static void usart_setup(void) {
	rcc_periph_clock_enable(RCC_USART2);

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

void host_comms_init(void) {
	ring_init(&output_ring, output_ring_buffer, sizeof(output_ring_buffer));
	ring_init(&input_ring, input_ring_buffer, sizeof(input_ring_buffer));
	hrx_state = HRX_RX;
	in_len = 0;
	usart_setup();
	return;
}

/** filter and save data
 *
 * Caller checked for overflow and escapes
 */
static void _rx_filter(u8 rxb) {
	if ((rxb == '\r') || (rxb == '\n')) {
		//terminate chunk normally
		ring_write_ch(&input_ring, '\n');
		ring_write_ch(&input_ring, CHUNK_VALID);
		return;
	}
	ring_write_ch(&input_ring, rxb);
	in_len += 1;

	return;
}

static void host_comms_rx(uint8_t rxb) {

	if (in_len == HOST_IN_BUFSIZE) {
		//overflow
		ring_write_ch(&input_ring, '\n');
		ring_write_ch(&input_ring, CHUNK_INVALID);
		hrx_state = HRX_RESYNC;
		return;
	}

	switch (hrx_state) {
	case HRX_RX:
		_rx_filter(rxb);
		break;
	case HRX_RESYNC:
		if ((rxb == '\r') || (rxb == '\n')) {
			//if the host sends both CR+LF, the "LF" will cause
			// an empty command to be parsed. No big deal
			hrx_state = HRX_RX;
			in_len = 0;
		}
		break;
	}
}

void host_tx(uint8_t txb) {
	if (ring_write_ch(&output_ring, txb) == -1) {
		//TODO : overflow
		return;
	}
	usart_enable_tx_interrupt(USART2);	//should trigger TXE instantly ?
	return;
}


void usart2_isr(void) {
	u8 rxbyte;
	u8 txbyte;

	// TODO : check for framing / overrun errors too

	// Check if we were called because of RXNE.
	if (usart_get_flag(USART2, USART_ISR_RXNE)) {
		rxbyte = usart_recv(USART2);
		host_comms_rx(rxbyte);
	}

	// Check if we were called because of TXE.
	if (usart_get_flag(USART2, USART_ISR_TXE)) {
		if (ring_read_ch(&output_ring, &txbyte) == -1) {
			//no more data in buffer.
			usart_disable_tx_interrupt(USART2);
		} else {
			//non-blocking : we'll end up back here anyway
			usart_send(USART2, txbyte);
		}
	}
}
