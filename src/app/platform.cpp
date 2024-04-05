#include "platform.h"
#include "invoke.h"
#include "app.h"

std::shared_ptr<TabBuffer> Platform::currentTab() const {
  return App::instance().currentTab();
}

int Platform::exec(std::string_view cmd) { return exec_cmd(cmd); }

void Platform::invokeBrowser(std::string_view url, std::string_view browser) {
  return invoke_browser(url, browser, 0);
}
