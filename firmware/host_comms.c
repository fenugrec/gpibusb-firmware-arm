/* Host communications code
 *
 * (c) fenugrec 2018
 *
 * The RX channel (receiving from host) needs a FIFO buffer
 *
 * To simplify the command parser, we do some initial filtering
 * according to state and received length.
 *
 * Once a complete chunk (either command
 * or data) is received, it gets copied to a command buffer to be
 * parsed by the cmd_parser. XXXX
 */

#include <stdint.h>

#include "hw_backend.h"
#include "host_comms.h"
#include "ring.h"

#include "stypes.h"


/**** globals */
struct ring input_ring;

/* Each incoming chunk is saved into the input FIFO.
 *
 * we do minimal filtering, except length check
 * and overflow recovery.
 */
#define UART_BUFFER_SIZE (HOST_IN_BUFSIZE * 3)

static uint8_t input_ring_buffer[UART_BUFFER_SIZE];

static unsigned in_len;	//length of chunk being received from host

/** Host RX state machine */
enum e_hrx_state {HRX_RX, HRX_ESCAPE, HRX_RESYNC};
static enum e_hrx_state hrx_state = HRX_RX;
	//HRX_RX while building a chunk
	//HRX_ESCAPE will not end the chunk if the next byte is \r or \n
	//HRX_RESYNC after a buffer overflow


/***** funcs */


void host_comms_init(void) {
	ring_init(&input_ring, input_ring_buffer, sizeof(input_ring_buffer));
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

	if (in_len == HOST_IN_BUFSIZE) {
		//overflow
		ring_write_ch(&input_ring, '\n');
		ring_write_ch(&input_ring, CHUNK_INVALID);
		hrx_state = HRX_RESYNC;
		return;
	}

	switch (hrx_state) {
	case HRX_RX:
		if (rxb == 27) {
			hrx_state = HRX_ESCAPE;
		} else 	if ((rxb == '\r') || (rxb == '\n')) {
			//terminate chunk normally
			ring_write_ch(&input_ring, '\n');
			ring_write_ch(&input_ring, CHUNK_VALID);
			break;
		}
		ring_write_ch(&input_ring, rxb);
		in_len += 1;
		break;
	case HRX_ESCAPE:
		//previous byte was Escape: do not check for \r or \n termination
		hrx_state = HRX_RX;
		ring_write_ch(&input_ring, rxb);
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
