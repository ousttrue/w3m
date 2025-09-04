#pragma once
#include "istream.h"

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

    long long current_content_length;
};

void initHttpClient(struct HttpClient* c, const char* path);
bool checkRedirection(struct HttpClient* c, struct _ParsedURL* pu);

struct HttpRequest;
struct form_list;
void openURL(struct HttpClient* c, ParsedURL* pu, ParsedURL* current,
    struct form_list* post,
    const char* referer,
    bool no_cache,
    TextList* extra_header,
    struct HttpRequest* hr);
