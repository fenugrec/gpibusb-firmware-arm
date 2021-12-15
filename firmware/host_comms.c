/* Host communications code
 *
 * (c) fenugrec 2018-2021
 * GPLv3
 *
 * The RX channel (receiving from host) needs a FIFO buffer
 *
 * To simplify the command parser, we do some initial filtering
 * according to state and received length.
 *
 * Once a complete chunk (either command
 * or data) is received, it gets copied to a command buffer to be
 * parsed by the cmd_parser.
 */

#include <stdint.h>

#include "hw_backend.h"
#include "host_comms.h"
#include "ecbuff.h"

#include "stypes.h"


/**** globals */


/* incoming data is saved into the input FIFO, after
 * minimal filtering, length check
 * and overflow recovery.
 */
static _Alignas(ecbuff) uint8_t fifo_in_buf[sizeof(ecbuff) + HOST_IN_BUFSIZE];
ecbuff *fifo_in = (ecbuff *) fifo_in_buf;

/* FIFO of data to host */
static _Alignas(ecbuff) uint8_t fifo_out_buf[sizeof(ecbuff) + HOST_OUT_BUFSIZE];
ecbuff *fifo_out = (ecbuff *) fifo_out_buf;



static unsigned in_len;	//length of chunk being received from host

/** Host RX state machine */
enum e_hrx_state {HRX_RX, HRX_ESCAPE, HRX_RESYNC};
static enum e_hrx_state hrx_state = HRX_RX;
	//HRX_RX while building a chunk
	//HRX_ESCAPE will not end the chunk if the next byte is \r or \n
	//HRX_RESYNC after a buffer overflow


/***** funcs */


void host_comms_init(void) {
	ecbuff_init(fifo_in, HOST_IN_BUFSIZE, 1);
	ecbuff_init(fifo_out, HOST_OUT_BUFSIZE, 1);

	hrx_state = HRX_RX;
	in_len = 0;
	return;
}

/** filter and save data.
 *
 * Checks for overflow, skips over escaped characters, and
 * appends a CHUNK_VALID / _INVALID guard byte after each chunk.
 *
 * Does not distinguish between data or commands.
 * Converts unescaped \r (CR) to \n
 */
void host_comms_rx(uint8_t rxb) {
	const u8 lf = '\n';
	const u8 chunkvalid = CHUNK_VALID;	//gross
	const u8 chunkinvalid = CHUNK_INVALID;	//gross

	if (in_len == HOST_IN_BUFSIZE) {
		//overflow

		ecbuff_write(fifo_in, &lf);
		ecbuff_write(fifo_in, &chunkinvalid);
		hrx_state = HRX_RESYNC;
		return;
	}

	switch (hrx_state) {
	case HRX_RX:
		if (rxb == 27) {
			hrx_state = HRX_ESCAPE;
		} else 	if ((rxb == '\r') || (rxb == '\n')) {
			//terminate chunk normally
			ecbuff_write(fifo_in, &lf);
			ecbuff_write(fifo_in, &chunkvalid);
			break;
		}
		ecbuff_write(fifo_in, &rxb);
		in_len += 1;
		break;
	case HRX_ESCAPE:
		//previous byte was Escape: do not check for \r or \n termination
		hrx_state = HRX_RX;
		ecbuff_write(fifo_in, &rxb);
		in_len += 1;
		break;
	case HRX_RESYNC:
		if ((rxb == '\r') || (rxb == '\n')) {
			//if the host sends both CR+LF, the "LF" will cause
			// an empty command to be parsed. No big deal
			hrx_state = HRX_RX;
			in_len = 0;
		}
		break;
	}
}


void host_tx(uint8_t txb) {
	if (!ecbuff_write(fifo_out, &txb)) {
		//TODO : overflow
		return;
	}
	return;
}

void host_tx_m(uint8_t *data, unsigned len) {
	if (len > 256) {
		len = 256;
	}
	//fugly, loop write
	unsigned idx;
	for (idx = 0; idx < len; idx++) {
		if (!ecbuff_write(fifo_out, &data[idx])) {
			//TODO : overflow
			return;
		}
	}
	return;
}
