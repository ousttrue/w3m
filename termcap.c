#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

/* BUFSIZE is the initial size allocated for the buffer
   for reading the termcap file.
   It is not a limit.
   Make it large normally for speed.
   Make it variable when debugging, so can exercise
   increasing the space dynamically.  */

#ifndef BUFSIZE
#ifdef DEBUG
#define BUFSIZE bufsize

int bufsize = 128;
#else
#define BUFSIZE 2048
#endif
#endif
#ifndef TERMCAP_FILE
#define TERMCAP_FILE "/etc/termcap"
#endif

struct termcap_buffer {
  char *beg;
  int size;
  char *ptr;
  int ateof;
  int full;
};

/* The pointer to the data made by tgetent is left here
   for tgetnum, tgetflag and tgetstr to find.  */
static char *term_entry;

/* Make sure that the buffer <- BUFP contains a full line
   of the file open on FD, starting at the place BUFP->ptr
   points to.  Can read more of the file, discard stuff before
   BUFP->ptr, or make the buffer bigger.

   Return the pointer to after the newline ending the line,
   or to the end of the file, if there is no newline to end it.

   Can also merge on continuation lines.  If APPEND_END is
   non-null, it points past the newline of a line that is
   continued; we add another line onto it and regard the whole
   thing as one line.  The caller decides when a line is continued.  */

static char *gobble_line(int fd, register struct termcap_buffer *bufp,
                         char *append_end) {
  register char *end;
  register int nread;
  register char *buf = bufp->beg;
  register char *tem;

  if (!append_end)
    append_end = bufp->ptr;

  while (1) {
    end = append_end;
    while (*end && *end != '\n')
      end++;
    if (*end)
      break;
    if (bufp->ateof)
      return buf + bufp->full;
    if (bufp->ptr == buf) {
      if (bufp->full == bufp->size) {
        bufp->size *= 2;
        /* Add 1 to size to ensure room for terminating null.  */
        tem = (char *)realloc(buf, bufp->size + 1);
        bufp->ptr = (bufp->ptr - buf) + tem;
        append_end = (append_end - buf) + tem;
        bufp->beg = buf = tem;
      }
    } else {
      append_end -= bufp->ptr - buf;
      bcopy(bufp->ptr, buf, bufp->full -= bufp->ptr - buf);
      bufp->ptr = buf;
    }
    if (!(nread = read(fd, buf + bufp->full, bufp->size - bufp->full)))
      bufp->ateof = 1;
    bufp->full += nread;
    buf[bufp->full] = '\0';
  }
  return end + 1;
}

static int compare_contin(register const char *str1,
                          register const char *str2) {
  register int c1, c2;
  while (1) {
    c1 = *str1++;
    c2 = *str2++;
    while (c1 == '\\' && *str1 == '\n') {
      str1++;
      while ((c1 = *str1++) == ' ' || c1 == '\t')
        ;
    }
    if (c2 == '\0') {
      /* End of type being looked up.  */
      if (c1 == '|' || c1 == ':')
        /* If end of name in data base, we win.  */
        return 0;
      else
        return 1;
    } else if (c1 != c2)
      return 1;
  }
}

/* Return nonzero if NAME is one of the names specified
   by termcap entry LINE.  */

static int name_match(const char *line, const char *name) {
  if (!compare_contin(line, name))
    return 1;
  /* This line starts an entry.  Is it the right one?  */
  for (auto tem = line; *tem && *tem != '\n' && *tem != ':'; tem++)
    if (*tem == '|' && !compare_contin(tem + 1, name))
      return 1;

  return 0;
}

/* Given file open on FD and buffer BUFP,
   scan the file from the beginning until a line is found
   that starts the entry for terminal type STR.
   Return 1 if successful, with that line in BUFP,
   or 0 if no entry is found in the file.  */

static int scan_file(const char *str, int fd,
                     register struct termcap_buffer *bufp) {
  register char *end;

  bufp->ptr = bufp->beg;
  bufp->full = 0;
  bufp->ateof = 0;
  *bufp->ptr = '\0';

  lseek(fd, 0L, 0);

  while (!bufp->ateof) {
    /* Read a line into the buffer.  */
    end = NULL;
    do {
      /* if it is continued, append another line to it,
         until a non-continued line ends.  */
      end = gobble_line(fd, bufp, end);
    } while (!bufp->ateof && end[-2] == '\\');

    if (*bufp->ptr != '#' && name_match(bufp->ptr, str))
      return 1;

    /* Discard the line just processed.  */
    bufp->ptr = end;
  }
  return 0;
}

