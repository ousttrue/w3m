#pragma once
#include "geometry.h"
#include "html_form.h"

extern int FoldTextarea;
extern char* Editor;
extern char* pre_form_file;

#define FORM_I_TEXT_DEFAULT_SIZE 40
#define FORM_I_SELECT_DEFAULT_SIZE 40
#define FORM_I_TEXTAREA_DEFAULT_WIDTH 40

int formChooseOptionByMenu(struct UI ui, struct FormItem* fi, int x, int y);

struct Anchor;
struct Buffer;
struct HtmlTagParsed;
struct AnchorList;

struct FormItem* formList_addInput(struct Form* fl,
    struct HtmlTagParsed* tag);
void formRecheckRadio(struct UI ui, struct Anchor* a, struct Buffer* buf, struct FormItem* form);
void formResetBuffer(struct Buffer* buf, struct AnchorList* formitem);
void formUpdateBuffer(struct Anchor* a, struct Buffer* buf, struct FormItem* form);
void preFormUpdateBuffer(struct UI ui, struct Buffer* buf);
void input_textarea(struct FormItem* fi);
void form_write_data(FILE* f, const char* boundary, const char* name, const char* value);
void form_write_from_file(FILE* f, const char* boundary, const char* name, const char* filename, const char* file);
void loadPreForm(void);
