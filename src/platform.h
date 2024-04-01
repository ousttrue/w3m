#include "func.h"

class Platform : public IPlatform {
public:
  // app.h
  std::shared_ptr<TabBuffer> currentTab() const override;
  // invoke.h
  int exec(std::string_view cmd) override;
  void invokeBrowser(std::string_view url, std::string_view browser,
                     int prec_num) override;
};
