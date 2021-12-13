/*
 * hardware back-end and helper functions
 *
 */

#include <stdint.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/iwdg.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/usb/usbd.h>

#include "hw_conf.h"
#include "hw_backend.h"
#include "host_comms.h" //XXX gross, need the host_rx callback
#include "ring.h"
#include "stypes.h"
#include "usb_cdc.h"

/****** IO, GPIO */

/** enable 5V supply for sn75xx drivers.
 *
 * they draw a ton of current and should be disabled
 * when USB is suspended. TODO
 */
static void enable_5v(bool enable) {
	bool set = enable;
#if EN5V_ACTIVEHIGH==0
	//if active-low : reverse polarity
	set = !enable;
#endif // EN5V_ACTIVEHIGH
	if (set) {
		gpio_set(EN5V_PORT, EN5V_PIN);
	} else {
		gpio_clear(EN5V_PORT, EN5V_PIN);
	}
}

static struct {
	u32 ts_last;	//last toggle
	u32 ts_delta;	//interval to next toggle
	enum LED_PATTERN pattern;
	bool state_next;
} led_status = {0};


static void led_setup(void) {
	/* LEDs, active high */
	gpio_set(LED_PORT, LED_ERROR | LED_STATUS);
	gpio_mode_setup(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_STATUS | LED_ERROR);

}

// TODO (higher level): get out of Error state on next good command ?
void led_update(enum LED_PATTERN np) {
	if (led_status.pattern == np) {
		//no change
		return;
	}

	//different pattern : call poll now to take effect before next tdelta
	led_status.pattern = np;
	led_status.ts_delta = 0;	//force
	led_poll();
	return;
}

void led_poll(void) {
	u32 tcur = get_us();

	// check if due for a state change
	if ((tcur - led_status.ts_last) < led_status.ts_delta) {
		return;
	}
	led_status.ts_last = tcur;

	bool set = led_status.state_next;
#if (LED_ACTIVEHIGH == 0)
	//invert logic
	set = !set;
#endif

	//set new LED state
	if (set) {
		gpio_set(LED_PORT, LED_STATUS);
	} else {
		gpio_clear(LED_PORT, LED_STATUS);
	}

	//decide next state and delta according to pattern
	switch (led_status.pattern) {
		case LEDPATTERN_OFF:
			//stay off at least 500ms
			led_status.state_next = 0;
			led_status.ts_delta = 500 * 1000U;
			break;
		case LEDPATTERN_ERROR:
			//solid on at least 500ms
			led_status.state_next = 1;
			led_status.ts_delta = 500 * 1000U;
			break;
		case LEDPATTERN_IDLE:
			//slow pulse
			if (led_status.state_next) {
				//ON for 900ms
				led_status.ts_delta = 900 * 1000U;
			} else {
				//then off for 100ms
				led_status.ts_delta = 100 * 1000U;
			}
			led_status.state_next = !led_status.state_next;
			break;
		case LEDPATTERN_ACT:
			//fast blink
			led_status.ts_delta = 100 * 1000U;
			led_status.state_next = !led_status.state_next;
			break;
		default:
			//XXX assert
			break;
	}
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
	return;
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

void hw_startcomms(void) {
	return;
}

void hw_host_tx(uint8_t txb) {
	if (ring_write_ch(&output_ring, txb) == -1) {
		//TODO : overflow
		return;
	}
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
	return;
}


/********** misc
*/

void reset_cpu(void) {
	scb_reset_system();
}

/* **** global hw setup */
void hw_setup(void) {
	rcc_clock_setup_in_hse_8mhz_out_48mhz();

	rcc_periph_clock_enable(RCC_USB);
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	/* need this to be able to halt the wdt while debugging */
	rcc_periph_clock_enable(RCC_DBGMCU);

	wdt_setup();
	init_timers();

	// USB pins
	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO11 | GPIO12);
	gpio_set_af(GPIOA, GPIO_AF10, GPIO11 | GPIO12);

	enable_5v(1);
	led_setup();
}
