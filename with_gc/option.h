#pragma once
#include <vector>
#include <unordered_map>
#include <list>
#include <string>

enum ParamTypes {
  P_INT = 0,
  P_SHORT = 1,
  P_CHARINT = 2,
  P_CHAR = 3,
  P_STRING = 4,
  P_SSLPATH = 5,
  P_PIXELS = 8,
  P_NZINT = 9,
  P_SCALE = 10,
};

enum ParamInputType {
  PI_TEXT = 0,
  PI_ONOFF = 1,
  PI_SEL_C = 2,
};

struct sel_c {
  int value;
  const char *cvalue;
  const char *text;
};

struct param_ptr {
  std::string name;
  ParamTypes type;
  ParamInputType inputtype;
  void *varptr;
  const char *comment;
  void *select;

  int setParseValue(const std::string &value);
  std::string to_str() const;
};

struct param_section {
  std::string name;
  std::list<param_ptr> params;
};

class Option {
  std::vector<param_section> sections;
  std::unordered_map<std::string, param_ptr *> RC_search_table;

  Option();

public:
  ~Option();
  Option(const Option &) = delete;
  Option &operator=(const Option &) = delete;
  static Option &instance();
  void create_option_search_table();
  param_ptr *search_param(const std::string &name);
  int set_param(const std::string &name, const std::string &value);
  std::string load_option_panel();
};
