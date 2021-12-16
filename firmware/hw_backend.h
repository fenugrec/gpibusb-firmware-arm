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

void prep_gpib_pins(bool controller_mode);

void restart_wdt(void);


/**************** IO
*/
enum LED_PATTERN {
	LEDPATTERN_OFF,	// all off
	LEDPATTERN_ERROR,
	LEDPATTERN_IDLE,	//normal ops
	LEDPATTERN_ACT,	//traffic / activity
};

/** set LEDs to given pattern
*/
void led_update(enum LED_PATTERN np);

/** call regularly to udpate patterns
*/
void led_poll(void);


/* Some IO helpers. Original code used these implementations from CCS that
 * implicitly set port direction !
 * These don't need to be used for stuff like LED or the 7516xx driver control signals
 * (PE, TC, DC etc)
 */

/** set pin output HIGH
 *
 * @param gpioport
 * @param gpios : bitmask, can be multiple pins OR'ed
 */
void output_high(uint32_t gpioport, uint16_t gpios);

/** set pin output HIGH
 *
 * @param gpioport
 * @param gpios : bitmask, can be multiple pins OR'ed
 */
void output_low(uint32_t gpioport, uint16_t gpios);


/**************** EEPROM
* TODO ? these could be replaced by "EEPROM emulation", see ST AN2594
*/
#define write_eeprom(addr, data)
#define read_eeprom(addr) 0

#endif //_HW_BACKEND_H
