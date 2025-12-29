#pragma once
#include "Str.h"
#include "url.h"
#include "html_table.h"
#include <libwc/ces.h>
#include <stdbool.h>

struct FormSelectOption;
struct HtmlBuilder {
    struct Url* cur_baseURL;

    wc_ces cur_document_charset;
    wc_ces meta_charset;

    Str cur_title;
    Str pre_title;

    int cur_hseq;
    int cur_iseq;

    // <table>
    struct table* tables[MAX_TABLE];
    struct table_mode table_mode[MAX_TABLE];

    // <form>
    struct FormList** forms;
    int* form_stack;
    int form_max; // = -1;
    int forms_size; // = 0;
    int form_sp; // = 0;

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
    /* menu based <select>  */
    struct FormSelectOption* select_option;
    int max_select; //= MAX_SELECT;
    int n_select;
    int cur_option_maxwidth;

    // <textarea>
    Str cur_textarea;
    Str* textarea_str;
    int cur_textarea_size;
    int cur_textarea_rows;
    int cur_textarea_readonly;
    int n_textarea;
    int ignore_nl_textarea;
    int max_textarea; // = MAX_TEXTAREA;
};

struct html_feed_environ;
struct HtmlTag;
struct readbuffer;
struct _textlinelist;
struct Buffer;

int HTMLtagproc1(struct HtmlBuilder* hb,
    struct HtmlTag* tag, struct html_feed_environ* h_env);
void HTMLlineproc2(struct HtmlBuilder* hb, struct Buffer* buf, struct _textlinelist* tl);
void HTMLlineproc0(struct HtmlBuilder* hb,
    const char* istr, struct html_feed_environ* h_env, bool internal);
void completeHTMLstream(struct HtmlBuilder* hb,
    struct html_feed_environ*, struct readbuffer*);

Str process_title(struct HtmlBuilder* hb, struct HtmlTag* tag);
Str process_n_title(struct HtmlBuilder* hb, struct HtmlTag* tag);
void feed_title(struct HtmlBuilder* hb, const char* str);
