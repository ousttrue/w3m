#pragma once
#include "Str.h"
#include "line.h"

struct Hist;

typedef int (*IncFunc)(int ch, Str buf, Lineprop *prop);

/* Flags for inputLine() */
enum InputlineFlags {
  IN_STRING = 0x10,
  IN_FILENAME = 0x20,
  IN_PASSWORD = 0x40,
  IN_COMMAND = 0x80,
  IN_URL = 0x100,
  IN_CHAR = 0x200,
};

const char *inputLineHistSearch(const char *prompt, const char *def_str,
                                enum InputlineFlags flag, struct Hist *hist,
                                IncFunc incfunc);
const char *inputLineHist(const char *p, const char *d, enum InputlineFlags f,
                          struct Hist *h);
const char *inputLine(const char *p, const char *d, enum InputlineFlags f);
const char *inputStr(const char *p, const char *d);
const char *inputStrHist(const char *p, const char *d, struct Hist *h);
const char *inputFilenameHist(const char *p, const char *d, struct Hist *h);
const char *inputChar(const char *p);
