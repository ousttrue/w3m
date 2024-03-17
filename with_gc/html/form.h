#pragma once
#include "Str.h"
#include "anchorlist.h"

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

struct FormItemList;
struct HtmlTag;

struct Form {
  FormItemList *item;
  FormItemList *lastitem;
  FormMethod method;
  Str *action;
  const char *target;
  const char *name;
  FormEncoding enctype;
  Form *next;
  int nitems;
  char *body;
  char *boundary;
  unsigned long length;

  FormItemList *formList_addInput(struct html_feed_environ *h_env,
                                  HtmlTag *tag);
};

struct FormAnchor : public Anchor {
  FormItemList *formItem;

  short y = 0;
  short rows = 0;
};

Form *newFormList(const char *action, const char *method,
                      const char *charset, const char *enctype,
                      const char *target, const char *name, Form *_next);

Str *Str_form_quote(Str *x);
