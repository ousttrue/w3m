#pragma once
#include "Str.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

struct Buffer* loadHTMLString(Str page);
int is_html_type(const char* type);

struct Url;
struct FormList;
struct Buffer* loadGeneralFile(const char* path, struct Url* current, const char* referer, int flag, struct FormList* request, bool do_download);

int _doFileCopy(const char* tmpf, const char* defstr, bool download);
inline static int doFileCopy(const char* tmpf, const char* defstr)
{
    return _doFileCopy(tmpf, defstr, false);
}
int doFileMove(const char* tmpf, const char* defstr);
int dir_exist(const char* path);
int checkCopyFile(const char* path1, const char* path2);
struct HtmlBuilder;
struct parsed_tag;
Str process_img(struct HtmlBuilder* hb, struct parsed_tag* tag, int width);
Str process_form(struct HtmlBuilder* hb, struct parsed_tag* tag);
Str process_input(struct HtmlBuilder* hb, struct parsed_tag* tag);
Str process_button(struct HtmlBuilder* hb, struct parsed_tag* tag);
Str process_select(struct HtmlBuilder* hb, struct parsed_tag* tag);
Str process_textarea(struct HtmlBuilder* hb, struct parsed_tag* tag, int width);
Str process_n_select(struct HtmlBuilder* hb);
void feed_select(struct HtmlBuilder* hb, const char* str);
void process_option(struct HtmlBuilder* hb);
Str process_n_textarea(struct HtmlBuilder* hb);
void feed_textarea(struct HtmlBuilder* hb, const char* str);
Str process_anchor(struct HtmlBuilder* hb, struct parsed_tag* tag, const char* tagbuf);
void showProgress(int64_t* linelen, int64_t* trbyte, size_t current_content_length);
Str process_n_form(struct HtmlBuilder* hb);
Str getLinkNumberStr(struct HtmlBuilder* hb, int correction);
int checkOverWrite(const char* path);
struct input_stream;
int checkSaveFile(struct input_stream* stream, const char* path);

