#pragma once
#include "Line.h"

enum InputFlags {
    IN_STRING = 0x10,
    IN_FILENAME = 0x20,
    IN_PASSWORD = 0x40,
    IN_COMMAND = 0x80,
    IN_URL = 0x100,
    IN_CHAR = 0x200,
};

struct Hist;
typedef int (*IncFunc)(int ch, Str buf, Lineprop* prop);
const char* inputLineHistSearch(const char* prompt, const char* def_str, enum InputFlags flag, struct Hist* hist, IncFunc incfunc);

inline static const char* inputLineHist(const char* p, const char* d, enum InputFlags f, struct Hist* h)
{
    return inputLineHistSearch(p, d, f, h, NULL);
}
inline static const char* inputLine(const char* p, const char* d, enum InputFlags f)
{
    return inputLineHist(p, d, f, NULL);
}
inline static const char* inputStr(const char* p, const char* d)
{
    return inputLine(p, d, IN_STRING);
}
inline static const char* inputStrHist(const char* p, const char* d, struct Hist* h)
{
    return inputLineHist(p, d, IN_STRING, h);
}
inline static const char* inputFilename(const char* p, const char* d)
{
    return inputLine(p, d, IN_FILENAME);
}
inline static const char* inputFilenameHist(const char* p, const char* d, struct Hist* h)
{
    return inputLineHist(p, d, IN_FILENAME, h);
}
inline static const char* inputChar(const char* p)
{
    return inputLine(p, "", IN_CHAR);
}

