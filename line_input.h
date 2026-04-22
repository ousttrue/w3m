#pragma once
#include <w3m.h>
#include "line.h"
#include "constants.h"
#include "LineInput.h"

typedef int (*IncrFunc)(struct CmdArgs* args, const char* str, Lineprop* prop);

char* inputLineHistSearch(struct CmdArgs* args, const char* prompt, const char* def_str,
    enum InputLineFlags flag, enum HistoryType hist, IncrFunc incfunc);

inline static char* inputLineHist(struct CmdArgs* args, const char* p, const char* d, enum InputLineFlags f, enum HistoryType h)
{
    return inputLineHistSearch(args, p, d, f, h, NULL);
}
inline static char* inputLine(struct CmdArgs* args, const char* p, const char* d, enum InputLineFlags f)
{
    return inputLineHist(args, p, d, f, HistoryNone);
}
inline static char* inputStr(struct CmdArgs* args, const char* p, const char* d)
{
    return inputLine(args, p, d, IN_STRING);
}
inline static char* inputStrHist(struct CmdArgs* args, const char* p, const char* d, enum HistoryType h)
{
    return inputLineHist(args, p, d, IN_STRING, h);
}
inline static char* inputFilename(struct CmdArgs* args, const char* p, const char* d)
{
    return inputLine(args, p, d, IN_FILENAME);
}
inline static char* inputFilenameHist(struct CmdArgs* args, const char* p, const char* d, enum HistoryType h)
{
    return inputLineHist(args, p, d, IN_FILENAME, h);
}
inline static char* inputChar(struct CmdArgs* args, const char* p)
{
    return inputLine(args, p, "", IN_CHAR);
}

char* inputAnswer(struct CmdArgs* args, const char* prompt);
