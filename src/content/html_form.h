#pragma once
#include <Str.h>
#include <ces.h>

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
    struct FormSelectOptionItem* select_option;
    Str label, init_label;
    int selected, init_selected;
    struct Form* parent;
    struct FormItem* next;
};

enum FormMethodType {
    FORM_METHOD_GET = 0,
    FORM_METHOD_POST = 1,
    FORM_METHOD_INTERNAL = 2,
    FORM_METHOD_HEAD = 3,
};
enum FormEncodeType {
    FORM_ENCTYPE_URLENCODED = 0,
    FORM_ENCTYPE_MULTIPART = 1,
};
struct Form {
    struct FormItem* item;
    struct FormItem* lastitem;
    enum FormMethodType method;
    Str action;
    const char* target;
    const char* name;
    wc_ces charset;
    enum FormEncodeType enctype;
    struct Form* next;
    int nitems;
    const char* body;
    const char* boundary;
    unsigned long length;
};
