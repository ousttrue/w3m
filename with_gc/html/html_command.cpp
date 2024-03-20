#include "html_command.h"
#define MAX_CMD_LEN 128
#include "hash.h"
#include "myctype.h"

HtmlCommand gethtmlcmd(const char **s) {
  extern Hash_si tagtable;
  char cmdstr[MAX_CMD_LEN];
  const char *p = cmdstr;
  const char *save = *s;

  (*s)++;
  /* first character */
  if (IS_ALNUM(**s) || **s == '_' || **s == '/') {
    *(char *)(p++) = TOLOWER(**s);
    (*s)++;
  } else
    return HTML_UNKNOWN;
  if (p[-1] == '/')
    SKIP_BLANKS(*s);
  while ((IS_ALNUM(**s) || **s == '_') && p - cmdstr < MAX_CMD_LEN) {
    *(char *)(p++) = TOLOWER(**s);
    (*s)++;
  }
  if (p - cmdstr == MAX_CMD_LEN) {
    /* buffer overflow: perhaps caused by bad HTML source */
    *s = save + 1;
    return HTML_UNKNOWN;
  }
  *(char *)p = '\0';

  /* hash search */
  auto cmd = (HtmlCommand)getHash_si(&tagtable, cmdstr, HTML_UNKNOWN);
  while (**s && **s != '>')
    (*s)++;
  if (**s == '>')
    (*s)++;
  return cmd;
}
