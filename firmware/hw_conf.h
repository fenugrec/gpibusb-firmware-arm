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
 */

#define USE_SN75162


#define LED_PORT GPIOA
#define LED_ERROR GPIO9
#define LED_STATUS GPIO5


/* GPIB data lines DIO1-DIO8 on PB0-PB7*/
#define DIO_PORT GPIOB


/* GPIB control lines */
#define CONTROL_PORT GPIOC

#define REN GPIO1
#define EOI GPIO2
#define DAV GPIO3
#define NRFD GPIO4
#define NDAC GPIO5
#define ATN GPIO6
#define SRQ GPIO7
#define IFC GPIO8


/* Direction and output mode controls
 * for the 7516x drivers
 */
#define FLOW_PORT GPIOA

#ifdef USE_SN75162
	#define SC GPIO5
#endif
#define TE GPIO6
#define PE GPIO7
#define DC GPIO8


#endif //_HW_CONF_H
