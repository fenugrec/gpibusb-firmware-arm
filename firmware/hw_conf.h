#ifndef _HW_CONF_H
#define _HW_CONF_H

/* USB-GPIB hardware configuration
 *
 * Most assignments can be adjusted to match the wiring.
 * Some groups of signals must be on the same GPIO port.
 *
 * By default, this assumes a 75162 IC is used to drive the
 * control lines. Undefine USE_SN75162 if using a 75161; the
 * only relevant difference is the presence of the SC line on the '162.
 *
 * Warning : DIO_PORT and CONTROL_PORT need 5V tolerant pins !
 * FLOW_PORT can be regular 3.3V IO since it's output-only.
 *
 * These settings are for a STM32F072 Discovery board,
 * USART comms
 */

#define USE_SN75162


#define LED_PORT GPIOC
#define LED_ERROR GPIO6
#define LED_STATUS GPIO7


/* GPIB data lines DIO1-DIO8 on PB8-PB15*/
#define DIO_PORT GPIOB
/** write DIO, takes care of inversion */
#define WRITE_DIO(x) gpio_port_write(DIO_PORT, ~((x) << 8));
/** read DIO lines, takes care of inversion */
#define READ_DIO(x) (~gpio_port_read(DIO_PORT) >> 8)

/* GPIB control lines
 * Note, PA11-12 will conflict with USB */
#define CONTROL_PORT GPIOA

#define REN GPIO8
#define EOI GPIO9
#define DAV GPIO10
#define NRFD GPIO11
#define NDAC GPIO12
#define ATN GPIO13
#define SRQ GPIO14
#define IFC GPIO15


/* Direction and output mode controls
 * for the 7516x drivers
 */
#define FLOW_PORT GPIOA

#ifdef USE_SN75162
	#define SC GPIO9
#endif
#define TE GPIO10
#define PE GPIO11
#define DC GPIO12

/********** TIMING */
#define APB_FREQ_MHZ	(48)	//we're going to be running off USB
#define APB_FREQ_HZ	(APB_FREQ_MHZ*1000*1000UL)

/** free-running timing source
 * 32-bit microsecond counter
 */

#define TMR_FREERUN TIM2

#endif //_HW_CONF_H
