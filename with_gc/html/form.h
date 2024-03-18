#pragma once
#include "anchor.h"
#include <memory>

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
struct HtmlTag;

struct Form : public std::enable_shared_from_this<Form> {
  std::string action;
  FormMethod method = FORM_METHOD_GET;
  FormEncoding enctype = FORM_ENCTYPE_URLENCODED;
  std::string target;
  std::string name;
  std::vector<std::shared_ptr<FormItem>> items;

  char *boundary;
  char *body = nullptr;
  unsigned long length = 0;

  Form(){};
  Form(const std::string &action, const std::string &method,
       const std::string &enctype, const std::string &target,
       const std::string &name);

  ~Form();
  std::shared_ptr<FormItem> formList_addInput(struct html_feed_environ *h_env,
                                              HtmlTag *tag);
};

struct FormAnchor : public Anchor {
  std::shared_ptr<FormItem> formItem;

  short y = 0;
  short rows = 0;
};

std::string form_quote(std::string_view x);
