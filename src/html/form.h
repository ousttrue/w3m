#pragma once
#include "current.h"
#include "text/Str.h"
#include "input/url.h"

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

enum FormMethod {
  FORM_METHOD_GET = 0,
  FORM_METHOD_POST = 1,
  FORM_METHOD_INTERNAL = 2,
  FORM_METHOD_HEAD = 3,
};

enum FormEncode {
  FORM_ENCTYPE_URLENCODED = 0,
  FORM_ENCTYPE_MULTIPART = 1,
};

struct FormItemList {
  enum FormItemType type;
  Str name;
  Str value;
  Str init_value;
  int checked;
  int init_checked;
  int accept;
  int size;
  int rows;
  int maxlength;
  int readonly;
  struct FormList *parent;
  struct FormItemList *next;
};

struct FormList {
  struct FormItemList *item;
  struct FormItemList *lastitem;
  enum FormMethod method;
  Str action;
  const char *target;
  const char *name;
  enum FormEncode enctype;
  struct FormList *next;
  int nitems;
  const char *body;
  const char *boundary;
  unsigned long length;
};

struct HtmlTag;
struct Anchor;
struct AnchorList;
struct Document;
struct FormList *newFormList(const char *action, const char *method,
                             const char *charset, const char *enctype,
                             const char *target, const char *name,
                             struct FormList *_next);
struct FormItemList *formList_addInput(struct FormList *fl,
                                       struct HtmlTag *tag);
char *form2str(struct FormItemList *fi);
int formtype(char *typestr);
void formRecheckRadio(struct Document *doc, struct Anchor *a,
                      struct FormItemList *form);
void formResetBuffer(struct Document *doc, struct AnchorList *formitem);
void formUpdateBuffer(struct Document *doc, struct Anchor *a,
                      struct FormItemList *form);
void preFormUpdateBuffer(struct Url url, struct Document *doc);
Str textfieldrep(Str s, int width);
void input_textarea(struct FormItemList *fi);
void do_internal(const char *action, const char *data, struct Current current);
void form_write_data(FILE *f, const char *boundary, const char *name,
                     const char *value);
void form_write_from_file(FILE *f, const char *boundary, const char *name,
                          const char *filename, const char *file);
void loadPreForm(void);
Str Str_form_quote(Str x);
void query_from_followform(Str *query, struct FormItemList *fi, int multipart);
