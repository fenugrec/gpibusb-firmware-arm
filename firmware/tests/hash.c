/* basic test suite for command string hashing and lookup
 * (c) fenugrec 2018
 *
 * This is meant to be compiled and run on the host system, not the mcu !
 *
 * TODO : fuzzing
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "dummy_handlers.h"

/** decls to be able to call the raw lookup func instead of
* using the cmd_find_run() interface
*/

struct cmd_entry { char *name; void (*handler)(const char *args); };

const struct cmd_entry *cmd_lookup (register const char *str, register size_t len);

/* incomplete / short commands */
static const char *invalid_chunks[] = {
	"+",
	"++",
	"+a",
	"+a 3",	//no ':'
	"a",	//no '+', short => data
	"asdf",	//data
	NULL
	};

static const char *valid_chunks[] = {
	"+a:3",
	"++addr 3",
	"++addr 5 8",	//2 args
	"++addr",	//no arg : query
	NULL
	};

static const struct cmd_entry null_cmd = {
	"", NULL};

/** test fixed array of inputs
 *
 * @param chunklist is a "const char *chunklist[]..."
 * @param expect_valid : if 1, chunks must be found
 */
static void test_array(const char **chunklist, const char *seriesname, bool expect_valid) {
	unsigned idx;
	const struct cmd_entry *cmde;
	const char *entry;

	for (idx = 0; chunklist[idx] != NULL; idx++) {
		entry = chunklist[idx];
		cmde = cmd_lookup(entry, strlen(entry));

		if (cmde == NULL) {
			cmde = &null_cmd;	//just to make it printable
		}

		if ((expect_valid && (cmde == &null_cmd)) ||
			(!expect_valid && (cmde != &null_cmd))) {
			printf("FAIL\t%s\t%u\t%s\tNULL\n", seriesname, idx, entry);
		} else {
			printf("PASS\t%s\t%u\t%s\t%s\n", seriesname, idx, entry, cmde->name);
		}
	}
}

int main(int argc, char **argv) {
	(void) argc;
	(void) argv;

	printf("RESULT\tseries\tidx\tstr\tretval\n");
	test_array(invalid_chunks, "invalid", 0);
	test_array(valid_chunks, "valid", 1);
	return 0;
}
