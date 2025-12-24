#pragma once
#include "line.h"

enum LineInputFlags {
    IN_STRING = 0x10,
    IN_FILENAME = 0x20,
    IN_PASSWORD = 0x40,
    IN_COMMAND = 0x80,
    IN_URL = 0x100,
    IN_CHAR = 0x200,
};

typedef int (*IncFunc)(int ch, Str buf, Lineprop* prop);

struct Hist;
char* inputLineHistSearch(const char* prompt, const char* def_str, enum LineInputFlags flag, struct Hist* hist, IncFunc incfunc);
inline static char* inputLineHist(const char* p, const char* d, enum LineInputFlags f, struct Hist* h)
{
    return inputLineHistSearch(p, d, f, h, NULL);
}
inline static char* inputLine(const char* p, const char* d, enum LineInputFlags f)
{
    return inputLineHist(p, d, f, NULL);
}
inline static char* inputStr(const char* p, const char* d)
{
    return inputLine(p, d, IN_STRING);
}
inline static char* inputStrHist(const char* p, const char* d, struct Hist* h)
{
    return inputLineHist(p, d, IN_STRING, h);
}
inline static char* inputFilename(const char* p, const char* d)
{
    return inputLine(p, d, IN_FILENAME);
}
inline static char* inputFilenameHist(const char* p, const char* d, struct Hist* h)
{
    return inputLineHist(p, d, IN_FILENAME, h);
}
inline static char* inputChar(const char* p)
{
    return inputLine(p, "", IN_CHAR);
}
char* inputAnswer(const char* prompt);
