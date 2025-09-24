#pragma once
#include "geometry.h"
#include "html_form.h"

extern int FoldTextarea;
extern char* Editor;
extern char* pre_form_file;

int formChooseOptionByMenu(struct UI* ui, struct FormItem* fi, int x, int y);

struct Anchor;
struct HtmlTagParsed;
struct AnchorList;
struct Document;

void formRecheckRadio(struct UI* ui, struct Document* doc, struct Anchor* a, struct FormItem* form);
void formResetDocument(struct Document* doc, struct AnchorList* formitem);
void formUpdateDocument(struct Document* doc, struct Anchor* a, struct FormItem* form);
void preFormUpdateDocument(struct UI* ui, struct Document* doc);
void input_textarea(struct UI* ui, struct FormItem* fi);
void form_write_data(FILE* f, const char* boundary, const char* name, const char* value);
void form_write_from_file(FILE* f, const char* boundary, const char* name, const char* filename, const char* file);
void loadPreForm(struct UI* ui);
