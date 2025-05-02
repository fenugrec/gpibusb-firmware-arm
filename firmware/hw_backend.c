/*
 * hardware back-end and helper functions
 *
 */

#include <stdint.h>

#include "printf_config.h"  //hax, just to get PRINTF_ALIAS_STANDARD_FUNCTION_NAMES...
#include <printf/printf.h>

#include <libopencm3/stm32/dbgmcu.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/iwdg.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/systick.h>

#include <cmsis_compiler.h> //can't include core_cm0.h because of conflicts with locm's headers

#include "firmware.h"
#include "gpib.h"   //for statemach states
#include "hw_conf.h"
#include "hw_backend.h"
#include "stypes.h"
#include "utils.h"



/** some fwd decls */
enum operatingModes {
	OP_IDLE,
	OP_CTRL,
	OP_DEVI
};

enum transmitModes {
	TM_IDLE,
	TM_RECV,
	TM_SEND
};


static void output_float(uint32_t gpioport, uint16_t gpios);
static void output_setmodes(enum transmitModes mode);
/** set interface mode */
static void setOperatingMode(enum operatingModes mode);



/****** IO, GPIO */

/** enable 5V supply for sn75xx drivers.
 *
 * they draw a ton of current and should be disabled
 * when USB is suspended. TODO
 */
static void enable_5v(bool enable) {
	bool set;
#if EN5V_ACTIVEHIGH
	set = enable
#else
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
	u32 ts_last;    //last toggle
	u32 ts_delta;   //interval to next toggle
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
	led_status.ts_delta = 0;    //force
	led_poll();
	return;
}

void led_poll(void) {
	u32 tcur = get_ms();

	// check if due for a state change
	if (!TS_ELAPSED(tcur, led_status.ts_last, led_status.ts_delta)) {
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
		assert_failed();
		break;
	}
}

#if (EOI_CP != HCTRL1_CP)
#error hardcoded garbage
#endif
/* set gpib pin modes (run once at startup) */
static void output_init(void) {
	output_setmodes(TM_IDLE);
	// open-drain when GPIO drives bus directly ? probably not very correct
	gpio_set_output_options(HCTRL1_CP, GPIO_OTYPE_OD, GPIO_OSPEED_25MHZ, NRFD | NDAC);
	gpio_set_output_options(HCTRL1_CP, GPIO_OTYPE_PP, GPIO_OSPEED_25MHZ, DAV | EOI);
	gpio_set_output_options(HCTRL2_CP, GPIO_OTYPE_PP, GPIO_OSPEED_25MHZ, REN | IFC | ATN | SRQ);
	gpio_set_output_options(DIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_25MHZ, DIO_PORTMASK);
	// Flow port pins will always be outputs
	gpio_clear(FLOW_PORT, TE);
	gpio_set(FLOW_PORT, PE);
#ifdef USE_SN75162
	gpio_mode_setup(FLOW_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, TE | PE | DC | SC);
#else
	gpio_mode_setup(FLOW_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, TE | PE | DC);
#endif
}


// TODO : if these become a bottleneck, they could be changed to
// manipulate ODR and MODER regs directly.

/** set pin output FLOAT (pull-up high)
 *
 * @param gpioport
 * @param gpios : bitmask, can be multiple pins OR'ed
 */
static void output_float(uint32_t gpioport, uint16_t gpios) {
	gpio_mode_setup(gpioport, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, gpios);
}

/* set DIO pins to input */
void dio_float(void) {
	// clearing MODER bits (2 bits per GPIO) configures as inputs
	GPIO_MODER(DIO_PORT) &= ~(u32) DIO_MODEMASK;
}

/* set DIO pins to output */
void dio_output(void) {
	// MODER bits (2 bits per GPIO)=b'01' configures as inputs
	u32 tmp = GPIO_MODER(DIO_PORT);
	tmp &= ~(u32) DIO_MODEMASK;
	GPIO_MODER(DIO_PORT) = tmp | DIO_MODEMASK_OUTPUTS;
}

/* set all control signals to hi-Z */
static void clearAllSignals(void) {
	output_float(HCTRL1_CP, EOI | DAV | NRFD | NDAC);
	output_float(HCTRL2_CP, ATN | IFC | SRQ | REN);
}


