#ifndef _CMD_HASHTABLE_H
#define _CMD_HASHTABLE_H

/** find and run the command handler
 *
 * @param cmdstr the command token such as "++addr", 0-terminated
 * @param cmdlen strlen(cmdstr) ?
 * @param args start of arguments after command token, 0-terminated
 */
void cmd_find_run(const char *cmdstr, unsigned cmdlen, const char *args);

struct cmd_entry {
	const char *name;
	void (*handler)(const char *args);
	const char *helpstring;
};

/** for every known command, run the given callback */
void cmd_walk_cmdlist(void (*cb)(const struct cmd_entry *cmd));

#endif // _CMD_HASHTABLE_H
