#pragma once
#include "Str.h"

#define FORM_UNKNOWN -1
#define FORM_INPUT_TEXT 0
#define FORM_INPUT_PASSWORD 1
#define FORM_INPUT_CHECKBOX 2
#define FORM_INPUT_RADIO 3
#define FORM_INPUT_SUBMIT 4
#define FORM_INPUT_RESET 5
#define FORM_INPUT_HIDDEN 6
#define FORM_INPUT_IMAGE 7
#define FORM_SELECT 8
#define FORM_TEXTAREA 9
#define FORM_INPUT_BUTTON 10
#define FORM_INPUT_FILE 11

#define FORM_I_TEXT_DEFAULT_SIZE 40
#define FORM_I_SELECT_DEFAULT_SIZE 40
#define FORM_I_TEXTAREA_DEFAULT_WIDTH 40

#define FORM_METHOD_GET 0
#define FORM_METHOD_POST 1
#define FORM_METHOD_INTERNAL 2
#define FORM_METHOD_HEAD 3

#define FORM_ENCTYPE_URLENCODED 0
#define FORM_ENCTYPE_MULTIPART 1

struct FormItemList;
struct FormList {
  struct FormItemList *item;
  struct FormItemList *lastitem;
  int method;
  Str action;
  const char *target;
  const char *name;
  int enctype;
  struct FormList *next;
  int nitems;
  const char *body;
  const char *boundary;
  unsigned long length;
};

struct FormItemList {
  int type;
  Str name;
  Str value, init_value;
  int checked, init_checked;
  int accept;
  int size;
  int rows;
  int maxlength;
  int readonly;
  struct FormList *parent;
  struct FormItemList *next;
};

extern struct FormList *newFormList(const char *action, const char *method,
                                    const char *charset, const char *enctype,
                                    const char *target, const char *name,
                                    struct FormList *_next);
struct parsed_tag;
extern struct FormItemList *formList_addInput(struct FormList *fl,
                                              struct parsed_tag *tag);
extern char *form2str(struct FormItemList *fi);
extern int formtype(char *typestr);
struct Anchor;
struct Buffer;
extern void formRecheckRadio(struct Anchor *a, struct Buffer *buf,
                             struct FormItemList *form);
struct AnchorList;
extern void formResetBuffer(struct Buffer *buf, struct AnchorList *formitem);
extern void formUpdateBuffer(struct Anchor *a, struct Buffer *buf,
                             struct FormItemList *form);
extern void preFormUpdateBuffer(struct Buffer *buf);
extern Str textfieldrep(Str s, int width);
extern void input_textarea(struct FormItemList *fi);
extern void do_internal(char *action, char *data);
extern void form_write_data(FILE *f, const char *boundary, const char *name,
                            const char *value);
extern void form_write_from_file(FILE *f, const char *boundary,
                                 const char *name, const char *filename,
                                 const char *file);
extern void loadPreForm(void);
extern Str Str_form_quote(Str x);
