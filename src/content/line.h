#pragma once
#include <Str.h>

#define LINELEN 256 /* Initial line length */
extern int Tabstop;

typedef unsigned short Lineprop;
typedef unsigned char Linecolor;

/*
 * Line Property
 */

#define P_CHARTYPE 0x3f00
#define PC_ASCII (WTF_TYPE_ASCII << 8)
#define PC_CTRL (WTF_TYPE_CTRL << 8)
#define PC_WCHAR1 (WTF_TYPE_WCHAR1 << 8)
#define PC_WCHAR2 (WTF_TYPE_WCHAR2 << 8)
#define PC_KANJI (WTF_TYPE_WIDE << 8)
#define PC_KANJI1 (PC_WCHAR1 | PC_KANJI)
#define PC_KANJI2 (PC_WCHAR2 | PC_KANJI)
#define PC_UNKNOWN (WTF_TYPE_UNKNOWN << 8)
#define PC_UNDEF (WTF_TYPE_UNDEF << 8)
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

typedef struct _Line {
    char* lineBuf;
    Lineprop* propBuf;
    Linecolor* colorBuf;
    struct _Line* next;
    struct _Line* prev;
    int len;
    int width;
    long linenumber; /* on buffer */
    long real_linenumber; /* on file */
    unsigned short usrflags;
    int size;
    int bpos;
    int bwidth;
} Line;

enum CalcPositionMode {
    CP_AUTO = 0,
    CP_FORCE = 1,
};

int calcPosition(char* l, Lineprop* pr, int len, int pos, int bpos, enum CalcPositionMode mode);

inline static int COLPOS(struct _Line* l, int c)
{
    return calcPosition(l->lineBuf, l->propBuf, l->len, c, 0, CP_AUTO);
}

int columnPos(Line* line, int column);
int columnLen(Line* line, int column);

// mulity byte sequence ?
// set PC_WCHAR1, PC_WCHAR2
Str checkType(Str s, Lineprop** oprop, Linecolor** ocolor);
