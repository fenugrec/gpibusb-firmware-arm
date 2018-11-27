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
#include <libopencm3/cm3/systick.h>

#include "hw_conf.h"
#include "hw_backend.h"
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
#error timer RCC must be changed !
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
	timer_reset(TMR_FREERUN);
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


/* **** global hw setup */
void hw_setup(void) {
	/* need this to be able to halt the wdt while debugging */
	rcc_periph_clock_enable(RCC_DBGMCU);

	wdt_setup();
	init_timers();

	/* we have stuff on GPIO A,B,C anyway */
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOC);

	/* disable printf buffering so it calls _write every time instead
	 * of waiting for a fflush() or other conditions. This will help keep
	 * the output in sequence.
	 * XXX This will need to be tuned when changing from UART to USB */
	setvbuf(stdout, NULL, _IONBF, 0);

	led_setup();
}
