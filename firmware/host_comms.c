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


/** Host RX state machine */
enum e_hrx_state {
	HRX_RX,	// while building a chunk
	HRX_ESCAPE,	// pass next byte without ending chunk
	HRX_RESYNC,	//after a buffer overflow : wait for CR/LF
};

static enum e_hrx_state hrx_state = HRX_RX;



/***** funcs */


void host_comms_init(void) {
	ecbuff_init(fifo_in, HOST_IN_BUFSIZE, 1);
	ecbuff_init(fifo_out, HOST_OUT_BUFSIZE, 1);

	hrx_state = HRX_RX;
	return;
}

/** filter and save data.
 *
 * Checks for overflow, skips over escaped characters, and
 * appends a CHUNK_VALID / _INVALID guard byte after each chunk.
 *
 * Does not distinguish between data or commands.
 * converts unescaped CR to LF.
 */
void host_comms_rx(uint8_t rxb) {
	const u8 lf = '\n';
	const u8 chunkvalid = CHUNK_VALID;	//gross
	const u8 chunkinvalid = CHUNK_INVALID;	//gross

	if (ecbuff_is_full(fifo_in)) return;
	if (ecbuff_unused(fifo_in) <= 2) {
		//overflow

		ecbuff_write(fifo_in, &lf);
		ecbuff_write(fifo_in, &chunkinvalid);
		hrx_state = HRX_RESYNC;
		return;
	}

	switch (hrx_state) {
	case HRX_RX:
		switch (rxb) {
		case '\r':
			//CR : replace with LF
			//fallthru
		case '\n':
			//terminate chunk normally
			ecbuff_write(fifo_in, &lf);
			ecbuff_write(fifo_in, &chunkvalid);
			break;
		case 27:
			//if it was an escape, it needs to be passed on
			hrx_state = HRX_ESCAPE;
			//fallthru
		default:
			ecbuff_write(fifo_in, &rxb);
			break;
		}
		break;
	case HRX_ESCAPE:
		//previous byte was Escape: do not check for \r or \n termination
		hrx_state = HRX_RX;
		ecbuff_write(fifo_in, &rxb);
		break;
	case HRX_RESYNC:
		//drop all chars except a CR or LF
		//TODO : skip unescaped CR/LF in case we were receiving tons of data
		if ((rxb == '\r') || (rxb == '\n')) {
			hrx_state = HRX_RX;
		}
		break;
	default:
		//XXX assert
		break;
	}
}


void host_tx(uint8_t txb) {
	if (!ecbuff_write(fifo_out, &txb)) {
		sys_incstats(STATS_TXOVF);
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
			sys_incstats(STATS_TXOVF);
			return;
		}
	}
	return;
}
