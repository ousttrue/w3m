#pragma once

#define LINELEN 256 /* Initial line length */

typedef unsigned short Lineprop;

#define P_CHARTYPE 0x3f00
#define PC_ASCII 0x0000
#define PC_CTRL 0x0100
#define PC_SYMBOL 0x8000

/* Effect ( standout/underline ) */
#define P_EFFECT 0x40ff
#define PE_NORMAL 0x00
#define PE_MARK 0x01
#define PE_UNDER 0x02
#define PE_STAND 0x04
#define PE_BOLD 0x08
#define PE_ANCHOR 0x10
#define PE_EMPH 0x08
#define PE_IMAGE 0x20
#define PE_FORM 0x40
#define PE_ACTIVE 0x80
#define PE_VISITED 0x4000

/* Extra effect */
#define PE_EX_ITALIC 0x01
#define PE_EX_INSERT 0x02
#define PE_EX_STRIKE 0x04

#define PE_EX_ITALIC_E PE_UNDER
#define PE_EX_INSERT_E PE_UNDER
#define PE_EX_STRIKE_E PE_STAND

#define CharType(c) ((c) & P_CHARTYPE)
#define CharEffect(c) ((c) & (P_EFFECT | PC_SYMBOL))
#define SetCharType(v, c) ((v) = (((v) & ~P_CHARTYPE) | (c)))

struct Line {
  char *lineBuf;
  Lineprop *propBuf;
  Line *next;
  Line *prev;
  int len;
  int width;
  long linenumber;      /* on buffer */
  long real_linenumber; /* on file */
  unsigned short usrflags;
  int size;
  int bpos;
  int bwidth;
};

/* Flags for calcPosition() */
#define CP_AUTO 0
#define CP_FORCE 1

#define COLPOS(l, c) calcPosition(l->lineBuf, l->propBuf, l->len, c, 0, CP_AUTO)

#define checkType(a, b, c) _checkType(a, b)
struct Str;
Str *checkType(Str *s, Lineprop **oprop, Linecolor **ocolor);
int calcPosition(char *l, Lineprop *pr, int len, int pos, int bpos, int mode);
