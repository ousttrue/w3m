#pragma once

typedef unsigned short Lineprop;

// #define P_CHARTYPE 0x3f00

#define WTF_TYPE_ASCII 0x0
#define WTF_TYPE_CTRL 0x1
#define WTF_TYPE_WCHAR1 0x2
#define WTF_TYPE_WCHAR2 0x4
#define WTF_TYPE_WIDE 0x8
#define WTF_TYPE_UNKNOWN 0x10
#define WTF_TYPE_UNDEF 0x20
#define WTF_TYPE_WCHAR1W (WTF_TYPE_WCHAR1 | WTF_TYPE_WIDE)
#define WTF_TYPE_WCHAR2W (WTF_TYPE_WCHAR2 | WTF_TYPE_WIDE)

#define PC_ASCII (WTF_TYPE_ASCII << 8)
#define PC_CTRL (WTF_TYPE_CTRL << 8)
#define PC_WCHAR1 (WTF_TYPE_WCHAR1 << 8)
#define PC_WCHAR2 (WTF_TYPE_WCHAR2 << 8)
#define PC_KANJI (WTF_TYPE_WIDE << 8)
#define PC_KANJI1 (PC_WCHAR1 | PC_KANJI)
#define PC_KANJI2 (PC_WCHAR2 | PC_KANJI)
#define PC_UNKNOWN (WTF_TYPE_UNKNOWN << 8)
#define PC_UNDEF (WTF_TYPE_UNDEF << 8)

enum LinePropFlags : unsigned short {
  PC_SYMBOL = 0x8000,

  /* Effect ( standout/underline ) */
  P_EFFECT = 0x40ff,
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

inline Lineprop CharEffect(Lineprop c) { return c & (P_EFFECT | PC_SYMBOL); }

// byte pos to column
int bytePosToColumn(const char *l, const Lineprop *pr, int len, int pos, int bpos,
                    bool forceCalc);

struct Str;
Str *checkType(Str *s, Lineprop **oprop);
