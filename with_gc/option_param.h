#pragma once
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
  // ParamTypes type;
  ParamInputType inputtype;
  // void *varptr;
  std::string comment;
  void *select;

  param_ptr(const std::string &name, ParamInputType inputtype,
            const std::string &comment)
      : name(name), inputtype(inputtype), comment(comment) {}
  virtual ~param_ptr(){};
  virtual int setParseValue(const std::string &value) = 0;
  virtual std::string to_str() const = 0;
  virtual std::string toOptionPanelHtml() const;
  bool isSelected() const { return false; }
};

struct param_int : param_ptr {
  int *varptr;
  bool notZero;

  param_int(const std::string &name, ParamInputType inputtype, int *p,
            const std::string &comment, bool notZero)
      : param_ptr(name, inputtype, comment), varptr(p), notZero(notZero) {}
  int setParseValue(const std::string &value) override;
  std::string to_str() const override;
};

struct param_bool : param_ptr {
  bool *varptr;

  param_bool(const std::string &name, bool *p, const std::string &comment)
      : param_ptr(name, PI_ONOFF, comment), varptr(p) {}
  int setParseValue(const std::string &value) override;
  std::string to_str() const override;
};

struct param_pixels : param_ptr {
  double *varptr;

  param_pixels(const std::string &name, ParamInputType inputtype, double *p,
               const std::string &comment)
      : param_ptr(name, inputtype, comment), varptr(p) {}
  int setParseValue(const std::string &value) override;
  std::string to_str() const override;
};

int str_to_bool(const char *value, int old);
