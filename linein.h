#pragma once
#include "Str.h"
#include "history.h"

enum InputLineFlag {
    IN_STRING = 0x10,
    IN_FILENAME = 0x20,
    IN_PASSWORD = 0x40,
    IN_COMMAND = 0x80,
    IN_URL = 0x100,
    IN_CHAR = 0x200,
};

typedef int (*IncrFunc)(int ch, pStr buf, Lineprop* prop);

struct Str inputLineHistSearch(const char* prompt, const char* def_str,
    enum InputLineFlag flag, Hist* hist, IncrFunc incfunc);

static inline struct Str inputLineHist(const char* p, const char* d, enum InputLineFlag f, Hist* h)
{
    return inputLineHistSearch(p, d, f, h, NULL);
}

static inline struct Str inputLine(const char* p, const char* d, enum InputLineFlag f)
{
    return inputLineHist(p, d, f, NULL);
}

static inline struct Str inputStr(const char* p, const char* d)
{
    return inputLine(p, d, IN_STRING);
}

static inline struct Str inputStrHist(const char* p, const char* d, Hist* h)
{
    return inputLineHist(p, d, IN_STRING, h);
}

static inline struct Str inputFilename(const char* p, const char* d)
{
    return inputLine(p, d, IN_FILENAME);
}

static inline struct Str inputFilenameHist(const char* p, const char* d, Hist* h)
{
    return inputLineHist(p, d, IN_FILENAME, h);
}

static inline struct Str inputChar(const char* p)
{
    return inputLine(p, "", IN_CHAR);
}
