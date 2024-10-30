#pragma once
#include "text/Str.h"
#include <stdint.h>

extern int Tabstop;
extern int ShowEffect;

/*
 * Line Property
 */

typedef unsigned short Lineprop;

enum WtfType {
  WTF_TYPE_ASCII = 0x0,
  WTF_TYPE_CTRL = 0x1,
  WTF_TYPE_WCHAR1 = 0x2,
  WTF_TYPE_WCHAR2 = 0x4,
  WTF_TYPE_WIDE = 0x8,
  WTF_TYPE_UNKNOWN = 0x10,
};

enum CharTypes {
  PC_ASCII = (WTF_TYPE_ASCII << 8),
  PC_CTRL = (WTF_TYPE_CTRL << 8),
  PC_WCHAR1 = (WTF_TYPE_WCHAR1 << 8),
  PC_WCHAR2 = (WTF_TYPE_WCHAR2 << 8),
  PC_KANJI = (WTF_TYPE_WIDE << 8),
  PC_KANJI1 = (PC_WCHAR1 | PC_KANJI),
  PC_KANJI2 = (PC_WCHAR2 | PC_KANJI),
  PC_UNKNOWN = (WTF_TYPE_UNKNOWN << 8),
  PC_SYMBOL = 0x8000,
};

enum LinePropertyMask {
  P_CHARTYPE = 0x3f00,
  P_EFFECT = 0x40ff,
};

/* Effect ( standout/underline ) */
enum CharEffects {
  PE_NORMAL = 0x00,
  PE_MARK = 0x01,
  PE_UNDER = 0x02,
  PE_STAND = 0x04,
  PE_BOLD = 0x08,
  PE_ANCHOR = 0x10,
  PE_EMPH = 0x08,
  PE_IMAGE = 0x20,
  PE_FORM = 0x40,
  PE_ACTIVE = 0x80,
  PE_VISITED = 0x4000,

  /* Extra effect */
  PE_EX_ITALIC = 0x01,
  PE_EX_INSERT = 0x02,
  PE_EX_STRIKE = 0x04,

  PE_EX_ITALIC_E = PE_UNDER,
  PE_EX_INSERT_E = PE_UNDER,
  PE_EX_STRIKE_E = PE_STAND,
};

enum CharEffects CharEffect(Lineprop c);

struct Line {
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
};

/* Flags for calcPosition() */
enum ColumnPositionMode {
  CP_AUTO = 0,
  CP_FORCE = 1,
};

int calcPosition(char *l, Lineprop *pr, int len, int pos,
                 enum ColumnPositionMode mode);
int COLPOS(struct Line *l, int c);
int get_mctype(const uint8_t *c);
int columnLen(struct Line *line, int column);
int columnPos(struct Line *line, int column);
Str checkType(Str s, Lineprop **oprop);
void clear_mark(struct Line *l);
struct Line *currentLineSkip(struct Line *line, int offset, int last);
void nextChar(int *s, struct Line *l);
void prevChar(int *s, struct Line *l);
