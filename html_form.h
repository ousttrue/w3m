/*
 * HTML forms
 */
#pragma once
#include "Str.h"
#include "libwc/wc_types.h"

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

#define MAX_TEXTAREA 10 /* max number of <textarea>..</textarea> \
                         * within one document */
#define MAX_SELECT 10 /* max number of <select>..</select> \
                       * within one document */

struct FormSelectOptionItem {
    Str value;
    Str label;
    int checked;
    struct FormSelectOptionItem* next;
};

struct FormSelectOption {
    struct FormSelectOptionItem* first;
    struct FormSelectOptionItem* last;
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
    struct FormSelectOptionItem* select_option;
    Str label, init_label;
    int selected, init_selected;
    struct FormList* parent;
    struct FormItemList* next;
};

struct FormList {
    struct FormItemList* item;
    struct FormItemList* lastitem;
    int method;
    Str action;
    char* target;
    char* name;
    wc_ces charset;
    int enctype;
    struct FormList* next;
    int nitems;
    char* body;
    char* boundary;
    unsigned long length;
};

void addSelectOption(struct FormSelectOption* fso, Str value, Str label, int chk);
void chooseSelectOption(struct FormItemList* fi, struct FormSelectOptionItem* item);
void updateSelectOption(struct FormItemList* fi, struct FormSelectOptionItem* item);
int formChooseOptionByMenu(struct FormItemList* fi, int x, int y);
void input_textarea(struct FormItemList* fi);
int formtype(const char* typestr);
struct HtmlBuilder;
struct HtmlTag;
struct FormItemList* formList_addInput(struct HtmlBuilder* hb, struct FormList* fl,
    struct HtmlTag* tag);
