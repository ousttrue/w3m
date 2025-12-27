#pragma once
#include "Str.h"
#include <stdbool.h>

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
void feed_select(const char* str);
struct HtmlBuilder;
struct parsed_tag;
Str process_img(struct HtmlBuilder *hb, struct parsed_tag* tag, int width);
Str process_form(struct HtmlBuilder *hb, struct parsed_tag* tag);
Str process_input(struct HtmlBuilder *hb, struct parsed_tag* tag);
Str process_button(struct HtmlBuilder *hb, struct parsed_tag* tag);
Str process_select(struct HtmlBuilder *hb, struct parsed_tag* tag);
Str process_textarea(struct HtmlBuilder *hb, struct parsed_tag* tag, int width);
struct _textlinelist;
void HTMLlineproc2(struct HtmlBuilder *hb, struct Buffer* buf, struct _textlinelist* tl);
