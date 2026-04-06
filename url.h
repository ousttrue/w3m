#pragma once
#include "url_scheme.h"
#include "Str.h"
#include <libwc/ces.h>

extern Str header_string;
extern int ai_family_order_table[7][3]; /* XXX */

typedef struct _Buffer Buffer;

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

struct Url* baseURL(Buffer* buf);
int openSocket(const char* hostname, const char* remoteport_name, unsigned short remoteport_num);
void parseURL(const char* url, struct Url* p_url, struct Url* current);
void copyParsedURL(struct Url* p, const struct Url* q);
void parseURL2(const char* url, struct Url* pu, struct Url* current);
Str parsedURL2Str(struct Url* pu);
Str parsedURL2RefererStr(struct Url* pu);
struct URLFile;
typedef union input_stream* InputStream;
void init_stream(struct URLFile* uf, int scheme, InputStream stream);

struct form_list;
struct _textlist;
struct HttpRequest;

#define NO_REFERER ((char*)-1)

enum UrlOptionFlags {
    RG_NOCACHE = 1,
    RG_FRAME = 2,
    RG_FRAME_SRC = 4,
};
struct URLOption {
    const char* referer;
    enum UrlOptionFlags flag;
};
struct URLFile openURL(const char* url, struct Url* pu, struct Url* current,
    struct URLOption* option, struct form_list* request,
    struct _textlist* extra_header, struct URLFile* ouf,
    struct HttpRequest* hr, unsigned char* status);

const char* filename_extension(const char* patch, int is_url);
struct Url* schemeToProxy(int scheme);
wc_ces url_to_charset(const char* url, const struct Url* base, wc_ces doc_charset);
char* url_encode(const char* url, const struct Url* base, wc_ces doc_charset);
char* url_decode2(const char* url, const Buffer* buf);
Str _parsedURL2Str(struct Url* pu, bool pass, bool user, bool label);
const char* guessContentType(const char* filename);
