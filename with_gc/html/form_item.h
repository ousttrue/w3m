#pragma once
#include <string>
#include <memory>

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
FormItemType formtype(const char *typestr);

struct Str;
struct Form;
struct FormItem {
  FormItemType type;
  Str *name;
  Str *value;
  Str *init_value;
  int checked;
  int init_checked;
  int accept;
  int size;
  int rows;
  int maxlength;
  int readonly;

  std::shared_ptr<Form> parent;

  std::shared_ptr<FormItem> next;

  Str *query_from_followform();
  void query_from_followform_multipart();
  std::string form2str() const;

  void input_textarea();
};
