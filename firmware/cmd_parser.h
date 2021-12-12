#ifndef _CMD_PARSER_H
#define _CMD_PARSER_H

/** wait for command inputs, or GPIB traffic if in device mode
 *
 * Assumes an interrupt-based process is feeding the input FIFO.
 * This func must be called in a loop
 */
void cmd_poll(void);


/** initialize command parser
 *
 */
void cmd_parser_init(void);

#endif // _CMD_PARSER_H
