#pragma once
#include "LineEditor.h"
#include "ui.h"

struct Hist;

typedef int (*IncFunc)(int ch, Str buf, Lineprop* prop);

char* inputLineHistSearch(struct UI ui,
    const char* prompt, const char* def_str, enum InputLineFlags flag,
    struct Hist* hist, IncFunc incfunc);

static inline char* inputLineHist(struct UI ui,
    const char* p, const char* d, enum InputLineFlags f, struct Hist* h)
{
    return inputLineHistSearch(ui, p, d, f, h, NULL);
}

static inline char* inputLine(struct UI ui,
    const char* p, const char* d, enum InputLineFlags f)
{
    return inputLineHist(ui, p, d, f, NULL);
}

static inline char* inputStr(struct UI ui,
    const char* p, const char* d)
{
    return inputLine(ui, p, d, IN_STRING);
}

static inline char* inputStrHist(struct UI ui,
    const char* p, const char* d, struct Hist* h)
{
    return inputLineHist(ui, p, d, IN_STRING, h);
}

static inline char* inputFilename(struct UI ui,
    const char* p, const char* d)
{
    return inputLine(ui, p, d, IN_FILENAME);
}

static inline char* inputFilenameHist(struct UI ui,
    const char* p, const char* d, struct Hist* h)
{
    return inputLineHist(ui, p, d, IN_FILENAME, h);
}

static inline char* inputChar(struct UI ui,
    const char* p)
{
    return inputLine(ui, p, "", IN_CHAR);
}
