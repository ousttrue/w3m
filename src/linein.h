#pragma once
#include "line.h"
#include <functional>

#define STR_LEN 1024

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

using KeyCallback = std::function<void(char)>;

class LineInput {
  std::array<KeyCallback, 32> InputKeymap;
  Str *strCurrentBuf = nullptr;
  Hist *CurrentHist = nullptr;
  bool move_word = true;
  bool need_redraw = false;
  bool is_passwd = false;
  bool use_hist = false;
  int cm_mode = {};
  bool cm_next = false;
  int cm_clear = {};
  int cm_disp_next = -1;
  int cm_disp_clear = {};

  // loop
  bool i_cont = true;
  bool i_broken = false;
  bool i_quote = false;

  Str *strBuf;
  int CPos = {};
  int CLen = {};

  int offset = 0;
  Lineprop strProp[STR_LEN];
  Str *CompleteBuf;
  Str *CFileName;
  Str *CBeforeBuf;
  Str *CAfterBuf;
  Str *CDirBuf;
  char **CFileBuf = NULL;
  int NCFileBuf;
  int NCFileOffset;

  void killb(char);
  void killn(char);
  void _mvE(char);
  void _mvB(char);
  void _bs(char);
  void _mvR(char);
  void _mvL(char);
  void delC(char);
  void insC(char);
  void _inbrk(char);
  void _quo(char);
  void _enter(char);
  void _tcompl(char);
  void _prev(char);
  void _next(char);
  void _esc(char);
  void _mvLw(char);
  void _mvRw(char);
  void _bsw(char);
  void insertself(char c);
  void _editor(char);
  void _compl(char);
  void _rcompl(char);
  void _dcompl(char);
  void _rdcompl(char);

  Str *doComplete(Str *ifn, int *status, int next);
  int setStrType(Str *str, Lineprop *prop);
  void next_compl(int next);
  void next_dcompl(int next);
  void addPasswd(char *p, Lineprop *pr, int len, int pos, int limit);
  void addStr(char *p, Lineprop *pr, int len, int pos, int limit);
  int terminated(unsigned char c);

public:
  LineInput(Hist *hist);
  void inputLineHistSearch(const char *prompt, const char *def_str,
                           InputFlags flag, IncFunc incFunc,
                           const OnInput &onInput);

  inline void inputLineHist(const char *p, const char *d, InputFlags f,
                            const OnInput &onInput) {
    inputLineHistSearch(p, d, f, nullptr, onInput);
  }

  inline void inputLine(const char *p, const char *d, InputFlags f,
                        const OnInput &onInput) {
    inputLineHist(p, d, f, onInput);
  }

  inline void inputStr(const char *p, const char *d, const OnInput &onInput) {
    inputLine(p, d, IN_STRING, onInput);
  }

  inline void inputStrHist(const char *p, const char *d,
                           const OnInput &onInput) {
    inputLineHist(p, d, IN_STRING, onInput);
  }

  inline void inputFilename(const char *p, const char *d,
                            const OnInput &onInput) {
    inputLine(p, d, IN_FILENAME, onInput);
  }

  inline void inputFilenameHist(const char *p, const char *d,
                                const OnInput &onInput) {
    inputLineHist(p, d, IN_FILENAME, onInput);
  }

  inline void inputChar(const char *p, const OnInput &onInput) {
    inputLine(p, "", IN_CHAR, onInput);
  }

  void inputAnswer(const char *prompt, const OnInput &onInput);
};
