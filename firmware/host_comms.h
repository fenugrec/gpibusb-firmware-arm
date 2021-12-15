#ifndef _HOST_COMMS_H
#define _HOST_COMMS_H

/*
 * These functions are for interacting with the host,
 * and take care of queuing and filtering the data.
 *
 * (c) fenugrec 2018
 *
 */

#include <stdbool.h>
#include <stdint.h>

#include "ecbuff.h"

/* host input beyond this length puts the device in a
 * "resync" mode that just waits for a CR/LF termination,
 * ignoring the data up to then.
 */

#define HOST_IN_BUFSIZE 256
#define HOST_OUT_BUFSIZE 256

/* at the end of each \n-terminated chunk in the
 * FIFO, we place a guard byte. This might be set
 * to INVALID if we detect read errors
 */
#define CHUNK_INVALID 0
#define CHUNK_VALID	1


/** initialize host comms workers
 *
 * should be done before any traffic is sent/received
 */
void host_comms_init(void);


/****************
* data flow :
* from host :
* - USB interrupt calls host_comms_rx() for each byte
* - host_comms_rx() does initial filtering and fills
*   fifo_in.
* - cmd_poll() empties fifo_in
*
*
* to host:
* - code (mostly printf) calls host_tx() or host_tx_m()
* - host_tx() fills fifo_out
* - USB interrupt empties fifo_out
*/


/** callback for data received from host
 *
 * Probably called from an interrupt
 */
void host_comms_rx(uint8_t rxb);

/** FIFO to host */
extern ecbuff *fifo_out;

/** FIFO from host */
extern ecbuff *fifo_in;

/** TX to host: queue one byte
 *
 * manages USART interrupts etc.
 * In case of buffer overflow: TODO
 */
void host_tx(uint8_t txb);

/** queue multiple bytes to send to host
*
* @param len max 256 bytes. extra data is dropped
* overflow : TODO
*/
void host_tx_m(uint8_t *data, unsigned len);

#endif // _HOST_COMMS_H
