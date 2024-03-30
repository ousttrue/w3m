#pragma once
#include <memory>
#include <string>

extern bool IgnoreCase;
extern bool WrapSearch;
extern bool show_srch_str;

enum SearchResult {
  SR_FOUND = 0x1,
  SR_NOTFOUND = 0x2,
  SR_WRAPPED = 0x4,
};

struct LineLayout;
using SearchFunc = int (*)(LineLayout *layout, const std::string &str);
int forwardSearch(LineLayout *layout, const std::string &str);
int backwardSearch(LineLayout *layout, const std::string &str);
void isrch(const std::shared_ptr<LineLayout> &layout, const SearchFunc &func,
           const std::string &str);
void srch(const std::shared_ptr<LineLayout> &layout, const SearchFunc &func,
          const std::string &str);
void srch_nxtprv(const std::shared_ptr<LineLayout> &layout,
                 const std::string &str, int reverse);
