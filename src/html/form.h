/* $Id: form.h,v 1.6 2003/09/22 21:02:18 ukai Exp $ */
/*
 * HTML forms
 */
#pragma once
#include "Str.h"
#include <memory>

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
struct HtmlTag;
struct FormList {
  FormItemList *item;
  FormItemList *lastitem;
  int method;
  Str *action;
  const char *target;
  const char *name;
  int enctype;
  FormList *next;
  int nitems;
  char *body;
  char *boundary;
  unsigned long length;

  FormItemList *formList_addInput(struct HtmlParser *parser, HtmlTag *tag);
};

FormList *newFormList(const char *action, const char *method,
                      const char *charset, const char *enctype,
                      const char *target, const char *name, FormList *_next);

struct Buffer;
struct Anchor;
struct AnchorList;
struct LineLayout;
char *form2str(FormItemList *fi);
int formtype(const char *typestr);
void formRecheckRadio(Anchor *a, Buffer *buf, FormItemList *form);
void formResetBuffer(LineLayout *layout, AnchorList *formitem);
void formUpdateBuffer(Anchor *a, LineLayout *layout, FormItemList *form);
void preFormUpdateBuffer(const std::shared_ptr<Buffer> &buf);
Str *textfieldrep(Str *s, int width);
void input_textarea(FormItemList *fi);
void do_internal(char *action, char *data);
void form_write_data(FILE *f, char *boundary, char *name, char *value);
void form_write_from_file(FILE *f, char *boundary, char *name, char *filename,
                          char *file);
void loadPreForm(void);
