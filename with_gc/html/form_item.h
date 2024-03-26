#pragma once
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
FormItemType formtype(const char *typestr);

struct Form;
struct html_feed_environ;
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

  static std::shared_ptr<FormItem>
  createFromInput(html_feed_environ *h_env,
                  const std::shared_ptr<HtmlTag> &tag);

  std::shared_ptr<Form> parent;

  void query_from_followform_multipart();
  std::string form2str() const;

  void input_textarea();
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
