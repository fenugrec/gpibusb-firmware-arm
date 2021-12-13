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

void prep_gpib_pins(bool mode);

void restart_wdt(void);

/** enable comms with host (interrupts etc). Command parser etc must be init'd first
*/
void hw_startcomms(void);

/** queue byte to send to host
*/
void hw_host_tx(uint8_t txb);

/** queue multiple bytes to send to host
*
* @param len max 256 bytes. extra data is dropped
*/
void hw_host_tx_m(uint8_t *data, unsigned len);

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


/**************** EEPROM
* TODO ? these could be replaced by "EEPROM emulation", see ST AN2594
*/
#define write_eeprom(addr, data)
#define read_eeprom(addr) 0

#endif //_HW_BACKEND_H
