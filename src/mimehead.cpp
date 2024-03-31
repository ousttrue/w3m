/*
 * MIME header support by Akinori ITO
 */
#include "mimehead.h"
#include "cmp.h"
#include "myctype.h"
#include <sys/types.h>
#include <sstream>

#define MIME_ENCODED_LINE_LIMIT 80
#define MIME_ENCODED_WORD_LENGTH_OFFSET 18
#define MIME_ENCODED_WORD_LENGTH_ESTIMATION(x)                                 \
  (((x) + 2) * 4 / 3 + MIME_ENCODED_WORD_LENGTH_OFFSET)
#define MIME_DECODED_WORD_LENGTH_ESTIMATION(x)                                 \
  (((x)-MIME_ENCODED_WORD_LENGTH_OFFSET) / 4 * 3)
#define J_CHARSET "ISO-2022-JP"

#define BAD_BASE64 255

// void w3m_GC_free(void *ptr) { GC_FREE(ptr); }

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

std::string decodeB(const char **ww) {
  growbuf gb;
  decodeB_to_growbuf(&gb, ww);
  return gb.to_Str();
}

void decodeB_to_growbuf(struct growbuf *gb, char **ww) {
  unsigned char c[4];
  char *wp = *ww;
  char d[3];
  int i, n_pad;

  // gb->reserve(strlen(wp) + 1);
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
      gb->GROWBUF_ADD_CHAR(d[i]);
    }
    if (n_pad || *wp == '\0' || *wp == '?')
      break;
  }
last:
  // gb->reserve(gb->length + 1);
  // gb->ptr[gb->length] = '\0';
  *ww = wp;
  return;
}

std::string decodeU(const char **ww) {
  struct growbuf gb;
  decodeU_to_growbuf(&gb, ww);
  return gb.to_Str();
}

void decodeU_to_growbuf(struct growbuf *gb, char **ww) {
  char *w = *ww;
  if (*w <= 0x20 || *w >= 0x60)
    return;

  int n = *w - 0x20;
  int i;
  for (w++, i = 2; *w != '\0' && n; n--) {
    unsigned char c1 = (w[0] - 0x20) % 0x40;
    unsigned char c2 = (w[1] - 0x20) % 0x40;
    gb->_buf.push_back((c1 << i) | (c2 >> (6 - i)));
    if (i == 6) {
      w += 2;
      i = 2;
    } else {
      w++;
      i += 2;
    }
  }
  // gb->ptr[gb->length] = '\0';
  return;
}

/* RFC2047 (4.2. The "Q" encoding) */
std::string decodeQ(const char **ww) {
  const char *w = *ww;
  std::stringstream a;
  for (; *w != '\0' && *w != '?'; w++) {
    if (*w == '=') {
      w++;
      a << ha2d(*w, *(w + 1));
      w++;
    } else if (*w == '_') {
      a << ' ';
    } else {
      a << *w;
    }
  }
  *ww = w;
  return a.str();
}

/* RFC2045 (6.7. Quoted-Printable Content-Transfer-Encoding) */
std::string decodeQP(const char **ww) {
  struct growbuf gb;
  decodeQP_to_growbuf(&gb, ww);
  return gb.to_Str();
}

void decodeQP_to_growbuf(struct growbuf *gb, char **ww) {
  char *w = *ww;
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
        gb->_buf.push_back(ha2d(*w, *(w + 1)));
        w++;
      }
    } else {
      gb->_buf.push_back(*w);
    }
  }
  // gb->ptr[gb->length] = '\0';
  *ww = w;
  return;
}

static std::string decodeWord0(const char **ow) {
  const char *p;
  const char *w = *ow;
  char method;
  std::string a;
  std::string tmp;

  if (*w != '=' || *(w + 1) != '?')
    goto convert_fail;
  w += 2;
  for (; *w != '?'; w++) {
    if (*w == '\0')
      goto convert_fail;
    tmp += *w;
  }
  if (strcasecmp(tmp, "ISO-8859-1") != 0 && strcasecmp(tmp, "US_ASCII") != 0)
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
  return {};
}

/*
 * convert MIME encoded string to the original one
 */
std::string decodeMIME0(const std::string &orgstr) {
  auto org = orgstr.data();
  auto endp = orgstr.data() + orgstr.size();
  const char *org0;
  const char *p;
  std::stringstream cnv;

  while (org < endp) {
    if (*org == '=' && *(org + 1) == '?') {
    nextEncodeWord:
      p = org;
      cnv << decodeWord0(&org);
      if (org == p) { /* Convert failure */
        cnv << org;
        return cnv.str();
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
      cnv << *org;
      org++;
    }
  }
  return cnv.str();
}

// static void *w3m_GC_realloc_atomic(void *ptr, size_t size) {
//   return ptr ? GC_REALLOC(ptr, size) : GC_MALLOC_ATOMIC(size);
// }

// void growbuf_init(struct growbuf *gb) {
//   gb->realloc_proc = &w3m_GC_realloc_atomic;
//   gb->free_proc = &w3m_GC_free;
// }
//
// void growbuf_init_without_GC(struct growbuf *gb) {
//   gb->realloc_proc = &xrealloc;
//   gb->free_proc = &xfree;
// }

// void growbuf::clear() {
//   (*this->free_proc)(this->ptr);
//   this->ptr = NULL;
//   this->length = 0;
//   this->area_size = 0;
// }

// Str *growbuf::to_Str() {
//   Str *s;
//
//   if (this->free_proc == &w3m_GC_free) {
//     this->reserve(this->length + 1);
//     this->ptr[this->length] = '\0';
//     s = (Str *)New(Str);
//     s->ptr = this->ptr;
//     s->length = this->length;
//     s->area_size = this->area_size;
//   } else {
//     s = Strnew_charp_n(this->ptr, this->length);
//     (*this->free_proc)(this->ptr);
//   }
//   this->ptr = NULL;
//   this->length = 0;
//   this->area_size = 0;
//   return s;
// }

// void growbuf::reserve(int leastarea) {
//   if (this->area_size < leastarea) {
//     int newarea = this->area_size * 3 / 2;
//     if (newarea < leastarea)
//       newarea = leastarea;
//     newarea += 16;
//     this->ptr = (char *)(*this->realloc_proc)(this->ptr, newarea);
//     this->area_size = newarea;
//   }
// }

// void growbuf::append(const unsigned char *src, int len) {
//   this->reserve(this->length + len);
//   memcpy(&this->ptr[this->length], src, len);
//   this->length += len;
// }
