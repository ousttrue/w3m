#pragma once
#include "history.h"

typedef unsigned short Lineprop;

typedef int (*IncFunc)(int ch, Str buf, Lineprop* prop);

char* inputLineHistSearch(char* prompt, char* def_str, int flag,
    Hist* hist, IncFunc incfunc);

#define inputLineHist(p, d, f, h) inputLineHistSearch(p, d, f, h, NULL)
#define inputLine(p, d, f) inputLineHist(p, d, f, NULL)
#define inputStr(p, d) inputLine(p, d, IN_STRING)
#define inputStrHist(p, d, h) inputLineHist(p, d, IN_STRING, h)
#define inputFilename(p, d) inputLine(p, d, IN_FILENAME)
#define inputFilenameHist(p, d, h) inputLineHist(p, d, IN_FILENAME, h)
#define inputChar(p) inputLine(p, "", IN_CHAR)
