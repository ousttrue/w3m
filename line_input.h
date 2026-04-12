#pragma once
#include "defun_impl.h"
#include "Str.h"
#include "line.h"

enum InputLineFlags {
    IN_STRING = 0x10,
    IN_FILENAME = 0x20,
    IN_PASSWORD = 0x40,
    IN_COMMAND = 0x80,
    IN_URL = 0x100,
    IN_CHAR = 0x200,
};

struct Hist;

typedef int (*IncrFunc)(int ch, Str buf, Lineprop* prop);

char* inputLineHistSearch(struct CmdArgs args, const char* prompt, const char* def_str,
    enum InputLineFlags flag, struct Hist* hist, IncrFunc incfunc);

inline static char* inputLineHist(struct CmdArgs args, const char* p, const char* d, enum InputLineFlags f, struct Hist* h)
{
    return inputLineHistSearch(args, p, d, f, h, NULL);
}
inline static char* inputLine(struct CmdArgs args, const char* p, const char* d, enum InputLineFlags f)
{
    return inputLineHist(args, p, d, f, NULL);
}
inline static char* inputStr(struct CmdArgs args, const char* p, const char* d)
{
    return inputLine(args, p, d, IN_STRING);
}
inline static char* inputStrHist(struct CmdArgs args, const char* p, const char* d, struct Hist* h)
{
    return inputLineHist(args, p, d, IN_STRING, h);
}
inline static char* inputFilename(struct CmdArgs args, const char* p, const char* d)
{
    return inputLine(args, p, d, IN_FILENAME);
}
inline static char* inputFilenameHist(struct CmdArgs args, const char* p, const char* d, struct Hist* h)
{
    return inputLineHist(args, p, d, IN_FILENAME, h);
}
inline static char* inputChar(struct CmdArgs args, const char* p)
{
    return inputLine(args, p, "", IN_CHAR);
}

char* inputAnswer(struct CmdArgs args, const char* prompt);
