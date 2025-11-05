#pragma once
#include "Line.h"

/* Flags for inputLine() */
#define IN_STRING 0x10
#define IN_FILENAME 0x20
#define IN_PASSWORD 0x40
#define IN_COMMAND 0x80
#define IN_URL 0x100
#define IN_CHAR 0x200

struct Hist;
char* inputLineHistSearch(char* prompt, char* def_str, int flag, struct Hist* hist, int (*incfunc)(int ch, Str buf, Lineprop* prop));

#define inputLineHist(p, d, f, h) inputLineHistSearch(p, d, f, h, NULL)
#define inputLine(p, d, f) inputLineHist(p, d, f, NULL)
#define inputStr(p, d) inputLine(p, d, IN_STRING)
#define inputStrHist(p, d, h) inputLineHist(p, d, IN_STRING, h)
#define inputFilename(p, d) inputLine(p, d, IN_FILENAME)
#define inputFilenameHist(p, d, h) inputLineHist(p, d, IN_FILENAME, h)
#define inputChar(p) inputLine(p, "", IN_CHAR)

Str unescape_spaces(Str s);
