#ifndef _HW_BACKEND_H
#define _HW_BACKEND_H

/*
 * hardware back-end and helper functions
 *
 */


#include <stdbool.h>
#include <stdint.h>

#include <libopencm3/cm3/scb.h>


/** Initialize timers
 */
void init_timers(void);

void delay_us(uint32_t us);

/** delay up to 4294967 ms */
#define delay_ms(x) delay_us((x) * 1000)



#define reset_cpu scb_reset_system

void led_setup(void);
void prep_gpib_pins(bool mode);

/** Start WDT
 *
 * original firmware has WDT set for 4096 * 4ms = ~ 16 seconds timeout.
 */
void wdt_setup(void);
void restart_wdt(void);


/** TODO ? these could be replaced by "EEPROM emulation", see ST AN2594 */
#define write_eeprom(addr, data)
#define read_eeprom(addr) 0

#endif //_HW_BACKEND_H
