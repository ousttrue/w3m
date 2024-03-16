#pragma once
#include <vector>
#include <unordered_map>
#include <list>
#include <string>
#include <memory>

struct param_section {
  std::string name;
  std::list<std::shared_ptr<struct param_ptr>> params;
};

#define OPTION_DISPLAY "Display Settings"
#define OPTION_MICC "Miscellaneous Settings"
#define OPTION_DIRECTORY "Directory Settings"
#define OPTION_EXTERNAL_PROGRAM "External Program Settings"
#define OPTION_NETWORK "Network Settings"
#define OPTION_PROXY "Proxy Settings"
#define OPTION_SSL "SSL Settings"
#define OPTION_COOKIE "Cookie Settings"

class Option {
  std::vector<param_section> sections = {
      {OPTION_DISPLAY},   {OPTION_MICC},
      {OPTION_DIRECTORY}, {OPTION_EXTERNAL_PROGRAM},
      {OPTION_NETWORK},   {OPTION_PROXY},
      {OPTION_SSL},       {OPTION_COOKIE},
  };

  std::unordered_map<std::string, std::shared_ptr<param_ptr>> RC_search_table;

  Option();

public:
  ~Option();
  Option(const Option &) = delete;
  Option &operator=(const Option &) = delete;
  static Option &instance();
  void create_option_search_table();

  void push_param(const std::string &section,
                  const std::shared_ptr<param_ptr> &param);
  void push_param_int(const std::string &section, const std::string &name,
                      int *p, const std::string &comment, bool notZero);
  void push_param_int_select(const std::string &section,
                             const std::string &name, int *p,
                             const std::string &comment, struct sel_c *select);
  void push_param_bool(const std::string &section, const std::string &name,
                       bool *p, const std::string &comment);
  void push_param_string(const std::string &section, const std::string &name,
                         std::string *p, const std::string &comment);

  std::shared_ptr<param_ptr> search_param(const std::string &name) const;
  std::string get_param_option(const char *name) const;
  int set_param(const std::string &name, const std::string &value);
  std::string load_option_panel();
};
