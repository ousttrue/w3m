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

class Option {
  std::vector<param_section> sections;
  std::unordered_map<std::string, std::shared_ptr<param_ptr>> RC_search_table;

  Option();

public:
  ~Option();
  Option(const Option &) = delete;
  Option &operator=(const Option &) = delete;
  static Option &instance();
  void create_option_search_table();
  std::shared_ptr<param_ptr> search_param(const std::string &name) const;
  std::string get_param_option(const char *name) const;
  int set_param(const std::string &name, const std::string &value);
  std::string load_option_panel();
};
