#pragma once
#include "url_scheme.h"
#include "Str.h"
#include <libwc/ces.h>

extern Str header_string;
extern int ai_family_order_table[7][3]; /* XXX */

struct Url {
    enum UrlScheme scheme;
    const char* user;
    const char* pass;
    const char* host;
    int port;
    const char* file;
    const char* real_file;
    const char* query;
    const char* label;
    bool is_nocache;
};
#define IS_EMPTY_PARSED_URL(pu) ((pu)->scheme == SCM_UNKNOWN && !(pu)->file)

struct Buffer;
struct Url* baseURL(struct Buffer* buf);
int openSocket(const char* hostname, const char* remoteport_name, unsigned short remoteport_num);
struct Url parseURL(const char* url, const struct Url* current);
void copyParsedURL(struct Url* p, const struct Url* q);
struct Url parseURL2(const char* url, const struct Url* current);
Str parsedURL2Str(struct Url* pu);
Str parsedURL2RefererStr(struct Url* pu);

struct Form;
struct _textlist;
struct HttpRequest;

const char* filename_extension(const char* patch, int is_url);
struct Url* schemeToProxy(int scheme);
wc_ces url_to_charset(const char* url, const struct Url* base, wc_ces doc_charset);
char* url_encode(const char* url, const struct Url* base, wc_ces doc_charset);
char* url_decode2(const char* url, const struct Buffer* buf);
Str _parsedURL2Str(struct Url* pu, bool pass, bool user, bool label);
char* url_quote(const char* str);
Str Str_url_unquote(Str x, int is_form, int safe);
Str Str_form_quote(Str x);
#define Str_form_unquote(x) Str_url_unquote((x), true, false)
Str qstr_unquote(Str s);
