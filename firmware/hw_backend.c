/*
 * hardware back-end and helper functions
 *
 *
 */

#include <stdint.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/iwdg.h>
#include <libopencm3/stm32/rcc.h>

#include "hw_conf.h"
#include "hw_backend.h"
#include "stypes.h"

void led_setup(void) {
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
	gpio_mode_setup(DIO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, 0xFF);

	// Set mode and pin state for all GPIB control lines
	if (mode) {
		gpio_mode_setup(CONTROL_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE,
						EOI | DAV | SRQ);
		gpio_set(CONTROL_PORT, ATN | IFC);
		gpio_clear(CONTROL_PORT, NRFD | NDAC | REN);
		gpio_mode_setup(CONTROL_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
						ATN | NRFD | NDAC | IFC | REN);
	} else {
		gpio_mode_setup(CONTROL_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE,
						ATN | EOI | DAV | NRFD | NDAC | IFC | SRQ | REN);
	}

}

/********* TIMERS
 *
 *
 */

#if (TMR_FREERUN != TIM2)
#error timer RCC must be changed !
#endif

void init_timers(void) {
	rcc_periph_clock_enable(RCC_TIM2);

	timer_reset(TMR_FREERUN);
	TIM_CR1(TMR_FREERUN) = 0;	//defaults : upcount, no reload, etc
	TIM_PSC(TMR_FREERUN) = APB_FREQ_MHZ - 1;
	timer_enable_counter(TMR_FREERUN);
}

void delay_us(u32 us) {
	u32 t0 = TIM_CNT(TMR_FREERUN);
	while ((TIM_CNT(TMR_FREERUN) - t0) < us);
	return;
}



/********* WDT
*
* we'll use the IWDG module
*/

void wdt_setup(void) {
	iwdg_reset();
	iwdg_set_period_ms(5000);
	iwdg_start();
}

void restart_wdt(void) {
	iwdg_reset();
}
