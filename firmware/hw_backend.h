#ifndef _HW_BACKEND_H
#define _HW_BACKEND_H

/*
 * hardware back-end and helper functions
 *
 */


#include <stdbool.h>
#include <stdint.h>

#include <libopencm3/cm3/scb.h>

/** Initialize hardware back-end
 *
 * includes peripheral clocks, timers, and IO.
 */
void hw_setup(void);

void delay_us(uint32_t us);

/** delay up to 4294967 ms */
#define delay_ms(x) delay_us((x) * 1000)

/** Get current timestamp in us */
uint32_t get_us(void);

#define reset_cpu scb_reset_system

void prep_gpib_pins(bool mode);

void restart_wdt(void);


/** TODO ? these could be replaced by "EEPROM emulation", see ST AN2594 */
#define write_eeprom(addr, data)
#define read_eeprom(addr) 0

#endif //_HW_BACKEND_H
