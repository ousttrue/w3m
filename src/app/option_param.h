#pragma once
#include <string>

int str_to_bool(const char *value, int old);

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

struct sel_c {
  int value;
  const char *cvalue;
  const char *text;
};

struct param_ptr {
  std::string name;
  std::string comment;

  param_ptr(const std::string &name, const std::string &comment)
      : name(name), comment(comment) {}
  virtual ~param_ptr(){};
  virtual int setParseValue(const std::string &value) = 0;
  virtual std::string to_str() const = 0;
  virtual std::string toOptionPanelHtml() const;
};

// value: int
// input: text
struct param_int : param_ptr {
  int *varptr;
  bool notZero;

  param_int(const std::string &name, int *p, const std::string &comment,
            bool notZero)
      : param_ptr(name, comment), varptr(p), notZero(notZero) {}
  int setParseValue(const std::string &value) override;
  std::string to_str() const override;
};

// value: int
// input: select
struct param_int_select : param_int {
  int *varptr;
  bool notZero;
  sel_c *select;

  param_int_select(const std::string &name, int *p, const std::string &comment,
                   sel_c *select)
      : param_int(name, p, comment, false), select(select) {}
  // int setParseValue(const std::string &value) override;
  // std::string to_str() const override;
  std::string toOptionPanelHtml() const override;
};

// value: double
// input: text
struct param_pixels : param_ptr {
  double *varptr;

  param_pixels(const std::string &name, double *p, const std::string &comment)
      : param_ptr(name, comment), varptr(p) {}
  int setParseValue(const std::string &value) override;
  std::string to_str() const override;
};

// value: bool
// input: checkbox
struct param_bool : param_ptr {
  bool *varptr;

  param_bool(const std::string &name, bool *p, const std::string &comment)
      : param_ptr(name, comment), varptr(p) {}
  int setParseValue(const std::string &value) override;
  std::string to_str() const override;
  std::string toOptionPanelHtml() const override;
};

// value: std::string
// input: text
struct param_string : param_ptr {
  std::string *varptr;

  param_string(const std::string &name, std::string *p,
               const std::string &comment)
      : param_ptr(name, comment), varptr(p) {}
  int setParseValue(const std::string &value) override;
  std::string to_str() const override;
};
