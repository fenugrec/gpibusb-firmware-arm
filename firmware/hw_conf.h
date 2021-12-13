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
 */

/*
 * These settings are for hw-1.00, stm32F070C6 / 70CB
 *
 *
 * Mapping:
 *
 * GPIOA
 * 0 : active low enable_5V
 * 2 : LED
 * 4,5,6 : FLOW_PORT.{TE,PE,DC} (3V3)
 * 8,9,10 : CONTROL_PORT_1 {DAV,NRFD,NDAC} (5V)
 * 11,12 : reserved for USB
 * 13,14 : SWD/SWCLK for debugging
 * 15 : CONTROL_PORT_1 {EOI} (5V)
 *
 * GPIOB	Can't use PB0:1 because not 5Vtol !!
 * 2,3,4,5 : CONTROL_PORT_2 {REN,IFC,ATN,SRQ} (5V)
 * 8-15 : DIO to GPIB (5V).
 *
 */

#undef USE_SN75162

#define EN5V_PORT GPIOA
#define EN5V_PIN GPIO0
#define EN5V_ACTIVEHIGH 0	//= active low


#define LED_PORT GPIOA
#define LED_ERROR GPIO2
#define LED_STATUS GPIO2


/* GPIB data lines DIO1-DIO8 on PB8-15 */
#define DIO_PORT GPIOB
#define DIO_PORTSHIFT 8
/** write DIO, takes care of inversion */
#define WRITE_DIO(x) gpio_port_write(DIO_PORT, ~(x) << DIO_PORTSHIFT)
/** read DIO lines, takes care of inversion */
#define READ_DIO(x) ((~gpio_port_read(DIO_PORT) >> DIO_PORTSHIFT) & 0xFF)

/* GPIB control lines
 * super messy
 *
 */
#define DAV_CP GPIOA
#define NRFD_CP GPIOA
#define NDAC_CP GPIOA	//NRFD and NDAC must be on the same port
#define EOI_CP GPIOA	//EOI and DAV must be on the same port

#define DAV GPIO8
#define NRFD GPIO9
#define NDAC GPIO10
#define EOI GPIO15

#define REN_CP GPIOB
#define IFC_CP GPIOB
#define ATN_CP GPIOB
#define SRQ_CP GPIOB

#define REN GPIO2
#define IFC GPIO3
#define ATN GPIO4
#define SRQ GPIO5


/* Direction and output mode controls
 * for the 7516x drivers
 */
#define FLOW_PORT GPIOA

#define TE GPIO4
#define PE GPIO5
#define DC GPIO6

// timing
#define APB_FREQ_MHZ	(48)	//we're going to be running off USB
#define APB_FREQ_HZ	(APB_FREQ_MHZ*1000*1000UL)



#if 0
/*
 * These settings are for a STM32F072 Discovery board,
 * USART comms.
 *
 *
 * Mapping:
 *
 * GPIOA
 * 1 : FLOW_PORT.SC (3V3)
 * 2,3 : UART (3V3)
 * 4,5,6 : FLOW_PORT.{DC,TE,PE} (3V3)
 * 8,9,10 : CONTROL_PORT_1 (5V)
 * 11,12 : will conflict with USB
 * 13,14 : SWD/SWCLK for debugging
 * 15 : CONTROL_PORT_1 (5V)
 *
 * GPIOB
 * 2-9 : DIO to GPIB (5V). Can't use PB0:1 because not 5Vtol !!
 * 8-15 : 5Vtol, but unusable because the disco board has a 3V-only part hardwired to PB10-12;
 *			also we write DIO to GPIO_ODR directly, clobbering the unused bits.
 *
 * GPIOC
 * 6,7 : LEDs
 * 8,10,11,12 : CONTROL_PORT_2 (5V)
 */

#define USE_SN75162


#define LED_PORT GPIOC
#define LED_ERROR GPIO6
#define LED_STATUS GPIO7


/* GPIB data lines DIO1-DIO8 on PB2-PB9 */
#define DIO_PORT GPIOB
#define DIO_PORTSHIFT 2
/** write DIO, takes care of inversion */
#define WRITE_DIO(x) gpio_port_write(DIO_PORT, ~(x) << DIO_PORTSHIFT)
/** read DIO lines, takes care of inversion */
#define READ_DIO(x) ((~gpio_port_read(DIO_PORT) >> DIO_PORTSHIFT) & 0xFF)

/* GPIB control lines
 * super messy, in order to work on the f072 disco board..
 *
 */
#define DAV_CP GPIOA
#define NRFD_CP GPIOA
#define NDAC_CP GPIOA	//NRFD and NDAC must be on the same port
#define EOI_CP GPIOA	//EOI and DAV must be on the same port

#define DAV GPIO8
#define NRFD GPIO9
#define NDAC GPIO10
#define EOI GPIO15

#define REN_CP GPIOC
#define IFC_CP GPIOC
#define ATN_CP GPIOC
#define SRQ_CP GPIOC

#define REN GPIO8	//will also drive an LED, incidentally...
#define IFC GPIO10
#define ATN GPIO11
#define SRQ GPIO12


/* Direction and output mode controls
 * for the 7516x drivers
 */
#define FLOW_PORT GPIOA

#ifdef USE_SN75162
	#define SC GPIO1
#endif
#define TE GPIO4
#define PE GPIO5
#define DC GPIO6

/********** TIMING */
#define APB_FREQ_MHZ	(48)	//we're going to be running off USB
#define APB_FREQ_HZ	(APB_FREQ_MHZ*1000*1000UL)

#endif	//f072disco settings

/** free-running timing source
 * 32-bit microsecond counter
 */

#define TMR_FREERUN TIM2

#endif //_HW_CONF_H
