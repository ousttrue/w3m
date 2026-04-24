#pragma once
#include "Str.h"
#include <stdio.h>
#include <stdint.h>
#include <libwc/wc_types.h>

extern wc_ces content_charset;
extern int64_t current_content_length;
extern int frame_source;
extern int n_textarea;

#define set_space_to_prevchar(x) Strcopy_charp_n((x), " ", 1)

struct URLFile;
struct Buffer;
void loadHTMLstream(struct URLFile* f, struct Buffer* newBuf, FILE* src, int internal);

void HTMLlineproc2body(struct Buffer* buf, Str (*feed)(), int llimit);

int getMetaRefreshParam(char* q, Str* refresh_uri);

char* convert_size(int64_t size, int usefloat);
char* convert_size2(int64_t size1, int64_t size2, int usefloat);

void showProgress(int64_t* linelen, int64_t* trbyte);

char* checkHeader(struct Buffer* buf, char* field);

Str process_n_form(void);

char* checkContentType(struct Buffer* buf);

struct HtmlTag;
extern Str process_img(struct HtmlTag* tag, int width);
extern Str process_anchor(struct HtmlTag* tag, char* tagbuf);
extern Str process_input(struct HtmlTag* tag);
extern Str process_button(struct HtmlTag* tag);
extern Str process_n_button(void);
extern Str process_select(struct HtmlTag* tag);
extern Str process_n_select(void);
extern void feed_select(const char* str);
extern void process_option(void);
extern Str process_textarea(struct HtmlTag* tag, int width);
extern Str process_n_textarea(void);
extern void feed_textarea(char* str);

struct readbuffer;
void set_breakpoint(struct readbuffer* obuf, int tag_length);
struct html_feed_environ;
void close_anchor(struct html_feed_environ* h_env, struct readbuffer* obuf);
Str process_form(struct HtmlTag* tag);

void completeHTMLstream(struct html_feed_environ*, struct readbuffer*);
