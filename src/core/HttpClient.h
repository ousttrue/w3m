#pragma once
#include "istream.h"

// flags for loadGeneralFile
#define RG_NOCACHE 1

struct URLOption {
    const char* referer;
    int flag;
};

enum HttpConnectionStatus {
    HTST_UNKNOWN = 255,
    HTST_MISSING = 254,
    HTST_NORMAL = 0,
    HTST_CONNECT = 1,
};

#define FollowRedirection 10
struct HttpClient {
    struct URLFile f;
    enum HttpConnectionStatus status;

    struct _ParsedURL puv[FollowRedirection];
    int nredir;

    const char* url;

    Str page;
    const char* content_type;
    wc_ces charset;
};

void initHttpClient(struct HttpClient* c, const char* path);
bool checkRedirection(struct HttpClient* c, struct _ParsedURL* pu);

struct HttpRequest;
struct form_list;
void openURL(struct HttpClient* c, ParsedURL* pu, ParsedURL* current,
    struct URLOption* option, struct form_list* request,
    TextList* extra_header,
    struct HttpRequest* hr);