/* Search entry BP for capability CAP.
   Return a pointer to the capability (in BP) if found,
   0 if not found.  */

static char *find_capability(register char *bp, register char *cap) {
  for (; *bp; bp++)
    if (bp[0] == ':' && bp[1] == cap[0] && bp[2] == cap[1])
      return &bp[4];
  return NULL;
}

#define valid_filename_p(fn) (*(fn) == '/')

/* Table, indexed by a character in range 0100 to 0140 with 0100 subtracted,
   gives meaning of character following \, or a space if no special meaning.
   Eight characters per line within the string.  */

static char esctab[] = " \007\010  \033\014 \
      \012 \
  \015 \011 \013 \
        ";

/* PTR points to a string value inside a termcap entry.
   Copy that value, processing \ and ^ abbreviations,
   into the block that *AREA points to,
   or to newly allocated storage if AREA is NULL.
   Return the address to which we copied the value,
   or NULL if PTR is NULL.  */

static char *tgetst1(char *ptr, char **area) {
  register char *p, *r;
  register int c;
  register int size;
  char *ret;
  register int c1;

  if (!ptr)
    return NULL;

  /* `ret' gets address of where to store the string.  */
  if (!area) {
    /* Compute size of block needed (may overestimate).  */
    p = ptr;
    while ((c = *p++) && c != ':' && c != '\n')
      ;
    ret = (char *)malloc(p - ptr + 1);
  } else
    ret = *area;

  /* Copy the string value, stopping at null or colon.
     Also process ^ and \ abbreviations.  */
  p = ptr;
  r = ret;
  while ((c = *p++) && c != ':' && c != '\n') {
    if (c == '^') {
      c = *p++;
      if (c == '?')
        c = 0177;
      else
        c &= 037;
    } else if (c == '\\') {
      c = *p++;
      if (c >= '0' && c <= '7') {
        c -= '0';
        size = 0;

        while (++size < 3 && (c1 = *p) >= '0' && c1 <= '7') {
          c *= 8;
          c += c1 - '0';
          p++;
        }
      }
#ifdef IS_EBCDIC_HOST
      else if (c >= 0200 && c < 0360) {
        c1 = esctab[(c & ~0100) - 0200];
        if (c1 != ' ')
          c = c1;
      }
#else
      else if (c >= 0100 && c < 0200) {
        c1 = esctab[(c & ~040) - 0100];
        if (c1 != ' ')
          c = c1;
      }
#endif
    }
    *r++ = c;
  }
  *r = '\0';
  /* Update *AREA.  */
  if (area)
    *area = r + 1;
  return ret;
}

/* Find the termcap entry data for terminal type NAME
   and store it in the block that BP points to.
   Record its address for future use.

   If BP is null, space is dynamically allocated.

   Return -1 if there is some difficulty accessing the data base
   of terminal types,
   0 if the data base is accessible but the type NAME is not defined
   in it, and some other value otherwise.  */

