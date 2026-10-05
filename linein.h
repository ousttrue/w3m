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
    enum InputLineFlag flags, Hist* history, IncrFunc incfunc);

static inline struct Str inputLineHist(const char* prompt, const char* def_str, enum InputLineFlag flags, Hist* history)
{
    return inputLineHistSearch(prompt, def_str, flags, history, NULL);
}

static inline struct Str inputLine(const char* prompt, const char* def_str, enum InputLineFlag flags)
{
    return inputLineHist(prompt, def_str, flags, NULL);
}

static inline struct Str inputStr(const char* prompt, const char* def_str)
{
    return inputLine(prompt, def_str, IN_STRING);
}

static inline struct Str inputStrHist(const char* prompt, const char* def_str, Hist* history)
{
    return inputLineHist(prompt, def_str, IN_STRING, history);
}

static inline struct Str inputFilename(const char* prompt, const char* def_str)
{
    return inputLine(prompt, def_str, IN_FILENAME);
}

static inline struct Str inputFilenameHist(const char* prompt, const char* def_str, Hist* history)
{
    return inputLineHist(prompt, def_str, IN_FILENAME, history);
}

static inline struct Str inputChar(const char* prompt)
{
    return inputLine(prompt, "", IN_CHAR);
}

bool confirm(const char* prompt);

bool canOverWrite(const char* path);
