#pragma once
#include "line.h"

/* Completion status. */
#define CPL_OK 0
#define CPL_AMBIG 1
#define CPL_FAIL 2
#define CPL_MENU 3

#define CPL_NEVER 0x0
#define CPL_OFF 0x1
#define CPL_ON 0x2
#define CPL_ALWAYS 0x4
#define CPL_URL 0x8

/* Flags for inputLine() */
#define IN_STRING 0x10
#define IN_FILENAME 0x20
#define IN_PASSWORD 0x40
#define IN_COMMAND 0x80
#define IN_URL 0x100
#define IN_CHAR 0x200

struct Hist;
struct Str;
char *inputLineHistSearch(char *prompt, char *def_str, int flag, Hist *hist,
                          int (*incfunc)(int ch, Str *buf, Lineprop *prop));

#define inputLineHist(p, d, f, h) inputLineHistSearch(p, d, f, h, nullptr)
#define inputLine(p, d, f) inputLineHist(p, d, f, nullptr)
#define inputStr(p, d) inputLine(p, d, IN_STRING)
#define inputStrHist(p, d, h) inputLineHist(p, d, IN_STRING, h)
#define inputFilename(p, d) inputLine(p, d, IN_FILENAME)
#define inputFilenameHist(p, d, h) inputLineHist(p, d, IN_FILENAME, h)
#define inputChar(p) inputLine(p, "", IN_CHAR)
