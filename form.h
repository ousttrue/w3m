#pragma once
#include <w3m.h>
#include "Str.h"
#include <libwc/wc_types.h>

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

struct FormItem {
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
    struct Form* parent;
    struct FormItem* next;
};

struct Form {
    struct FormItem* item;
    struct FormItem* lastitem;
    int method;
    Str action;
    const char* target;
    const char* name;
    wc_ces charset;
    int enctype;
    struct Form* next;
    int nitems;
    const char* body;
    const char* boundary;
    unsigned long length;
};

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

void addSelectOption(struct FormSelectOption* fso, Str value, Str label, int chk);
void chooseSelectOption(struct FormItem* fi, struct FormSelectOptionItem* item);
void updateSelectOption(struct FormItem* fi, struct FormSelectOptionItem* item);
int formChooseOptionByMenu(struct CmdArgs *args, struct FormItem* fi, int x, int y);
void form_write_data(FILE* f, const char* boundary, const char* name, const char* value);
void form_write_from_file(FILE* f, const char* boundary, const char* name, const char* filename, char* file);
struct Form* newFormList(char* action, char* method, char* charset,
    char* enctype, char* target, char* name,
    struct Form* _next);
struct HtmlTag;
struct FormItem* formList_addInput(struct Form* fl,
    struct HtmlTag* tag);
char* form2str(struct FormItem* fi);
int formtype(char* typestr);
struct Buffer;
void preFormUpdateBuffer(struct Buffer* buf);
Str textfieldrep(Str s, int width);
void input_textarea(struct CmdArgs *args, struct FormItem* fi);
void do_internal(struct CmdArgs *args, char* action, char* data);
struct Buffer* page_info_panel(struct Buffer* buf);
void loadPreForm(void);
struct Url;
char* last_modified(struct Buffer* buf);
Str romanNumeral(int n);
Str romanAlphabet(int n);
void mySystem(char* command, int background);
char* url_unquote_conv(const char* url, wc_ces charset);
char* expandName(char* name);
struct parsed_tagarg;
void change_charset(struct CmdArgs *args, struct parsed_tagarg* arg);
const char* guess_save_name(struct Buffer* buf, const char* file);
Str getLinkNumberStr(int correction);
