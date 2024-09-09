#include "termcap.h"
#include <stdlib.h>
#include <string.h>

char *BC;
char *UP;
static char tgoto_buf[50];

static char *tparam1(char *string, char *outstring, int len, char *up,
                     char *left, register int *argp) {
  register int c;
  register char *p = string;
  register char *op = outstring;
  char *outend;
  int outlen = 0;

  register int tem;
  int *old_argp = argp;
  int doleft = 0;
  int doup = 0;

  outend = outstring + len;

  while (1) {
    /* If the buffer might be too short, make it bigger.  */
    if (op + 5 >= outend) {
      register char *new;
      int offset = op - outstring;

      if (outlen == 0) {
        outlen = len + 40;
        new = (char *)malloc(outlen);
        memcpy(new, outstring, offset);
      } else {
        outlen *= 2;
        new = (char *)realloc(outstring, outlen);
      }

      op = new + offset;
      outend = new + outlen;
      outstring = new;
    }
    c = *p++;
    if (!c)
      break;
    if (c == '%') {
      c = *p++;
      tem = *argp;
      switch (c) {
      case 'd': /* %d means output in decimal.  */
        if (tem < 10)
          goto onedigit;
        if (tem < 100)
          goto twodigit;
      case '3': /* %3 means output in decimal, 3 digits.  */
        if (tem > 999) {
          *op++ = tem / 1000 + '0';
          tem %= 1000;
        }
        *op++ = tem / 100 + '0';
      case '2': /* %2 means output in decimal, 2 digits.  */
      twodigit:
        tem %= 100;
        *op++ = tem / 10 + '0';
      onedigit:
        *op++ = tem % 10 + '0';
        argp++;
        break;

      case 'C':
        /* For c-100: print quotient of value by 96, if nonzero,
           then do like %+.  */
        if (tem >= 96) {
          *op++ = tem / 96;
          tem %= 96;
        }
      case '+': /* %+x means add character code of char x.  */
        tem += *p++;
      case '.': /* %. means output as character.  */
        if (left) {
          /* If want to forbid output of 0 and \n and \t,
             and this is one of them, increment it.  */
          while (tem == 0 || tem == '\n' || tem == '\t') {
            tem++;
            if (argp == old_argp)
              doup++, outend -= strlen(up);
            else
              doleft++, outend -= strlen(left);
          }
        }
        *op++ = tem ? tem : 0200;
      case 'f': /* %f means discard next arg.  */
        argp++;
        break;

      case 'b': /* %b means back up one arg (and re-use it).  */
        argp--;
        break;

      case 'r': /* %r means interchange following two args.  */
        argp[0] = argp[1];
        argp[1] = tem;
        old_argp++;
        break;

      case '>':             /* %>xy means if arg is > char code of x, */
        if (argp[0] > *p++) /* then add char code of y to the arg, */
          argp[0] += *p;    /* and in any case don't output.  */
        p++;                /* Leave the arg to be output later.  */
        break;

      case 'a': /* %a means arithmetic.  */
        /* Next character says what operation.
           Add or subtract either a constant or some other arg.  */
        /* First following character is + to add or - to subtract
           or = to assign.  */
        /* Next following char is 'p' and an arg spec
           (0100 plus position of that arg relative to this one)
           or 'c' and a constant stored in a character.  */
        tem = p[2] & 0177;
        if (p[1] == 'p')
          tem = argp[tem - 0100];
        if (p[0] == '-')
          argp[0] -= tem;
        else if (p[0] == '+')
          argp[0] += tem;
        else if (p[0] == '*')
          argp[0] *= tem;
        else if (p[0] == '/')
          argp[0] /= tem;
        else
          argp[0] = tem;

        p += 3;
        break;

      case 'i':    /* %i means add one to arg, */
        argp[0]++; /* and leave it to be output later.  */
        argp[1]++; /* Increment the following arg, too!  */
        break;

      case '%': /* %% means output %; no arg.  */
        goto ordinary;

      case 'n': /* %n means xor each of next two args with 140.  */
        argp[0] ^= 0140;
        argp[1] ^= 0140;
        break;

      case 'm': /* %m means xor each of next two args with 177.  */
        argp[0] ^= 0177;
        argp[1] ^= 0177;
        break;

      case 'B': /* %B means express arg as BCD char code.  */
        argp[0] += 6 * (tem / 10);
        break;

      case 'D': /* %D means weird Delta Data transformation.  */
        argp[0] -= 2 * (tem % 16);
        break;

      default:
        abort();
      }
    } else
      /* Ordinary character in the argument string.  */
    ordinary:
      *op++ = c;
  }
  *op = 0;
  while (doup-- > 0)
    strcat(op, up);
  while (doleft-- > 0)
    strcat(op, left);
  return outstring;
}

char *tgoto(const char *cm, int hpos, int vpos) {
  int args[2];
  if (!cm)
    return NULL;
  args[0] = vpos;
  args[1] = hpos;
  return tparam1(cm, tgoto_buf, 50, UP, BC, args);
}

#ifndef emacs
short ospeed;
/* If OSPEED is 0, we use this as the actual baud rate.  */
int tputs_baud_rate;
#endif
char PC;

#ifndef emacs
/* Actual baud rate if positive;
   - baud rate / 100 if negative.  */

static int speeds[] = {
#ifdef VMS
    0,   50,  75,  110, 134, 150, -3,  -6,  -12,
    -18, -20, -24, -36, -48, -72, -96, -192
#else  /* not VMS */
    0,   50,  75,  110, 135,  150,  -2,   -3,   -6,   -12,
    -18, -24, -48, -96, -192, -288, -384, -576, -1152
#endif /* not VMS */
};

#endif /* not emacs */

void tputs(const char *str, int nlines, int (*outfun)(char)) {
  register int padcount = 0;
  register int speed;

#ifdef emacs
  extern int baud_rate;
  speed = baud_rate;
  /* For quite high speeds, convert to the smaller
     units to avoid overflow.  */
  if (speed > 10000)
    speed = -speed / 100;
#else
  if (ospeed == 0)
    speed = tputs_baud_rate;
  else
    speed = speeds[ospeed];
#endif

  if (!str)
    return;

  while (*str >= '0' && *str <= '9') {
    padcount += *str++ - '0';
    padcount *= 10;
  }
  if (*str == '.') {
    str++;
    padcount += *str++ - '0';
  }
  if (*str == '*') {
    str++;
    padcount *= nlines;
  }
  while (*str)
    (*outfun)(*str++);

  /* PADCOUNT is now in units of tenths of msec.
     SPEED is measured in characters per 10 seconds
     or in characters per .1 seconds (if negative).
     We use the smaller units for larger speeds to avoid overflow.  */
  padcount *= speed;
  padcount += 500;
  padcount /= 1000;
  if (speed < 0)
    padcount = -padcount;
  else {
    padcount += 50;
    padcount /= 100;
  }

  while (padcount-- > 0)
    (*outfun)(PC);
}
