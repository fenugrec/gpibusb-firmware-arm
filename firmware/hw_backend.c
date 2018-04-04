/*
 * hardware back-end and helper functions
 *
 *
 */

#include <stdint.h>
#include <libopencm3/stm32/timer.h>

#include "hw_conf.h"
#include "stypes.h"

void delay_us(u32 us) {
	u32 t0 = TIM_CNT(TMR_FREERUN);
	while ((TIM_CNT(TMR_FREERUN) - t0) < us);
	return;
}
