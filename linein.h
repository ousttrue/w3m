#pragma once
#include "Str.h"
#include "line.h"

struct Hist;

struct Document;
typedef int (*IncFunc)(struct Document *doc, int ch, Str buf, Lineprop *prop);

/* Flags for inputLine() */
enum InputlineFlags {
  IN_STRING = 0x10,
  IN_FILENAME = 0x20,
  IN_PASSWORD = 0x40,
  IN_COMMAND = 0x80,
  IN_URL = 0x100,
  IN_CHAR = 0x200,
};

const char *inputLineHistSearch(struct Document *doc, const char *prompt,
                                const char *def_str, enum InputlineFlags flag,
                                struct Hist *hist, IncFunc incfunc);
const char *inputLineHist(struct Document *doc, const char *p, const char *d,
                          enum InputlineFlags f, struct Hist *h);
const char *inputLine(struct Document *doc, const char *p, const char *d,
                      enum InputlineFlags f);
const char *inputStr(struct Document *doc, const char *p, const char *d);
const char *inputStrHist(struct Document *doc, const char *p, const char *d,
                         struct Hist *h);
const char *inputFilenameHist(struct Document *doc, const char *p,
                              const char *d, struct Hist *h);
const char *inputChar(struct Document *doc, const char *p);