int tgetent(char *bp, const char *name) {
  register char *termcap_name;
  register int fd;
  struct termcap_buffer buf;
  register char *bp1;
  char *tc_search_point;
  const char *term;
  int malloc_size = 0;
  register int c;
  char *tcenv = NULL;    /* TERMCAP value, if it contains :tc=.  */
  char *indirect = NULL; /* Terminal type in :tc= in TERMCAP value.  */
  int filep;

#ifdef INTERNAL_TERMINAL
  /* For the internal terminal we don't want to read any termcap file,
     so fake it.  */
  if (!strcmp(name, "internal")) {
    term = INTERNAL_TERMINAL;
    if (!bp) {
      malloc_size = 1 + strlen(term);
      bp = (char *)malloc(malloc_size);
    }
    strcpy(bp, term);
    goto ret;
  }
#endif /* INTERNAL_TERMINAL */

  /* For compatibility with programs like `less' that want to
     put data in the termcap buffer themselves as a fallback.  */
  if (bp)
    term_entry = bp;

  termcap_name = getenv("TERMCAP");
  if (termcap_name && *termcap_name == '\0')
    termcap_name = NULL;
#if defined(MSDOS) && !defined(TEST)
  if (termcap_name &&
      (*termcap_name == '\\' || *termcap_name == '/' || termcap_name[1] == ':'))
    dostounix_filename(termcap_name);
#endif

  filep = termcap_name && valid_filename_p(termcap_name);

  /* If termcap_name is non-null and starts with / (in the un*x case, that is),
     it is a file name to use instead of /etc/termcap.
     If it is non-null and does not start with /,
     it is the entry itself, but only if
     the name the caller requested matches the TERM variable.  */

  if (termcap_name && !filep && !strcmp(name, getenv("TERM"))) {
    indirect = tgetst1(find_capability(termcap_name, "tc"), (char **)0);
    if (!indirect) {
      if (!bp)
        bp = termcap_name;
      else
        strcpy(bp, termcap_name);
      goto ret;
    } else { /* It has tc=.  Need to read /etc/termcap.  */
      tcenv = termcap_name;
      termcap_name = NULL;
    }
  }

  if (!termcap_name || !filep)
    termcap_name = TERMCAP_FILE;

    /* Here we know we must search a file and termcap_name has its name.  */

#ifdef MSDOS
  fd = open(termcap_name, O_RDONLY | O_TEXT, 0);
#else
  fd = open(termcap_name, O_RDONLY, 0);
#endif
  if (fd < 0)
    return -1;

  buf.size = BUFSIZE;
  /* Add 1 to size to ensure room for terminating null.  */
  buf.beg = (char *)malloc(buf.size + 1);
  term = indirect ? indirect : name;

  if (!bp) {
    malloc_size = indirect ? strlen(tcenv) + 1 : buf.size;
    bp = (char *)malloc(malloc_size);
  }
  tc_search_point = bp1 = bp;

  if (indirect)
  /* Copy the data from the environment variable.  */
  {
    strcpy(bp, tcenv);
    bp1 += strlen(tcenv);
  }

  while (term) {
    /* Scan the file, reading it via buf, till find start of main entry.  */
    if (scan_file(term, fd, &buf) == 0) {
      close(fd);
      free(buf.beg);
      if (malloc_size)
        free(bp);
      return 0;
    }

    /* Free old `term' if appropriate.  */
    if (term != name)
      free((void *)term);

    /* If BP is malloc'd by us, make sure it is big enough.  */
    if (malloc_size) {
      int offset1 = bp1 - bp, offset2 = tc_search_point - bp;
      malloc_size = offset1 + buf.size;
      bp = termcap_name = (char *)realloc(bp, malloc_size);
      bp1 = termcap_name + offset1;
      tc_search_point = termcap_name + offset2;
    }

    /* Copy the line of the entry from buf into bp.  */
    termcap_name = buf.ptr;
    while ((*bp1++ = c = *termcap_name++) && c != '\n')
      /* Drop out any \ newline sequence.  */
      if (c == '\\' && *termcap_name == '\n') {
        bp1--;
        termcap_name++;
      }
    *bp1 = '\0';

    /* Does this entry refer to another terminal type's entry?
       If something is found, copy it into heap and null-terminate it.  */
    tc_search_point = find_capability(tc_search_point, "tc");
    term = tgetst1(tc_search_point, (char **)0);
  }

  close(fd);
  free(buf.beg);

  if (malloc_size)
    bp = (char *)realloc(bp, bp1 - bp + 1);

ret:
  term_entry = bp;
  return 1;
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

char *BC;
char *UP;
static char tgoto_buf[50];

char *tgoto(const char *cm, int hpos, int vpos) {
  int args[2];
  if (!cm)
    return NULL;
  args[0] = vpos;
  args[1] = hpos;
  return tparam1(cm, tgoto_buf, 50, UP, BC, args);
}

int
tgetnum (const char *cap)
{
  register char *ptr = find_capability (term_entry, cap);
  if (!ptr || ptr[-1] != '#')
    return -1;
  return atoi (ptr);
}

int
tgetflag (const char *cap)
{
  register char *ptr = find_capability (term_entry, cap);
  return ptr && ptr[-1] == ':';
}

/* Look up a string-valued capability CAP.
   If AREA is non-null, it points to a pointer to a block in which
   to store the string.  That pointer is advanced over the space used.
   If AREA is null, space is allocated with `malloc'.  */

char *
tgetstr (char *cap, char **area)
{
  register char *ptr = find_capability (term_entry, cap);
  if (!ptr || (ptr[-1] != '=' && ptr[-1] != '~'))
    return NULL;
  return tgetst1 (ptr, area);
}
