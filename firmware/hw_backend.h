#ifndef _HW_BACKEND_H
#define _HW_BACKEND_H

/*
 * hardware back-end and helper functions
 *
 */


#include <stdbool.h>
#include <stdint.h>

/** Initialize hardware back-end
 *
 * includes peripheral clocks, timers, and IO.
 */
void hw_setup(void);

void delay_us(uint16_t us);

void delay_ms(uint16_t ms);

/** get current timestamp in ms
 *
 * not interrupt-safe
 */
uint32_t get_ms(void);

/** Get current timestamp in us */
uint16_t get_us(void);

void reset_cpu(void);

/** restart in DFU mode */
void reset_dfu(void);

/** must be called as early as possible */
void pre_main(void);

void restart_wdt(void);

/* why does gcc not have a builtin for this ? */
static __inline__ uint32_t get_pc(void)  {
	uint32_t pc;
	asm ("mov %0, pc" : "=r" (pc));
	return pc;
}


enum stats_type {
	STATS_RXOVF,
	STATS_TXOVF
};

/** increment stats counter
 *
 * TODO : make interrupt-safe
 */
void sys_incstats(enum stats_type);

/** print some system info & stats */
void sys_printstats(void);

/**************** IO
*/
enum LED_PATTERN {
	LEDPATTERN_OFF, // all off
	LEDPATTERN_ERROR,
	LEDPATTERN_IDLE,    //normal ops
	LEDPATTERN_ACT, //traffic / activity
};

/** set LEDs to given pattern
*/
void led_update(enum LED_PATTERN np);

/** call regularly to udpate patterns
*/
void led_poll(void);


/**************** GPIB / IO stuff */

enum transmitModes; //in gpib.h
enum operatingModes;
enum gpib_states;


/** sets transceiver pins etc */
void setControls(enum gpib_states gs);

/** set DIO pins to input */
void dio_float(void);

/** set DIO pins to output */
void dio_output(void);

/** assert (in the GPIB sense) one signal
 * assumes port direction already OK
 */
void assert_signal(uint32_t gpioport, uint16_t gpios);

/** unassert (in the GPIB sense) one signal
 * assumes port direction already OK
 */
void unassert_signal(uint32_t gpioport, uint16_t gpios);

/**************** EEPROM
* TODO ? these could be replaced by "EEPROM emulation", see ST AN2594
*/
#define write_eeprom(addr, data)
#define read_eeprom(addr) 0

#endif //_HW_BACKEND_H
