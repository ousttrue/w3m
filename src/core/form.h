#pragma once
#include "Str.h"
#include <stdio.h>
#include <wc.h>

extern int FoldTextarea;
extern char* Editor;
extern char* pre_form_file;

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

typedef struct form_list {
    struct FormItem* item;
    struct FormItem* lastitem;
    int method;
    Str action;
    const char* target;
    char* name;
    wc_ces charset;
    int enctype;
    struct form_list* next;
    int nitems;
    char* body;
    char* boundary;
    unsigned long length;
} FormList;

typedef struct form_select_option_item {
    Str value;
    Str label;
    int checked;
    struct form_select_option_item* next;
} FormSelectOptionItem;

typedef struct form_select_option {
    FormSelectOptionItem* first;
    FormSelectOptionItem* last;
} FormSelectOption;

void addSelectOption(FormSelectOption* fso, Str value, Str label, int chk);
void chooseSelectOption(struct FormItem* fi, FormSelectOptionItem* item);
void updateSelectOption(struct FormItem* fi, FormSelectOptionItem* item);
int formChooseOptionByMenu(struct FormItem* fi, int x, int y);

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
struct FormItem {
    enum FormItemType type;
    Str name;
    Str value, init_value;
    int checked, init_checked;
    int accept;
    int size;
    int rows;
    int maxlength;
    int readonly;
    FormSelectOptionItem* select_option;
    Str label, init_label;
    int selected, init_selected;
    struct form_list* parent;
    struct FormItem* next;
};

struct _anchor;
struct _Buffer;
struct HtmlTagParsed;
struct _anchorList;

struct form_list* newFormList(char* action, char* method, char* charset,
    char* enctype, char* target, char* name,
    struct form_list* _next);
struct FormItem* formList_addInput(struct form_list* fl,
    struct HtmlTagParsed* tag);
char* form2str(struct FormItem* fi);
int formtype(char* typestr);
void formRecheckRadio(struct _anchor* a, struct _Buffer* buf, struct FormItem* form);
void formResetBuffer(struct _Buffer* buf, struct _anchorList* formitem);
void formUpdateBuffer(struct _anchor* a, struct _Buffer* buf, struct FormItem* form);
void preFormUpdateBuffer(struct _Buffer* buf);
Str textfieldrep(Str s, int width);
void input_textarea(struct FormItem* fi);
void do_internal(char* action, char* data);
void form_write_data(FILE* f, char* boundary, char* name, char* value);
void form_write_from_file(FILE* f, char* boundary, char* name, char* filename, char* file);
void loadPreForm(void);
