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

#include "hw_conf.h"
#include "hw_backend.h"
#include "stypes.h"

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
	gpio_mode_setup(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_STATUS);
	led_update(LEDPATTERN_OFF);
	return;
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
	u32 tcur = get_ms();

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
			led_status.ts_delta = 500;
			break;
		case LEDPATTERN_ERROR:
			//solid on at least 500ms
			led_status.state_next = 1;
			led_status.ts_delta = 500;
			break;
		case LEDPATTERN_IDLE:
			//slow pulse
			if (led_status.state_next) {
				//ON for 900ms
				led_status.ts_delta = 900;
			} else {
				//then off for 100ms
				led_status.ts_delta = 100;
			}
			led_status.state_next = !led_status.state_next;
			break;
		case LEDPATTERN_ACT:
			//fast blink
			led_status.ts_delta = 100;
			led_status.state_next = !led_status.state_next;
			break;
		default:
			//XXX assert
			break;
	}
}


void prep_gpib_pins(bool controller_mode) {
	/* Flow control pins */
	gpio_clear(FLOW_PORT, TE | PE);
	if (controller_mode) {
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
	gpio_mode_setup(DIO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, DIO_PORTMASK);

	// Set mode and pin state for all GPIB control lines
	if (controller_mode) {
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

#if (TMR_FREERUN != TIM14)
#error some stuff is hardcoded for TIM14 here !
#endif

volatile u32 freerun_ms;

/* Called when systick fires */
void sys_tick_handler(void)
{
	freerun_ms += 1;
	return;
}


static void init_timers(void) {
	rcc_periph_clock_enable(RCC_TIM14);

	/* free-running microsecond counter */
	rcc_periph_reset_pulse(RST_TIM14);
	TIM_CR1(TMR_FREERUN) = 0;	//defaults : upcount, no reload, etc
	TIM_PSC(TMR_FREERUN) = APB_FREQ_MHZ - 1;
	timer_enable_counter(TMR_FREERUN);

	/* utility 1ms periodic systick interrupt. clk source=AHB/8 */
	systick_set_clocksource(STK_CSR_CLKSOURCE_EXT);
	/* clear counter so it starts right away */
	STK_CVR = 0;

	systick_set_reload(rcc_ahb_frequency / (8 * 1000U));
	systick_counter_enable();
	systick_interrupt_enable();
}


void delay_ms(uint16_t ms) {
	u32 t0 = get_ms();
	while ((get_ms() - t0) < ms);
	return;
}


void delay_us(uint16_t us) {
	u16 t0 = TIM_CNT(TMR_FREERUN);
	while ((TIM_CNT(TMR_FREERUN) - t0) < us);
	return;
}

uint16_t get_us(void) {
	return TIM_CNT(TMR_FREERUN);
}

uint32_t get_ms(void) {
	return freerun_ms;
}

/********* WDT
*
* we'll use the IWDG module
*/

#ifdef WITH_WDT
/** Start WDT
 *
 * original firmware has WDT set for 4096 * 4ms = ~ 16 seconds timeout.
 */
static void wdt_setup(void) {
	iwdg_reset();
	iwdg_set_period_ms(5000);
	iwdg_start();
}
#endif // WITH_WDT

void restart_wdt(void) {
	iwdg_reset();
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
	rcc_set_usbclk_source(RCC_PLL);

	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	/* need this to be able to halt the wdt while debugging */
	rcc_periph_clock_enable(RCC_DBGMCU);

#ifdef WITH_WDT
	wdt_setup();
#endif
	init_timers();

	enable_5v(1);
	led_setup();
}
