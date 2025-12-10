#pragma once
#include "Url.h"
#include "input_stream.h"
#include <wc/wc.h>
#include <gcstr.h>

#define PIPEBUFFERNAME "*stream*"

struct Buffer;
struct form_list;
struct Buffer* loadGeneralFile(const char* path, struct Url* current, const char* referer, enum LoadGeneralFlags flag, struct form_list* request);
wc_ces url_to_charset(const char* url, const struct Url* base,
    wc_ces doc_charset);
char* url_encode(const char* url, const struct Url* base,
    wc_ces doc_charset);
struct URLFile;

#define RAW_MODE 0
#define PAGER_MODE 1
#define HTML_MODE 2
#define HEADER_MODE 3
void cleanup_line(Str s, int mode);
Str convertLine(struct URLFile* uf, Str line, int mode, wc_ces* charset, wc_ces doc_charset);

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
extern void feed_textarea(const char* str);
extern Str process_form(struct HtmlTag* tag);
extern Str process_n_form(void);
extern int getMetaRefreshParam(const char* q, Str* refresh_uri);
extern Str loadGopherDir(struct URLFile* uf, struct Url* pu, wc_ces* charset);
extern Str loadGopherSearch(struct URLFile* uf, struct Url* pu, wc_ces* charset);
extern Str getLinkNumberStr(int correction);
extern void examineFile(const char* path, struct URLFile* uf);
extern int is_boundary(unsigned char*, unsigned char*);
extern void pushEvent(int cmd, void* data);

extern int save2tmp(struct URLFile* uf, char* tmpf);
char* acceptableEncoding(void);

void uncompress_stream(struct URLFile* uf, const char** src);
char* guess_save_name(struct Buffer* buf, const char* file);
