#pragma once
#include <memory>
#include <string>
#include <vector>
#include <string>
#include <memory>
#include "anchor.h"

enum FormItemType {
  FORM_UNKNOWN = -1,
  FORM_INPUT_TEXT = 0,
  FORM_INPUT_PASSWORD = 1,
  FORM_INPUT_CHECKBOX = 2,
  FORM_INPUT_RADIO = 3,
  FORM_INPUT_SUBMIT = 4,
  FORM_INPUT_RESET = 5,
  FORM_INPUT_HIDDEN = 6,
  FORM_INPUT_IMAGE = 7,
  FORM_SELECT = 8,
  FORM_TEXTAREA = 9,
  FORM_INPUT_BUTTON = 10,
  FORM_INPUT_FILE = 11,
};
FormItemType formtype(const std::string &typestr);

struct Form;
class HtmlTag;
struct FormItem {
  FormItemType type;
  std::string name;
  std::string value;
  std::string init_value;
  int checked;
  int init_checked;
  int accept;
  int size;
  int rows;
  int maxlength;
  int readonly;

  std::shared_ptr<Form> parent;

  void query_from_followform_multipart(int pid);
  std::string form2str() const;
};

struct FormAnchor : public Anchor {
  std::shared_ptr<FormItem> formItem;
  short y = 0;
  short rows = 0;

  FormAnchor() {}
  FormAnchor(const BufferPoint &bp, const std::shared_ptr<FormItem> &item)
      : formItem(item) {
    start = bp;
    end = bp;
  }
};
enum FormMethod {
  FORM_METHOD_GET = 0,
  FORM_METHOD_POST = 1,
  FORM_METHOD_INTERNAL = 2,
  FORM_METHOD_HEAD = 3,
};

enum FormEncoding {
  FORM_ENCTYPE_URLENCODED = 0,
  FORM_ENCTYPE_MULTIPART = 1,
};

struct FormItem;
struct Form : public std::enable_shared_from_this<Form> {
  std::string action;
  FormMethod method = FORM_METHOD_GET;
  FormEncoding enctype = FORM_ENCTYPE_URLENCODED;
  std::string target;
  std::string name;
  std::vector<std::shared_ptr<FormItem>> items;

  std::string boundary;
  std::string body;
  unsigned long length = 0;

  Form(){};
  Form(const std::string &action, const std::string &method,
       const std::string &enctype, const std::string &target,
       const std::string &name);
  ~Form();

  std::string query(const std::shared_ptr<FormItem> &item) const;
};

std::string form_quote(std::string_view x);
