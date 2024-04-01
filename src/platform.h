#include "func.h"

class Platform : public IPlatform {
  bool _isRuuning = true;

public:
  void quit() override { _isRuuning = false; }
  bool isRunning() const { return _isRuuning; }
  // app.h
  std::shared_ptr<TabBuffer> currentTab() const override;
  // invoke.h
  int exec(std::string_view cmd) override;
  void invokeBrowser(std::string_view url, std::string_view browser) override;
};
