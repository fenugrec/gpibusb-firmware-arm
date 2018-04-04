#ifndef _HW_BACKEND_H
#define _HW_BACKEND_H

/*
 * hardware back-end and helper functions
 *
 */


#include <stdint.h>

void delay_us(uint32_t us);

/** delay up to 4294967 ms */
#define delay_ms(x) delay_us((x) * 1000)


#define restart_wdt(x)

#endif _HW_BACKEND_H
