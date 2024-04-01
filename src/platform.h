#include "func.h"

class Platform : public IPlatform {
public:
  int exec(std::string_view cmd) override;
  void invokeBrowser(std::string_view url, std::string_view browser,
                     int prec_num) override;
};
