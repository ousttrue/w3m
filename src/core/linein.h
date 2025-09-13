#pragma once
#include "LineEditor.h"
#include "geometry.h"

extern int space_autocomplete;
extern int emacs_like_lineedit;

struct Hist;

typedef int (*IncFunc)(struct UI ui, int ch, Str buf, Lineprop* prop);

const char* inputLineHistSearch(struct UI ui,
    const char* prompt, const char* def_str, enum InputLineFlags flag,
    struct Hist* hist, IncFunc incfunc);

static inline const char* inputLineHist(struct UI ui,
    const char* p, const char* d, enum InputLineFlags f, struct Hist* h)
{
    return inputLineHistSearch(ui, p, d, f, h, NULL);
}

static inline const char* inputLine(struct UI ui,
    const char* p, const char* d, enum InputLineFlags f)
{
    return inputLineHist(ui, p, d, f, NULL);
}

static inline const char* inputStr(struct UI ui,
    const char* p, const char* d)
{
    return inputLine(ui, p, d, IN_STRING);
}

static inline const char* inputStrHist(struct UI ui,
    const char* p, const char* d, struct Hist* h)
{
    return inputLineHist(ui, p, d, IN_STRING, h);
}

static inline const char* inputFilename(struct UI ui,
    const char* p, const char* d)
{
    return inputLine(ui, p, d, IN_FILENAME);
}

static inline const char* inputFilenameHist(struct UI ui,
    const char* p, const char* d, struct Hist* h)
{
    return inputLineHist(ui, p, d, IN_FILENAME, h);
}

static inline const char* inputChar(struct UI ui,
    const char* p)
{
    return inputLine(ui, p, "", IN_CHAR);
}

const char* inputAnswer(const char* prompt);
bool notExistsOrOverWrite(const char* path);
