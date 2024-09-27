/* $Id: entity.c,v 1.7 2003/09/24 18:48:59 ukai Exp $ */
#ifdef DUMMY
#include "Str.h"
#define NBSP " "
#define UseAltEntity 1
#undef USE_M17N
#else /* DUMMY */
#include "fm.h"
#endif /* DUMMY */
#include "ctrlcode.h"

/* *INDENT-OFF* */
static char *alt_latin1[96] = {NBSP,  "!",   "-c-", "-L-", "CUR", "=Y=", "|",
                               "S:",  "\"",  "(C)", "-a",  "<<",  "NOT", "-",
                               "(R)", "-",   "DEG", "+-",  "^2",  "^3",  "'",
                               "u",   "P:",  ".",   ",",   "^1",  "-o",  ">>",
                               "1/4", "1/2", "3/4", "?",   "A`",  "A'",  "A^",
                               "A~",  "A:",  "AA",  "AE",  "C,",  "E`",  "E'",
                               "E^",  "E:",  "I`",  "I'",  "I^",  "I:",  "D-",
                               "N~",  "O`",  "O'",  "O^",  "O~",  "O:",  "x",
                               "O/",  "U`",  "U'",  "U^",  "U:",  "Y'",  "TH",
                               "ss",  "a`",  "a'",  "a^",  "a~",  "a:",  "aa",
                               "ae",  "c,",  "e`",  "e'",  "e^",  "e:",  "i`",
                               "i'",  "i^",  "i:",  "d-",  "n~",  "o`",  "o'",
                               "o^",  "o~",  "o:",  "-:",  "o/",  "u`",  "u'",
                               "u^",  "u:",  "y'",  "th",  "y:"};
/* *INDENT-ON* */

char *conv_entity(unsigned int c) {
  char b = c & 0xff;

  if (c < 0x20) /* C0 */
    return " ";
  if (c < 0x7f) /* ASCII */
    return Strnew_charp_n(&b, 1)->ptr;
  if (c < 0xa0) /* DEL, C1 */
    return " ";
  if (c == 0xa0)
    return NBSP;
  if (c == 0xad) /* SOFT HYPHEN */
    return "";
  if (c < 0x100) { /* Latin1 (ISO 8859-1) */
    if (UseAltEntity)
      return alt_latin1[c - 0xa0];
    return Strnew_charp_n(&b, 1)->ptr;
  }
  if (c == 0x201c || c == 0x201f || c == 0x201d || c == 0x2033)
    return "\"";
  if (c == 0x2018 || c == 0x201b || c == 0x2019 || c == 0x2032)
    return "'";
  if (c >= 0x2010 && c < 0x2014)
    return "-";
  if (c == 0x2014)
    return "--";
  return "?";
}
