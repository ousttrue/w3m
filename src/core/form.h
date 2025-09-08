#pragma once
#include "html_form.h"

extern int FoldTextarea;
extern char* Editor;
extern char* pre_form_file;

#define FORM_I_TEXT_DEFAULT_SIZE 40
#define FORM_I_SELECT_DEFAULT_SIZE 40
#define FORM_I_TEXTAREA_DEFAULT_WIDTH 40

#define MAX_TEXTAREA 10 /* max number of <textarea>..</textarea> \
                         * within one document */
#define MAX_SELECT 10 /* max number of <select>..</select> \
                       * within one document */

void addSelectOption(struct FormSelectOption* fso, Str value, Str label, int chk);
void chooseSelectOption(struct FormItem* fi, struct FormSelectOptionItem* item);
void updateSelectOption(struct FormItem* fi, struct FormSelectOptionItem* item);
int formChooseOptionByMenu(struct FormItem* fi, int x, int y);

struct _anchor;
struct _Buffer;
struct HtmlTagParsed;
struct _anchorList;

struct Form* newFormList(char* action, char* method, char* charset,
    char* enctype, char* target, char* name,
    struct Form* _next);
struct FormItem* formList_addInput(struct Form* fl,
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
