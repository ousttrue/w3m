#pragma once
#include "Str.h"
#include "url.h"
#include <libwc/ces.h>
#include <stdbool.h>

struct HtmlBuilder {
    struct Url* cur_baseURL;
    wc_ces cur_document_charset;
    Str cur_title;
    Str pre_title;

    // <select>
    Str cur_select;
    Str select_str;
    int select_is_multiple;
    int n_selectitem;
    Str cur_option;
    Str cur_option_value;
    Str cur_option_label;
    int cur_option_selected;
    int cur_status;
};

struct html_feed_environ;
struct parsed_tag;
struct readbuffer;
struct _textlinelist;
struct Buffer;

int HTMLtagproc1(struct HtmlBuilder* hb,
    struct parsed_tag* tag, struct html_feed_environ* h_env);
void HTMLlineproc2(struct HtmlBuilder *hb, struct Buffer* buf, struct _textlinelist* tl);
void HTMLlineproc0(struct HtmlBuilder* hb,
    const char* istr, struct html_feed_environ* h_env, bool internal);
void completeHTMLstream(struct HtmlBuilder* hb,
    struct html_feed_environ*, struct readbuffer*);

Str process_title(struct HtmlBuilder* hb, struct parsed_tag* tag);
Str process_n_title(struct HtmlBuilder* hb, struct parsed_tag* tag);
void feed_title(struct HtmlBuilder* hb, const char* str);
