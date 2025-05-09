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
 * PE on the 75160 changes behavior of the DIO port wrt pull-up / open-collector output;
 * apparently to allow parallel poll (this is rarely supported; none of the instruments below have it)
 * Commercial instruments drive PE in different ways:
 * - tie PE to 0 : Advantest MS2601, Fluke 2620, Keithley 199, 236, 705; HP 6060, HP 3478
 * - tie PE to 1 : Fluke PM6680, HP 34970,34401, 8590*
 * - PE = (ATN | EOI) : Keithley 7001, Tek 744, HP 864x
 * - PE controlled by 7210/9914 controller IC: Marconi 2023, Anritsu MS2601, Xitron 2000
 *
 * As for controllers, I have less info, but
 * - PE tied to 1 in the reverse-engineered 82357B clone
 * - PE controlled by 7210 on old NI PCIIA ISA card
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
 * 0 : active low enable_5V (XXX will need to move to PB6 for hw-1.01)
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

#define EN5V_PORT		GPIOA
#define EN5V_PIN		GPIO0
#define EN5V_ACTIVEHIGH 0   //= active low


#define LED_PORT	   GPIOA
#define LED_ERROR	   GPIO2
#define LED_STATUS	   GPIO2
#define LED_ACTIVEHIGH 0    //=active low


/* GPIB data lines DIO1-DIO8 on PB8-15 */
#define DIO_PORT			 GPIOB
#define DIO_PORTSHIFT		 8
#define DIO_PORTMASK		 (0xFF << DIO_PORTSHIFT)
#define DIO_MODEMASK		 (0xFFFF << (2*DIO_PORTSHIFT))  // MODER fields are 2 bits per gpio !
#define DIO_MODEMASK_OUTPUTS (0x5555 << (2*DIO_PORTSHIFT))  //value to set pin dir to Output

/** write DIO, takes care of inversion.
 * also need to mask other bits because some control signals are also on GPIOB
 */
#define WRITE_DIO(x) gpio_port_write(DIO_PORT, \
									 (GPIO_ODR(DIO_PORT) & ~DIO_PORTMASK) | (~(x) << DIO_PORTSHIFT))

/** read DIO lines, takes care of inversion */
#define READ_DIO(x) ((~gpio_port_read(DIO_PORT) >> DIO_PORTSHIFT) & 0xFF)

/* GPIB control lines
 * super messy. Some hardcoded stuff in hw_backend.c
 *
 * NRFD and NDAC must be on the same port;
 * EOI and DAV must be on the same port;
 * REN,IFC,ATN,SRQ must be on same port
 */
#define HCTRL1_CP GPIOA     //handshake + EOI
#define EOI_CP	  HCTRL1_CP

#define DAV	 GPIO8
#define NRFD GPIO9
#define NDAC GPIO10
#define EOI	 GPIO15


#define HCTRL2_CP GPIOB

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
#define APB_FREQ_MHZ (48)       //we're going to be running off USB
#define APB_FREQ_HZ	 (APB_FREQ_MHZ*1000*1000UL)



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


#define LED_PORT   GPIOC
#define LED_ERROR  GPIO6
#define LED_STATUS GPIO7


/* GPIB data lines DIO1-DIO8 on PB2-PB9 */
#define DIO_PORT	  GPIOB
#define DIO_PORTSHIFT 2
/** write DIO, takes care of inversion */
#define WRITE_DIO(x) gpio_port_write(DIO_PORT, ~(x) << DIO_PORTSHIFT)
/** read DIO lines, takes care of inversion */
#define READ_DIO(x) ((~gpio_port_read(DIO_PORT) >> DIO_PORTSHIFT) & 0xFF)

/* GPIB control lines
 * super messy, in order to work on the f072 disco board..
 *
 */
#define HCTRL1_CP GPIOA
#define HCTRL1_CP GPIOA
#define HCTRL1_CP GPIOA   //NRFD and NDAC must be on the same port
#define EOI_CP	  GPIOA //EOI and DAV must be on the same port

#define DAV	 GPIO8
#define NRFD GPIO9
#define NDAC GPIO10
#define EOI	 GPIO15

#define HCTRL2_CP GPIOC

#define REN GPIO8     //will also drive an LED, incidentally...
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
#define APB_FREQ_MHZ (48)       //we're going to be running off USB
#define APB_FREQ_HZ	 (APB_FREQ_MHZ*1000*1000UL)

#endif  //f072disco settings

/** free-running 16-bit microsecond counter
 *
 * the F0 doesn't have any 32-bit counters !
 */

#define TMR_FREERUN TIM14

#endif //_HW_CONF_H
