#pragma once
#include "line.h"

extern bool space_autocomplete;
extern bool emacs_like_lineedit;

/* Flags for inputLine() */
enum InputFlags {
  IN_STRING = 0x10,
  IN_FILENAME = 0x20,
  IN_PASSWORD = 0x40,
  IN_COMMAND = 0x80,
  IN_URL = 0x100,
  IN_CHAR = 0x200,
};

struct Hist;
struct Str;
using IncFunc = int (*)(int ch, Str *buf, Lineprop *prop);
const char *inputLineHistSearch(const char *prompt, const char *def_str,
                                InputFlags flag, Hist *hist, IncFunc incFunc);

inline const char *inputLineHist(const char *p, const char *d, InputFlags f,
                                 Hist *h) {
  return inputLineHistSearch(p, d, f, h, nullptr);
}
inline const char *inputLine(const char *p, const char *d, InputFlags f) {
  return inputLineHist(p, d, f, nullptr);
}
inline const char *inputStr(const char *p, const char *d) {
  return inputLine(p, d, IN_STRING);
}
inline const char *inputStrHist(const char *p, const char *d, Hist *h) {
  return inputLineHist(p, d, IN_STRING, h);
}
inline const char *inputFilename(const char *p, const char *d) {
  return inputLine(p, d, IN_FILENAME);
}

// inline char *inputFilenameHist(const char *p, const char *d, Hist *h) {
//   return inputLineHist(p, d, IN_FILENAME, h);
// }

// const char *inputAnswer(const char *prompt);
