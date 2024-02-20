#pragma once
#include "line.h"
#include <functional>
#include <string>
#include <memory>
#include <array>

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
  std::shared_ptr<class Screen> _screen;
  std::string prompt;
  int opos = 0;
  int epos = 0;
  int lpos = 0;
  int rpos = 0;
  InputFlags flag = {};
  OnInput onInput;

  std::array<KeyCallback, 32> InputKeymap;
  Hist *CurrentHist = nullptr;
  IncFunc incrfunc = nullptr;

  Str *strCurrentBuf = nullptr;
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
  // void _esc(char);
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
  // void addPasswd(char *p, Lineprop *pr, int len, int pos, int limit);
  // void addStr(char *p, Lineprop *pr, int len, int pos, int limit);
  int terminated(unsigned char c);

  LineInput(const std::shared_ptr<Screen> &screen, const char *prompt,
            Hist *hist, const OnInput &onInput, IncFunc incrfunc);

public:
  LineInput(const LineInput &) = delete;
  LineInput &operator=(const LineInput &) = delete;
  bool dispatch(const char *buf, int len);
  void draw();
  void onBreak();

  static std::shared_ptr<LineInput>
  inputLineHistSearch(const std::shared_ptr<Screen> &screen,
                      const char *prompt, const char *def_str, Hist *hist,
                      InputFlags flag, const OnInput &onInput,
                      IncFunc incrfunc = {});

  static std::shared_ptr<LineInput>
  inputLineHist(const std::shared_ptr<Screen> &screen, const char *prompt,
                const char *def_str, Hist *hist, InputFlags f,
                const OnInput &onInput) {
    return inputLineHistSearch(screen, prompt, def_str, hist, f, onInput);
  }

  static std::shared_ptr<LineInput>
  inputLine(const std::shared_ptr<Screen> &screen, const char *prompt,
            const char *d, InputFlags f, const OnInput &onInput) {
    return inputLineHist(screen, prompt, d, {}, f, onInput);
  }

  static std::shared_ptr<LineInput>
  inputStr(const std::shared_ptr<Screen> &screen, const char *prompt,
           const char *d, const OnInput &onInput) {
    return inputLine(screen, prompt, d, IN_STRING, onInput);
  }

  static std::shared_ptr<LineInput>
  inputStrHist(const std::shared_ptr<Screen> &screen, const char *prompt,
               const char *d, Hist *hist, const OnInput &onInput) {
    return inputLineHist(screen, prompt, d, hist, IN_STRING, onInput);
  }

  static std::shared_ptr<LineInput>
  inputFilename(const std::shared_ptr<Screen> &screen, const char *prompt,
                const char *d, const OnInput &onInput) {
    return inputLine(screen, prompt, d, IN_FILENAME, onInput);
  }

  static std::shared_ptr<LineInput>
  inputFilenameHist(const std::shared_ptr<Screen> &screen,
                    const char *prompt, const char *d, const OnInput &onInput) {
    return inputLineHist(screen, prompt, d, {}, IN_FILENAME, onInput);
  }

  static std::shared_ptr<LineInput>
  inputChar(const std::shared_ptr<Screen> &screen, const char *prompt,
            const OnInput &onInput) {
    return inputLine(screen, prompt, "", IN_CHAR, onInput);
  }

  static std::shared_ptr<LineInput>
  inputAnswer(const std::shared_ptr<Screen> &screen,
              const OnInput &onInput);
};