static void setOperatingMode(enum operatingModes mode) {
	switch (mode) {
	case OP_IDLE:
		output_float(HCTRL2_CP, ATN | IFC | SRQ | REN);
		break;
	case OP_CTRL:
		// Signal IFC, REN and ATN, listen to SRQ
		gpio_set(HCTRL2_CP, IFC | REN | ATN);
		gpio_mode_setup(HCTRL2_CP, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, SRQ);
		gpio_mode_setup(HCTRL2_CP, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, IFC | REN | ATN);
		break;
	case OP_DEVI:
		// Signal SRQ, listen to IFC, REN and ATN
		gpio_set(HCTRL2_CP, SRQ);
		gpio_mode_setup(HCTRL2_CP, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, SRQ);
		gpio_mode_setup(HCTRL2_CP, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, IFC | REN | ATN);
		break;
	default:
		assert_basic(0);
		break;
	}
}


// XXX temp macro conversion for code copypasta'd from AR488
// 75160 (DIO) has TE + PE signals
// 75161 (control) has TE + DC
// 75162 (control) has TE + DC + SC
#define SN7516X
#define SN7516X_DC
#ifdef USE_75162
	#define SN7516X_SC
#endif

/***** Control the GPIB bus - set various GPIB states *****/
void setControls(enum gpib_states gs) {
	static enum gpib_states gpibstate = CINI;
	if (gpibstate == gs) {
		return;
	}
	DEBUG_PRINTF("gpibstate %s => %s\n", gpib_states_s[gpibstate], gpib_states_s[gs]);
	switch (gs) {
	case CINI:      // Initialisation
		setOperatingMode(OP_CTRL);
		output_setmodes(TM_IDLE);
		assert_signal(HCTRL2_CP, REN);
#ifdef SN7516X
		gpio_clear(FLOW_PORT, TE);
#ifdef SN7516X_DC
		gpio_clear(FLOW_PORT, DC);
#endif
#ifdef SN7516X_SC
		gpio_set(FLOW_PORT, SC);
#endif
#endif
		break;

	case CIDS:      // Controller idle state
		unassert_signal(HCTRL2_CP, ATN);
		output_setmodes(TM_IDLE);
#ifdef SN7516X
		gpio_clear(FLOW_PORT, TE);
#endif
		break;

	case CCMS:      // Controller active - send commands
		output_setmodes(TM_SEND);
		assert_signal(HCTRL2_CP, ATN);
#ifdef SN7516X
		gpio_set(FLOW_PORT, TE);
#endif
		break;


	case CLAS:      // Controller - read data bus
		// Set state for receiving data
		unassert_signal(HCTRL2_CP, ATN);
		output_setmodes(TM_RECV);
#ifdef SN7516X
		gpio_clear(FLOW_PORT, TE);
#endif
		break;


	case CTAS:      // Controller - write data bus
		unassert_signal(HCTRL2_CP, ATN);
		output_setmodes(TM_SEND);
#ifdef SN7516X
		gpio_set(FLOW_PORT, TE);
#endif
		break;

	case DINI:      // Listner initialisation
#ifdef SN7516X
		gpio_set(FLOW_PORT, TE);
#ifdef SN7516X_DC
		gpio_set(FLOW_PORT, DC);
#endif
#ifdef SN7516X_SC
		gpio_clear(FLOW_PORT, SC);
#endif
#endif
		clearAllSignals();
		setOperatingMode(OP_DEVI);      // Set up for device mode
		// Set data bus to idle state
		dio_float();
		break;

	case DIDS:      // Device idle state
#ifdef SN7516X
		gpio_set(FLOW_PORT, TE);
#endif
		output_setmodes(TM_IDLE);
		// Set data bus to idle state
		dio_float();
		break;


	case DLAS:      // Device listner active (actively listening - can handshake)
#ifdef SN7516X
		gpio_clear(FLOW_PORT, TE);
#endif
		output_setmodes(TM_RECV);
		break;


	case DTAS:      // Device talker active (sending data)
#ifdef SN7516X
		gpio_set(FLOW_PORT, TE);
#endif
		output_setmodes(TM_SEND);
		break;
	default:
		assert_basic(0);
	}

	gpibstate = gs;
}


/***** Set the transmission mode *****/
static void output_setmodes(enum transmitModes mode) {
	static enum transmitModes txmode_current = TM_IDLE;
	if (mode == txmode_current) {
		return;
	}
	txmode_current = mode;
	switch (mode) {
	case TM_IDLE:
		gpio_mode_setup(HCTRL1_CP, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, EOI | DAV | NRFD | NDAC);
		break;
	case TM_RECV:
		gpio_clear(HCTRL1_CP, NRFD | NDAC);
		gpio_mode_setup(HCTRL1_CP, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, EOI | DAV);
		gpio_mode_setup(HCTRL1_CP, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, NRFD | NDAC);
		break;
	case TM_SEND:
		gpio_set(HCTRL1_CP, EOI | DAV);
		gpio_mode_setup(HCTRL1_CP, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, NRFD | NDAC);
		gpio_mode_setup(HCTRL1_CP, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, EOI | DAV);
		break;
	}
}


