/*
 * hardware back-end and helper functions
 *
 * Arguably the USART code would belong here, but it's a bit
 * too tightly integrated in host_comms.c
 *
 */

#include <stdint.h>
#include <stdio.h>


#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/iwdg.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/usart.h>

#include "hw_conf.h"
#include "hw_backend.h"
#include "host_comms.h" //XXX gross, need the host_rx callback
#include "ring.h"
#include "stypes.h"

/****** IO, GPIO */

static void led_setup(void) {
	/* LEDs, active high */
	gpio_set(LED_PORT, LED_ERROR | LED_STATUS);
	gpio_mode_setup(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_STATUS | LED_ERROR);

}

void prep_gpib_pins(bool mode) {
	/* Flow control pins */
	gpio_clear(FLOW_PORT, TE | PE);
	if (mode) {
#ifdef USE_SN75162
		gpio_set(FLOW_PORT, SC); // TX on REN and IFC
#endif
		gpio_clear(FLOW_PORT, DC); // TX on ATN and RX on SRQ
	} else {
#ifdef USE_SN75162
		gpio_clear(FLOW_PORT, SC);
#endif
		gpio_set(FLOW_PORT, DC);
	}

	// Flow port pins will always be outputs

#ifdef USE_SN75162
	gpio_mode_setup(FLOW_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, TE | PE | DC | SC);
#else
	gpio_mode_setup(FLOW_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, TE | PE | DC);
#endif


	// Float all DIO lines
	gpio_mode_setup(DIO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, 0xFF << DIO_PORTSHIFT);

	// Set mode and pin state for all GPIB control lines
	if (mode) {
		gpio_mode_setup(EOI_CP, GPIO_MODE_INPUT, GPIO_PUPD_NONE, EOI | DAV);
		gpio_mode_setup(SRQ_CP, GPIO_MODE_INPUT, GPIO_PUPD_NONE, SRQ);
		gpio_set(ATN_CP, ATN | IFC);
		gpio_clear(NRFD_CP, NRFD | NDAC);
		gpio_clear(REN_CP, REN);
		gpio_mode_setup(REN_CP, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
						ATN | IFC | REN);
		gpio_mode_setup(NRFD_CP, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
						NRFD | NDAC);
	} else {
		gpio_mode_setup(REN_CP, GPIO_MODE_INPUT, GPIO_PUPD_NONE,
						ATN | IFC | SRQ | REN);
		gpio_mode_setup(EOI_CP, GPIO_MODE_INPUT, GPIO_PUPD_NONE,
						EOI | DAV | NRFD | NDAC);
	}

}

/********* TIMERS
 *
 *
 */

#if (TMR_FREERUN != TIM2)
#error some stuff is hardcoded for TIM2 here !
#endif

/* Called when systick fires */
void sys_tick_handler(void)
{
	static unsigned ms = 0;
	ms++;
	if (ms > 300) {
		gpio_toggle(LED_PORT, LED_STATUS);
		ms = 0;
	}
}


static void init_timers(void) {
	rcc_periph_clock_enable(RCC_TIM2);

	/* free-running microsecond counter */
	rcc_periph_reset_pulse(RST_TIM2);
	TIM_CR1(TMR_FREERUN) = 0;	//defaults : upcount, no reload, etc
	TIM_PSC(TMR_FREERUN) = APB_FREQ_MHZ - 1;
	timer_enable_counter(TMR_FREERUN);

	/* utility 1ms periodic systick interrupt. clk source=AHB/8 */
	systick_set_clocksource(STK_CSR_CLKSOURCE_EXT);
	/* clear counter so it starts right away */
	STK_CVR = 0;

	systick_set_reload(rcc_ahb_frequency / 8 / 1000);
	systick_counter_enable();
	systick_interrupt_enable();
}

void delay_us(u32 us) {
	u32 t0 = TIM_CNT(TMR_FREERUN);
	while ((TIM_CNT(TMR_FREERUN) - t0) < us);
	return;
}

uint32_t get_us(void) {
	return TIM_CNT(TMR_FREERUN);
}


/********* WDT
*
* we'll use the IWDG module
*/

/** Start WDT
 *
 * original firmware has WDT set for 4096 * 4ms = ~ 16 seconds timeout.
 */
static void wdt_setup(void) {
	iwdg_reset();
	iwdg_set_period_ms(5000);
	iwdg_start();
}

void restart_wdt(void) {
	iwdg_reset();
}


/********* USART
*/

static void usart_setup(void) {
	rcc_periph_clock_enable(RCC_USART2);

	/* A2 = TX, A3 = RX */
	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO2 | GPIO3);
	gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_25MHZ, GPIO2);
	gpio_set_af(GPIOA, GPIO_AF1, GPIO2 | GPIO3);

	usart_set_baudrate(USART2, 115200);
	usart_set_databits(USART2, 8);
	usart_set_stopbits(USART2, USART_STOPBITS_1);
	usart_set_mode(USART2, USART_MODE_TX_RX);
	usart_set_parity(USART2, USART_PARITY_NONE);
	usart_set_flow_control(USART2, USART_FLOWCONTROL_NONE);
}


void hw_startcomms(void) {
	usart_enable(USART2);
	//purge possibly garbage byte and error flags
	usart_recv(USART2);
	USART2_ICR = USART_ICR_ORECF | USART_ICR_NCF | USART_ICR_FECF | USART_ICR_PECF;

	usart_enable_rx_interrupt(USART2);
	nvic_enable_irq(NVIC_USART2_IRQ);
}


/**** private */
static struct ring output_ring;
static uint8_t output_ring_buffer[HOST_IN_BUFSIZE];

void hw_host_tx(uint8_t txb) {
	if (ring_write_ch(&output_ring, txb) == -1) {
		//TODO : overflow
		return;
	}
	usart_enable_tx_interrupt(USART2);	//should trigger TXE instantly ?
	return;
}

void hw_host_tx_m(uint8_t *data, unsigned len) {
	if (len > 256) {
		len = 256;
	}
	if (ring_write(&output_ring, data, (ring_size_t) len) != (ring_size_t) len) {
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


/********** misc
*/

void reset_cpu(void) {
	scb_reset_system();
}

/* **** global hw setup */
void hw_setup(void) {
	/* need this to be able to halt the wdt while debugging */
	rcc_periph_clock_enable(RCC_DBGMCU);

	wdt_setup();
	init_timers();

	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);

	/* disable printf buffering so it calls _write every time instead
	 * of waiting for a fflush() or other conditions. This will help keep
	 * the output in sequence.
	 * XXX This will need to be tuned when changing from UART to USB */
	setvbuf(stdout, NULL, _IONBF, 0);

	ring_init(&output_ring, output_ring_buffer, sizeof(output_ring_buffer));
	usart_setup();

	led_setup();
}
