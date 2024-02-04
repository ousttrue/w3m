/* $Id: mimehead.c,v 1.10 2003/10/05 18:52:51 ukai Exp $ */
/*
 * MIME header support by Akinori ITO
 */
#include "mimehead.h"
#include <sys/types.h>
#include <string.h>
#include "alloc.h"
#include "myctype.h"
#include "Str.h"
#include "indep.h"

#define MIME_ENCODED_LINE_LIMIT 80
#define MIME_ENCODED_WORD_LENGTH_OFFSET 18
#define MIME_ENCODED_WORD_LENGTH_ESTIMATION(x)                                 \
  (((x) + 2) * 4 / 3 + MIME_ENCODED_WORD_LENGTH_OFFSET)
#define MIME_DECODED_WORD_LENGTH_ESTIMATION(x)                                 \
  (((x)-MIME_ENCODED_WORD_LENGTH_OFFSET) / 4 * 3)
#define J_CHARSET "ISO-2022-JP"

#define BAD_BASE64 255

void w3m_GC_free(void *ptr) { GC_FREE(ptr); }

static unsigned char c2e(char x) {
  if ('A' <= x && x <= 'Z')
    return (x) - 'A';
  if ('a' <= x && x <= 'z')
    return (x) - 'a' + 26;
  if ('0' <= x && x <= '9')
    return (x) - '0' + 52;
  if (x == '+')
    return 62;
  if (x == '/')
    return 63;
  return BAD_BASE64;
}

static int ha2d(char x, char y) {
  int r = 0;

  if ('0' <= x && x <= '9')
    r = x - '0';
  else if ('A' <= x && x <= 'F')
    r = x - 'A' + 10;
  else if ('a' <= x && x <= 'f')
    r = x - 'a' + 10;

  r <<= 4;

  if ('0' <= y && y <= '9')
    r += y - '0';
  else if ('A' <= y && y <= 'F')
    r += y - 'A' + 10;
  else if ('a' <= y && y <= 'f')
    r += y - 'a' + 10;

  return r;
}

Str *decodeB(char **ww) {
  struct growbuf gb;

  growbuf_init(&gb);
  decodeB_to_growbuf(&gb, ww);
  return growbuf_to_Str(&gb);
}

void decodeB_to_growbuf(struct growbuf *gb, char **ww) {
  unsigned char c[4];
  char *wp = *ww;
  char d[3];
  int i, n_pad;

  growbuf_reserve(gb, strlen(wp) + 1);
  n_pad = 0;
  while (1) {
    for (i = 0; i < 4; i++) {
      c[i] = *(wp++);
      if (*wp == '\0' || *wp == '?') {
        i++;
        for (; i < 4; i++) {
          c[i] = '=';
        }
        break;
      }
    }
    if (c[3] == '=') {
      n_pad++;
      c[3] = 'A';
      if (c[2] == '=') {
        n_pad++;
        c[2] = 'A';
      }
    }
    for (i = 0; i < 4; i++) {
      c[i] = c2e(c[i]);
      if (c[i] == BAD_BASE64) {
        goto last;
      }
    }
    d[0] = ((c[0] << 2) | (c[1] >> 4));
    d[1] = ((c[1] << 4) | (c[2] >> 2));
    d[2] = ((c[2] << 6) | c[3]);
    for (i = 0; i < 3 - n_pad; i++) {
      GROWBUF_ADD_CHAR(gb, d[i]);
    }
    if (n_pad || *wp == '\0' || *wp == '?')
      break;
  }
last:
  growbuf_reserve(gb, gb->length + 1);
  gb->ptr[gb->length] = '\0';
  *ww = wp;
  return;
}

Str *decodeU(char **ww) {
  struct growbuf gb;

  growbuf_init(&gb);
  decodeU_to_growbuf(&gb, ww);
  return growbuf_to_Str(&gb);
}

void decodeU_to_growbuf(struct growbuf *gb, char **ww) {
  unsigned char c1, c2;
  char *w = *ww;
  int n, i;

  if (*w <= 0x20 || *w >= 0x60)
    return;
  n = *w - 0x20;
  growbuf_reserve(gb, n + 1);
  for (w++, i = 2; *w != '\0' && n; n--) {
    c1 = (w[0] - 0x20) % 0x40;
    c2 = (w[1] - 0x20) % 0x40;
    gb->ptr[gb->length++] = (c1 << i) | (c2 >> (6 - i));
    if (i == 6) {
      w += 2;
      i = 2;
    } else {
      w++;
      i += 2;
    }
  }
  gb->ptr[gb->length] = '\0';
  return;
}

/* RFC2047 (4.2. The "Q" encoding) */
Str *decodeQ(char **ww) {
  char *w = *ww;
  Str *a = Strnew_size(strlen(w));

  for (; *w != '\0' && *w != '?'; w++) {
    if (*w == '=') {
      w++;
      Strcat_char(a, ha2d(*w, *(w + 1)));
      w++;
    } else if (*w == '_') {
      Strcat_char(a, ' ');
    } else
      Strcat_char(a, *w);
  }
  *ww = w;
  return a;
}

/* RFC2045 (6.7. Quoted-Printable Content-Transfer-Encoding) */
Str *decodeQP(char **ww) {
  struct growbuf gb;

  growbuf_init(&gb);
  decodeQP_to_growbuf(&gb, ww);
  return growbuf_to_Str(&gb);
}