void assert_signal(uint32_t gpioport, uint16_t gpios) {
	gpio_clear(gpioport, gpios);
}

void unassert_signal(uint32_t gpioport, uint16_t gpios) {
	gpio_set(gpioport, gpios);
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
	TIM_CR1(TMR_FREERUN) = 0;   //defaults : upcount, no reload, etc
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
	while (!TS_ELAPSED(get_ms(), t0, ms));
	return;
}


void delay_us(uint16_t us) {
	u16 t0 = TIM_CNT(TMR_FREERUN);
	while (!TS_ELAPSED(TIM_CNT(TMR_FREERUN), t0, us));
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


/** Start WDT
 *
 * original firmware has WDT set for 4096 * 4ms = ~ 16 seconds timeout.
 */
static void wdt_setup(void) {
	DBGMCU_CR  |= DBGMCU_CR_IWDG_STOP;
#ifdef WITH_WDT
	iwdg_reset();
	iwdg_set_period_ms(5000);
	iwdg_start();
#endif
}


void restart_wdt(void) {
	iwdg_reset();
}

/********** misc
*/

void reset_cpu(void) {
	scb_reset_system();
}

#define DFU_MAGIC 0x5555AAAA
/* these are very device-specific, and not in any locm3/CMSIS/ST headers ? */
//#define SYSMEM_BASE 0x1fffc400	//good for F070x6
#define SYSMEM_BASE 0x1fffc800  // F070xB

/* special struct stored in a RAM area not cleared at reset
 *
 */
static struct {
	u32 dfu_token;  //alternately, if pre_main() runs before BSS is cleared, dfu_token can be in regular ram.
	int assert_reason;  //cleared manually on POR
	char reset_reason;
} sys_state  __attribute__ ((section (".svram")));


void reset_dfu(void) {
	sys_state.dfu_token = DFU_MAGIC;
	scb_reset_system();
}


/* could go in struct sys_state, but then wouldn't be cleared with BSS...
 * Access needs to be interrupt-safe
 */
static struct {
	unsigned tx_ovf;    //# of bytes dropped due to overflow (to host)
	unsigned rx_ovf; // (from host)
} stats = {0};

void sys_incstats(enum stats_type st) {
	bool i = disable_irq();
	switch (st) {
	case STATS_TXOVF:
		stats.tx_ovf++;
		break;
	case STATS_RXOVF:
		stats.rx_ovf++;
		break;
	default:
		break;
	}
	restore_irq(i);
}

void sys_printstats(void) {
	unsigned rx_ovf, tx_ovf;
	bool i = disable_irq();
	rx_ovf = stats.rx_ovf;
	tx_ovf = stats.tx_ovf;
	restore_irq(i);

	printf("last reset: %c\nlast error: %i\ntxovf: %u, rxovf: %u\n", \
		   (char) sys_state.reset_reason, sys_state.assert_reason, tx_ovf, rx_ovf);
	return;
}

/********** assert stuff **********/
void  assert_failed(void) {
	__disable_irq();
	__BKPT(0);
	while (1);
}

void assert_failed_v(int reason) {
	sys_state.assert_reason = reason;
	assert_failed();
}



void pre_main(void) {
	// check reset reason.
	u32 csr_tmp = RCC_CSR;
	if (csr_tmp & RCC_CSR_PORRSTF) {
		sys_state.reset_reason = 'P';
		sys_state.assert_reason = E_OK;
	} else if (csr_tmp & RCC_CSR_SFTRSTF) {
		// reset into/out of DFU would report this
		sys_state.reset_reason = 'S';
	} else if (csr_tmp & RCC_CSR_IWDGRSTF) {
		sys_state.reset_reason = 'I';
	} else {
		// nRST toggled
		sys_state.reset_reason = 'N';
	}
	RCC_CSR |= RCC_CSR_RMVF;    //clear reset reason flags

	if (sys_state.dfu_token != DFU_MAGIC) return;
	sys_state.dfu_token = 0;

	void (*bootloader)(void) = (void (*)(void)) (*((uint32_t *) (SYSMEM_BASE + 4)));

	__set_MSP(*(uint32_t*) SYSMEM_BASE);
	bootloader();

	while (1);
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

	wdt_setup();

	init_timers();

	output_init();
	enable_5v(1);
	led_setup();
}
