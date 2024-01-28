/* $Id: form.h,v 1.6 2003/09/22 21:02:18 ukai Exp $ */
/*
 * HTML forms
 */
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

#define MAX_TEXTAREA                                                           \
  10 /* max number of <textarea>..</textarea>                                  \
      * within one document */

struct FormItemList;
struct FormList {
  FormItemList *item;
  FormItemList *lastitem;
  int method;
  Str action;
  char *target;
  char *name;
  int enctype;
  FormList *next;
  int nitems;
  char *body;
  char *boundary;
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
  FormList *parent;
  FormItemList *next;
};

FormList *newFormList(char *action, char *method, char *charset, char *enctype,
                      char *target, char *name, FormList *_next);
struct FormItemList *formList_addInput(FormList *fl, struct parsed_tag *tag);
