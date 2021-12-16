/* ANSI-C code produced by gperf version 3.1 */
/* Command-line: gperf -m 4 --output-file cmd_hashtable.c cmd_hashtable.gen  */
/* Computed positions: -k'3,5' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gperf@gnu.org>."
#endif

#line 7 "cmd_hashtable.gen"

#include <string.h>

#include "cmd_hashtable.h"
#include "cmd_handlers.h"

// silly warning for missing prototype
const struct cmd_entry *cmd_lookup (register const char *str, register size_t len);
#line 18 "cmd_hashtable.gen"
struct cmd_entry { char *name; void (*handler)(const char *args); };

#define TOTAL_KEYWORDS 25
#define MIN_WORD_LENGTH 4
#define MAX_WORD_LENGTH 13
#define MIN_HASH_VALUE 4
#define MAX_HASH_VALUE 28
/* maximum key range = 25, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
cmd_hash (register const char *str, register size_t len)
{
  static const unsigned char asso_values[] =
    {
      29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
      29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
      29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
      29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
      29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
      29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
      29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
      29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
      29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
      29, 29, 29, 29, 29, 29, 29,  0, 29,  2,
      20,  0,  0,  0,  9, 11, 29, 29, 13,  1,
       7,  6, 29, 12,  2,  6,  0, 16,  6, 29,
      29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
      29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
      29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
      29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
      29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
      29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
      29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
      29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
      29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
      29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
      29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
      29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
      29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
      29, 29, 29, 29, 29, 29
    };
  register unsigned int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[4]];
      /*FALLTHROUGH*/
      case 4:
      case 3:
        hval += asso_values[(unsigned char)str[2]];
        break;
    }
  return hval;
}

const struct cmd_entry *
cmd_lookup (register const char *str, register size_t len)
{
  static const struct cmd_entry wordlist[] =
    {
      {"",do_nothing}, {"",do_nothing}, {"",do_nothing},
      {"",do_nothing},
#line 27 "cmd_hashtable.gen"
      {"+dfu", do_reset_dfu},
#line 48 "cmd_hashtable.gen"
      {"++trg", do_trg},
#line 30 "cmd_hashtable.gen"
      {"++auto", do_autoRead},
#line 43 "cmd_hashtable.gen"
      {"++rst", do_reset},
#line 41 "cmd_hashtable.gen"
      {"++read", do_readCmd2},
#line 31 "cmd_hashtable.gen"
      {"++clr", do_clr},
#line 35 "cmd_hashtable.gen"
      {"++eot_char", do_eotChar},
#line 33 "cmd_hashtable.gen"
      {"++eos", do_eos2},
#line 34 "cmd_hashtable.gen"
      {"++eot_enable", do_eotEnable},
#line 49 "cmd_hashtable.gen"
      {"++ver", do_version2},
#line 47 "cmd_hashtable.gen"
      {"++status", do_status},
#line 42 "cmd_hashtable.gen"
      {"++read_tmo_ms", do_readTimeout},
#line 32 "cmd_hashtable.gen"
      {"++eoi", do_eoi},
#line 25 "cmd_hashtable.gen"
      {"+strip", do_strip},
#line 36 "cmd_hashtable.gen"
      {"++ifc", do_ifc},
#line 45 "cmd_hashtable.gen"
      {"++spoll", do_spoll},
#line 38 "cmd_hashtable.gen"
      {"++loc", do_loc},
#line 44 "cmd_hashtable.gen"
      {"++savecfg", do_savecfg},
#line 26 "cmd_hashtable.gen"
      {"+debug", do_debug},
#line 46 "cmd_hashtable.gen"
      {"++srq", do_srq},
#line 37 "cmd_hashtable.gen"
      {"++llo", do_llo},
#line 39 "cmd_hashtable.gen"
      {"++lon", do_lon},
#line 29 "cmd_hashtable.gen"
      {"++addr", do_addr},
#line 40 "cmd_hashtable.gen"
      {"++mode", do_mode},
#line 50 "cmd_hashtable.gen"
      {"++help", do_help}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register unsigned int key = cmd_hash (str, len);

      if (key <= MAX_HASH_VALUE)
        {
          register const char *s = wordlist[key].name;

          if (*str == *s && !strncmp (str + 1, s + 1, len - 1) && s[len] == '\0')
            return &wordlist[key];
        }
    }
  return 0;
}
#line 51 "cmd_hashtable.gen"

void cmd_find_run(const char *cmdstr, unsigned cmdlen, const char *args) {
	const struct cmd_entry *cmd;

	cmd = cmd_lookup(cmdstr, cmdlen);
	if (cmd == NULL) {
		return;
	}
	cmd->handler(args);
}
