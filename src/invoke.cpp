#include "invoke.h"
#include "quote.h"
#include "myctype.h"
#include <assert.h>
#include <sstream>

#ifdef _MSC_VER
#else
#include <unistd.h>
#endif

#define DEF_EXT_BROWSER "/usr/bin/firefox"
std::string ExtBrowser = DEF_EXT_BROWSER;
std::string ExtBrowser2;
std::string ExtBrowser3;
std::string ExtBrowser4;
std::string ExtBrowser5;
std::string ExtBrowser6;
std::string ExtBrowser7;
std::string ExtBrowser8;
std::string ExtBrowser9;

int exec_cmd(const std::string &cmd) {
  // App::instance().endRawMode();
  if (auto rv = system(cmd.c_str())) {
    printf("\n[Hit any key]");
    fflush(stdout);
    // App::instance().beginRawMode();
    // getch();
    return rv;
  }
  // App::instance().endRawMode();
  return 0;
}

std::string myExtCommand(std::string_view cmd, std::string_view arg,
                         bool redirect) {
  std::stringstream tmp;

  bool set_arg = false;

  for (auto p = cmd.begin(); p != cmd.end(); p++) {
    if (*p == '%' && *(p + 1) == 's' && !set_arg) {
      tmp << arg;
      set_arg = true;
      p++;
    } else {
      tmp << *p;
    }
  }

  if (!set_arg) {
    tmp.str("");
    if (redirect)
      tmp << "(" << cmd << ") < " << arg;
    else
      tmp << cmd << " " << arg;
  }

  return tmp.str();
}

// #ifdef _MSC_VER
#if 1
static void mySystem(const std::string &command, bool background) {
  assert(false);
}
#else
static void myExec(const char *command) {
  // mySignal(SIGINT, SIG_DFL);
  execl("/bin/sh", "sh", "-c", command, 0);
  exit(127);
}

static void mySystem(const std::string &command, bool background) {
  if (background) {
    // flush_tty();
    if (!fork()) {
      setup_child(false, 0, -1);
      myExec(command);
    }
  } else {
    system(command.c_str());
  }
}
#endif

/* spawn external browser */
void invoke_browser(std::string_view url, std::string_view browser,
                    int prec_num) {

  if (browser.empty()) {
    switch (prec_num) {
    case 0:
    case 1:
      browser = ExtBrowser;
      break;
    case 2:
      browser = ExtBrowser2;
      break;
    case 3:
      browser = ExtBrowser3;
      break;
    case 4:
      browser = ExtBrowser4;
      break;
    case 5:
      browser = ExtBrowser5;
      break;
    case 6:
      browser = ExtBrowser6;
      break;
    case 7:
      browser = ExtBrowser7;
      break;
    case 8:
      browser = ExtBrowser8;
      break;
    case 9:
      browser = ExtBrowser9;
      break;
    }
    if (browser.empty()) {
      // browser = inputStr("Browse command: ", nullptr);
    }
  }
  if (browser.empty()) {
    return;
  }

  bool bg = false;
  if ((browser.size()) >= 2 && browser.back() == '&' &&
      browser[browser.size() - 2] != '\\') {
    browser = browser.substr(0, browser.size() - 2);
    bg = true;
  }

  auto cmd = myExtCommand(browser, shell_quote(url), false);
  // Strremovetrailingspaces(cmd);
  int i = cmd.size() - 1;
  for (; i >= 0 && IS_SPACE(cmd[i]); i--)
    ;
  cmd = cmd.substr(0, i);
  mySystem(cmd, bg);
  // App::instance().endRawMode();
  // App::instance().beginRawMode();
}
