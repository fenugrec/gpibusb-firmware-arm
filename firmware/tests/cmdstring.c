/* basic test suite for command string tokenizing and parsing
 * (c) fenugrec 2018
 *
 * This is meant to be compiled and run on the host system, not the mcu !
 *
 * It would be too complicated to compile the relevant part of cmd_parser.c,
 * so instead the code was just copied into test_cmd_poll() !
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "../host_comms.h"
#include "../stypes.h"

struct tvect {
	const char *input;
	unsigned input_len;
	const char *expected_out;	//unescaped command token or data chunk
	unsigned expected_len;	//either length of command tok, or length of unescaped data
	bool is_cmd;
};

/* test vectors; it's assumed they're all marked as CHUNK_VALID */
const struct tvect vectors[] = {
	/* some commands, no escapes */
	{"+\n", 2, "+", 1, 1},
	{"+a\n", 3, "+a", 2, 1},
	{"+a:\n", 4, "+a:", 3, 1},	//correct command tok, but no args
	{"++a \n", 5, "++a", 3, 1},	//space-separated tok, but no args
	/* more commands, with esacpes */
	{"+a\x1b""\nb\n", 6, "+a\nb", 3, 1},	//escaped \n
	{"+a\x1b""a\n", 5, "+aa", 3, 1},	//escaped 'a' for no reason
	/* data, with and without escapes */
	{"\n", 1, "", 0, 0},	//stray \n : empty chunk
	{"1234\n", 5, "1234", 4, 0},	//normal chunk
	{"123\x1b""\n\n", 6, "123\n", 4, 0},	//escaped \n
	{"123\x1b""4\n", 6, "1234", 4, 0},	//escaped 4
	{"12\x1b\x00""34\n", 7, "12\x00""34", 5, 0},	//escaped 0x00
	{NULL, 0, NULL, 0, 0}
};


u8 input_buf[HOST_IN_BUFSIZE];	//cmd_poll writes parsed chunk into this
unsigned in_len = 0;
unsigned cmd_len = 0;	//length of command token, excluding 0 termination. Args start at cmd_len+2
bool in_cmd = 0;

/** hacked cmd_poll to be fed one byte at a time;
 * @return 1 when a chunk is
 * parsed (that would have resulted in a call to cmd_chunk or data_chunk)
 *
 * uses globals
 */
bool test_cmd_poll(u8 rxb) {

	static bool escape_next = 0;
	static bool wait_guardbyte = 0;

	bool bogus = 1;
	while (bogus) {
		bogus = 0;	//force to run once
		if ((in_len == 0) && (rxb == '+')) {
			//start of new command chunk
			in_cmd = 1;
			cmd_len = 0;
		}

		if (wait_guardbyte) {
			//just finished a chunk; make sure it was valid
			wait_guardbyte = 0;
			if (rxb != CHUNK_VALID) {
				//discard
				in_len = 0;
				continue;
			}
			if (in_cmd) {
				//chunk_cmd((char *) input_buf, cmd_len);
				return 1;
			} else {
				//chunk_data((char *) input_buf, in_len);
				return 1;
			}
			in_len = 0;
			continue;
		}

		if (!escape_next && (rxb == 27)) {
			//regardless of chunk type (command or data),
			//strip escape char
			escape_next = 1;
			continue;
		}

		if (in_cmd) {
			if (!escape_next && (rxb == '\n')) {
				//0-terminate.
				wait_guardbyte = 1;
				input_buf[in_len] = 0;
				continue;
			}
			//also, tokenize now instead of calling strtok later.
			if (rxb == ':') {
				//commands of form "+<cmd>:<args>" : split args after ':'
				input_buf[in_len++] = rxb;
				cmd_len = in_len;
				input_buf[in_len++] = 0;
			} else if (rxb == ' ') {
				cmd_len = in_len;
				//commands of form "++<cmd> <args>": split args on ' '
				input_buf[in_len++] = 0;
			} else {
				input_buf[in_len++] = rxb;
			}
			escape_next = 0;
			continue;
		}

		// if we made it here, we're dealing with data.
		if (!escape_next && (rxb == '\n')) {
			//terminate.
			wait_guardbyte = 1;
			continue;
		}
		input_buf[in_len++] = rxb;
		escape_next = 0;
	}
	return 0;
}


/** ret 1 if ok */
bool run_test(const struct tvect *tv) {
	unsigned icur;
	bool done = 0;

	// 1) reset stuff
	in_len = 0;
	cmd_len = 0;
	in_cmd = 0;

	// 2) feed input string
	for (icur=0; icur < tv->input_len; icur++) {
		done = test_cmd_poll((u8) tv->input[icur]);
		if (done) {
			printf("FAIL\tchunk ended early @ %u\t", icur);
			return 0;
		}
	}
	// 3) cmd_poll should be waiting for guard byte here
	done = test_cmd_poll(CHUNK_VALID);

	//chunk should have ended
	if (!done) {
		printf("FAIL\tchunk ended late\t");
		return 0;
	}
	// cmd/data match ?
	if (in_cmd != tv->is_cmd) {
		printf("FAIL\tcmd/data ident\t");
		return 0;
	}
	// expected length match ? different depending on cmd/data
	if (in_cmd) {
		if (cmd_len != tv->expected_len) {
			printf("FAIL\tbad cmd len, got %u, want %u\t",
					cmd_len, tv->expected_len);
		return 0;
		}
	} else {
		if (in_len != tv->expected_len) {
			printf("FAIL\tbad data len, got %u, want %u\t",
					in_len, tv->expected_len);
		return 0;
		}
	}
	// expected chunk contents match ?
	if (memcmp(input_buf, tv->expected_out, tv->expected_len)) {
		printf("FAIL\texpected contents mismatch\t");
		return 0;
	}
	printf("PASS\t");
	return 1;
}


void u8_dump(const void *data, size_t len) {
	const uint8_t *p = (const uint8_t *)data;
	size_t i;
	for (i = 0; i < len; i++) {
		printf("0x%02X ", p[i]);
	}
}

int main(int argc, char **argv) {
	(void) argc;
	(void) argv;
	const struct tvect *tv;
	unsigned icur;

	printf("RESULT\tdetail\t\t(pattern)\n");

	for (icur = 0; vectors[icur].input; icur++) {
		tv = &vectors[icur];
		if (run_test(tv)) {
			printf("\n");
		} else {
			printf("(pattern %u: ", icur);
			u8_dump(tv->input, tv->expected_len);
			printf("...\n");	//input len >= expected_len due to escapes
		}
	}

	return 0;
}
