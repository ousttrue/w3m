#pragma once
#include <Str.h>
#include "line.h"

struct Hist;

typedef int (*IncFunc)(int ch, Str buf, Lineprop* prop);

enum InputLineFlags {
    IN_STRING = 0x10,
    IN_FILENAME = 0x20,
    IN_PASSWORD = 0x40,
    IN_COMMAND = 0x80,
    IN_URL = 0x100,
    IN_CHAR = 0x200,
};

char* inputLineHistSearch(const char* prompt, const char* def_str, enum InputLineFlags flag,
    struct Hist* hist, IncFunc incfunc);

static inline char* inputLineHist(const char* p, const char* d, enum InputLineFlags f, struct Hist* h)
{
    return inputLineHistSearch(p, d, f, h, NULL);
}

static inline char* inputLine(const char* p, const char* d, enum InputLineFlags f)
{
    return inputLineHist(p, d, f, NULL);
}

static inline char* inputStr(const char* p, const char* d)
{
    return inputLine(p, d, IN_STRING);
}

static inline char* inputStrHist(const char* p, const char* d, struct Hist* h)
{
    return inputLineHist(p, d, IN_STRING, h);
}

static inline char* inputFilename(const char* p, const char* d) { return inputLine(p, d, IN_FILENAME); }
static inline char* inputFilenameHist(const char* p, const char* d, struct Hist* h) { return inputLineHist(p, d, IN_FILENAME, h); }
static inline char* inputChar(const char* p) { return inputLine(p, "", IN_CHAR); }