void decodeQP_to_growbuf(struct growbuf *gb, char **ww) {
  char *w = *ww;

  growbuf_reserve(gb, strlen(w) + 1);
  for (; *w != '\0'; w++) {
    if (*w == '=') {
      w++;
      if (*w == '\n' || *w == '\r' || *w == ' ' || *w == '\t') {
        while (*w != '\n' && *w != '\0')
          w++;
        if (*w == '\0')
          break;
      } else {
        if (*w == '\0' || *(w + 1) == '\0')
          break;
        gb->ptr[gb->length++] = ha2d(*w, *(w + 1));
        w++;
      }
    } else
      gb->ptr[gb->length++] = *w;
  }
  gb->ptr[gb->length] = '\0';
  *ww = w;
  return;
}

Str *decodeWord0(char **ow) {
  char *p, *w = *ow;
  char method;
  Str *a = Strnew();
  Str *tmp = Strnew();

  if (*w != '=' || *(w + 1) != '?')
    goto convert_fail;
  w += 2;
  for (; *w != '?'; w++) {
    if (*w == '\0')
      goto convert_fail;
    Strcat_char(tmp, *w);
  }
  if (strcasecmp(tmp->ptr, "ISO-8859-1") != 0 &&
      strcasecmp(tmp->ptr, "US_ASCII") != 0)
    /* NOT ISO-8859-1 encoding ... don't convert */
    goto convert_fail;
  w++;
  method = *(w++);
  if (*w != '?')
    goto convert_fail;
  w++;
  p = w;
  switch (TOUPPER(method)) {
  case 'B':
    a = decodeB(&w);
    break;
  case 'Q':
    a = decodeQ(&w);
    break;
  default:
    goto convert_fail;
  }
  if (p == w)
    goto convert_fail;
  if (*w == '?') {
    w++;
    if (*w == '=')
      w++;
  }
  *ow = w;
  return a;

convert_fail:
  return Strnew();
}

/*
 * convert MIME encoded string to the original one
 */
Str *decodeMIME0(Str *orgstr) {
  char *org = orgstr->ptr, *endp = org + orgstr->length;
  char *org0, *p;
  Str *cnv = NULL;

  while (org < endp) {
    if (*org == '=' && *(org + 1) == '?') {
      if (cnv == NULL) {
        cnv = Strnew_size(orgstr->length);
        Strcat_charp_n(cnv, orgstr->ptr, org - orgstr->ptr);
      }
    nextEncodeWord:
      p = org;
      Strcat(cnv, decodeWord(&org, charset));
      if (org == p) { /* Convert failure */
        Strcat_charp(cnv, org);
        return cnv;
      }
      org0 = org;
    SPCRLoop:
      switch (*org0) {
      case ' ':
      case '\t':
      case '\n':
      case '\r':
        org0++;
        goto SPCRLoop;
      case '=':
        if (org0[1] == '?') {
          org = org0;
          goto nextEncodeWord;
        }
      default:
        break;
      }
    } else {
      if (cnv != NULL)
        Strcat_char(cnv, *org);
      org++;
    }
  }
  if (cnv == NULL)
    return orgstr;
  return cnv;
}

static void *w3m_GC_realloc_atomic(void *ptr, size_t size) {
  return ptr ? GC_REALLOC(ptr, size) : GC_MALLOC_ATOMIC(size);
}

void growbuf_init(struct growbuf *gb) {
  gb->ptr = NULL;
  gb->length = 0;
  gb->area_size = 0;
  gb->realloc_proc = &w3m_GC_realloc_atomic;
  gb->free_proc = &w3m_GC_free;
}

void growbuf_init_without_GC(struct growbuf *gb) {
  gb->ptr = NULL;
  gb->length = 0;
  gb->area_size = 0;
  gb->realloc_proc = &xrealloc;
  gb->free_proc = &xfree;
}

void growbuf_clear(struct growbuf *gb) {
  (*gb->free_proc)(gb->ptr);
  gb->ptr = NULL;
  gb->length = 0;
  gb->area_size = 0;
}

Str *growbuf_to_Str(struct growbuf *gb) {
  Str *s;

  if (gb->free_proc == &w3m_GC_free) {
    growbuf_reserve(gb, gb->length + 1);
    gb->ptr[gb->length] = '\0';
    s = (Str *)New(Str);
    s->ptr = gb->ptr;
    s->length = gb->length;
    s->area_size = gb->area_size;
  } else {
    s = Strnew_charp_n(gb->ptr, gb->length);
    (*gb->free_proc)(gb->ptr);
  }
  gb->ptr = NULL;
  gb->length = 0;
  gb->area_size = 0;
  return s;
}

void growbuf_reserve(struct growbuf *gb, int leastarea) {
  int newarea;

  if (gb->area_size < leastarea) {
    newarea = gb->area_size * 3 / 2;
    if (newarea < leastarea)
      newarea = leastarea;
    newarea += 16;
    gb->ptr = (char *)(*gb->realloc_proc)(gb->ptr, newarea);
    gb->area_size = newarea;
  }
}

void growbuf_append(struct growbuf *gb, const unsigned char *src, int len) {
  growbuf_reserve(gb, gb->length + len);
  memcpy(&gb->ptr[gb->length], src, len);
  gb->length += len;
}
