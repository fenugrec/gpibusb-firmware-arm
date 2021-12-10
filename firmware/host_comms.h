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

#include "ring.h"

/* host input beyond this length puts the device in a
 * "resync" mode that just waits for a CR/LF termination,
 * ignoring the data up to then.
 */

#define HOST_IN_BUFSIZE 256

/* at the end of each \n-terminated chunk in the
 * FIFO, we place a guard byte. This might be set
 * to INVALID if we detect read errors
 */
#define CHUNK_INVALID 0
#define CHUNK_VALID	1


/** initialize host comms workers
 *
 * This also starts receiving data from host
 */
void host_comms_init(void);

/** callback for data received from host
 *
 * Possibly called from a UART / USB interrupt
 */
void host_comms_rx(uint8_t rxb);

/** RX from host: sink can poll ring_read_ch() */
extern struct ring input_ring;

/** TX to host: queue one byte
 *
 * manages USART interrupts etc.
 * In case of buffer overflow: TODO
 */
void host_tx(uint8_t txb);

/** TX to host: queue multiple bytes
 *
 * if overflow : TODO
 */
void host_tx_m(uint8_t *data, unsigned len);

#endif // _HOST_COMMS_H
