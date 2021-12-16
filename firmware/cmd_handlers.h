#ifndef _CMD_HANDLERS_H
#define _CMD_HANDLERS_H

void do_nothing(const char *args);
void do_addr(const char *args);
void do_eos2(const char *args);
void do_eoi(const char *args);
void do_readCmd2(const char *args);
void do_strip(const char *args);
void do_version2(const char *args);
void do_autoRead(const char *args);
void do_reset(const char *args);
void do_debug(const char *args);
void do_clr(const char *args);
void do_eotEnable(const char *args);
void do_eotChar(const char *args);
void do_ifc(const char *args);
void do_llo(const char *args);
void do_loc(const char *args);
void do_lon(const char *args);
void do_mode(const char *args);
void do_readTimeout(const char *args);
void do_savecfg(const char *args);
void do_spoll(const char *args);
void do_srq(const char *args);
void do_status(const char *args);
void do_trg(const char *args);
void do_help(const char *args);

#endif
