/*
 * HTML forms
 */
#pragma once
#include "Str.h"
#include "config.h"
#include "libwc/ces.h"

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
#ifdef USE_MENU
#define MAX_SELECT 10 /* max number of <select>..</select> \
                       * within one document */
#endif /* USE_MENU */

typedef struct form_list {
    struct form_item_list* item;
    struct form_item_list* lastitem;
    int method;
    pStr action;
    const char* target;
    const char* name;
    wc_ces charset;
    int enctype;
    struct form_list* next;
    int nitems;
    const char* body;
    const char* boundary;
    unsigned long length;
} FormList;

#ifdef USE_MENU
typedef struct form_select_option_item {
    pStr value;
    pStr label;
    int checked;
    struct form_select_option_item* next;
} FormSelectOptionItem;

typedef struct form_select_option {
    FormSelectOptionItem* first;
    FormSelectOptionItem* last;
} FormSelectOption;

void addSelectOption(FormSelectOption* fso, pStr value, pStr label, int chk);
void chooseSelectOption(struct form_item_list* fi, FormSelectOptionItem* item);
void updateSelectOption(struct form_item_list* fi, FormSelectOptionItem* item);
int formChooseOptionByMenu(struct form_item_list* fi, int x, int y);
#endif /* USE_MENU */

typedef struct form_item_list {
    int type;
    pStr name;
    pStr value, init_value;
    int checked, init_checked;
    int accept;
    int size;
    int rows;
    int maxlength;
    int readonly;
#ifdef USE_MENU
    FormSelectOptionItem* select_option;
    pStr label, init_label;
    int selected, init_selected;
#endif /* USE_MENU */
    struct form_list* parent;
    struct form_item_list* next;
} FormItemList;

struct form_list* newFormList(const char* action, const char* method, const char* charset,
    const char* enctype, const char* target, const char* name,
    struct form_list* _next);
struct parsed_tag;
struct form_item_list* formList_addInput(struct form_list* fl,
    struct parsed_tag* tag);
char* form2str(FormItemList* fi);
int formtype(char* typestr);
struct Anchor;
struct _Buffer;
void formRecheckRadio(struct Anchor* a, struct _Buffer* buf, FormItemList* form);
struct AnchorList;
void formResetBuffer(struct _Buffer* buf, struct AnchorList* formitem);
void formUpdateBuffer(struct Anchor* a, struct _Buffer* buf, struct form_item_list* form);
void preFormUpdateBuffer(struct _Buffer* buf);
pStr textfieldrep(pStr s, int width);
void input_textarea(FormItemList* fi);
void do_internal(char* action, char* data);
void form_write_data(FILE* f, const char* boundary, const char* name, const char* value);
void form_write_from_file(FILE* f, const char* boundary, const char* name,
    const char* filename, const char* file);
