#pragma once
#include "Str.h"
#include "history.h"
#include <stddef.h>

enum InputLineFlag {
    IN_STRING = 0x10,
    IN_FILENAME = 0x20,
    IN_PASSWORD = 0x40,
    IN_COMMAND = 0x80,
    IN_URL = 0x100,
    IN_CHAR = 0x200,
};

#define inputLineHist(p, d, f, h) inputLineHistSearch(p, d, f, h, NULL)
#define inputLine(p, d, f) inputLineHist(p, d, f, NULL)
#define inputStr(p, d) inputLine(p, d, IN_STRING)
#define inputStrHist(p, d, h) inputLineHist(p, d, IN_STRING, h)
#define inputFilename(p, d) inputLine(p, d, IN_FILENAME)
#define inputFilenameHist(p, d, h) inputLineHist(p, d, IN_FILENAME, h)
#define inputChar(p) inputLine(p, "", IN_CHAR)

typedef int (*IncrFunc)(int ch, Str buf, Lineprop* prop);

extern char* inputLineHistSearch(const char* prompt, const char* def_str,
    enum InputLineFlag flag, Hist* hist, IncrFunc incfunc);
