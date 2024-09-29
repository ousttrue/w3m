#pragma once

extern int Tabstop;

#define LINELEN 256 /* Initial line length */

/*
 * Line Property
 */

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

typedef unsigned short Lineprop;

/* Flags for calcPosition() */
enum ColumnPositionMode {
  CP_AUTO = 0,
  CP_FORCE = 1,
};

extern int calcPosition(char *l, Lineprop *pr, int len, int pos, int bpos,
                        enum ColumnPositionMode mode);

#define COLPOS(l, c) calcPosition(l->lineBuf, l->propBuf, l->len, c, 0, CP_AUTO)

typedef struct Line {
  char *lineBuf;
  Lineprop *propBuf;
  struct Line *next;
  struct Line *prev;
  int len;
  int width;
  long linenumber;      /* on buffer */
  long real_linenumber; /* on file */
  unsigned short usrflags;
  int size;
  int bpos;
  int bwidth;
} Line;

#define get_mctype(c) (IS_CNTRL(*(c)) ? PC_CTRL : PC_ASCII)
#define get_mclen(c) 1
#define get_mcwidth(c) 1
#define get_strwidth(c) strlen(c)
#define get_Str_strwidth(c) ((c)->length)

int columnLen(Line *line, int column);
int columnPos(Line *line, int column);
