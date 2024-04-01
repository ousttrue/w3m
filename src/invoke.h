#pragma once
#include <string>

extern std::string ExtBrowser;
extern std::string ExtBrowser2;
extern std::string ExtBrowser3;
extern std::string ExtBrowser4;
extern std::string ExtBrowser5;
extern std::string ExtBrowser6;
extern std::string ExtBrowser7;
extern std::string ExtBrowser8;
extern std::string ExtBrowser9;

int exec_cmd(std::string_view cmd);

std::string myExtCommand(std::string_view cmd, std::string_view arg,
                         bool redirect);

void invoke_browser(std::string_view url, std::string_view browser,
                    int prec_num);
