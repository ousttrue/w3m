#pragma once
#include "constants.h"
#include "LineInput.h"

struct CmdArgs;
const char* inputLineHistSearch(struct CmdArgs* args, const char* prompt, const char* def_str,
    enum InputLineFlags flag, enum HistoryType hist, IncrFunc incfunc);

inline static const char* inputLineHist(struct CmdArgs* args, const char* p, const char* d, enum InputLineFlags f, enum HistoryType h)
{
    return inputLineHistSearch(args, p, d, f, h, 0);
}
inline static const char* inputLine(struct CmdArgs* args, const char* p, const char* d, enum InputLineFlags f)
{
    return inputLineHist(args, p, d, f, HistoryNone);
}
inline static const char* inputStr(struct CmdArgs* args, const char* p, const char* d)
{
    return inputLine(args, p, d, IN_STRING);
}
inline static const char* inputStrHist(struct CmdArgs* args, const char* p, const char* d, enum HistoryType h)
{
    return inputLineHist(args, p, d, IN_STRING, h);
}
inline static const char* inputFilename(struct CmdArgs* args, const char* p, const char* d)
{
    return inputLine(args, p, d, IN_FILENAME);
}
inline static const char* inputFilenameHist(struct CmdArgs* args, const char* p, const char* d, enum HistoryType h)
{
    return inputLineHist(args, p, d, IN_FILENAME, h);
}
inline static const char* inputChar(struct CmdArgs* args, const char* p)
{
    return inputLine(args, p, "", IN_CHAR);
}

const char* inputAnswer(struct CmdArgs* args, const char* prompt);
