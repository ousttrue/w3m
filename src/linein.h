#pragma once
#include "line.h"
#include <functional>

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

enum CompletionStatus {
  CPL_OK = 0,
  CPL_AMBIG = 1,
  CPL_FAIL = 2,
  CPL_MENU = 3,
};

enum CompletionModes {
  CPL_NEVER = 0x0,
  CPL_OFF = 0x1,
  CPL_ON = 0x2,
  CPL_ALWAYS = 0x4,
  CPL_URL = 0x8,
};

struct Hist;
struct Str;
using IncFunc = int (*)(int ch, Str *buf, Lineprop *prop);
using OnInput = std::function<void(const char *input)>;
void inputLineHistSearch(const char *prompt, const char *def_str,
                         InputFlags flag, Hist *hist, IncFunc incFunc,
                         const OnInput &onInput);

inline void inputLineHist(const char *p, const char *d, InputFlags f, Hist *h,
                          const OnInput &onInput) {
  inputLineHistSearch(p, d, f, h, nullptr, onInput);
}

inline void inputLine(const char *p, const char *d, InputFlags f,
                      const OnInput &onInput) {
  inputLineHist(p, d, f, nullptr, onInput);
}

inline void inputStr(const char *p, const char *d, const OnInput &onInput) {
  inputLine(p, d, IN_STRING, onInput);
}

// TODO:
inline void inputStrHist(const char *p, const char *d, Hist *h,
                         const OnInput &onInput) {
  inputLineHist(p, d, IN_STRING, h, onInput);
}

inline void inputFilename(const char *p, const char *d,
                          const OnInput &onInput) {
  inputLine(p, d, IN_FILENAME, onInput);
}

inline void inputFilenameHist(const char *p, const char *d, Hist *h,
                              const OnInput &onInput) {
  inputLineHist(p, d, IN_FILENAME, h, onInput);
}

void inputAnswer(const char *prompt, const OnInput &onInput);

Str *unescape_spaces(Str *s);
