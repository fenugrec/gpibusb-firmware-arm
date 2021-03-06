#ifndef _CMD_HASHTABLE_H
#define _CMD_HASHTABLE_H

/** find and run the command handler
 *
 * @param cmdstr the command token such as "++addr", 0-terminated
 * @param cmdlen strlen(cmdstr) ?
 * @param args start of arguments after command token, 0-terminated
 */
void cmd_find_run(const char *cmdstr, unsigned cmdlen, const char *args);

#endif // _CMD_HASHTABLE_H
