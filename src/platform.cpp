#include "platform.h"
#include "invoke.h"

int Platform::exec(std::string_view cmd) { return exec_cmd(cmd); }

void Platform::invokeBrowser(std::string_view url, std::string_view browser,
                             int prec_num) {
  return invoke_browser(url, browser, prec_num);
}
