#pragma once
#include "URLFile.h"
#include "url.h"
#include "UserInteraction.h"
#include <wc.h>

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

    struct Url puv[FollowRedirection];
    int nredir;

    // const char* url;

    Str uname;
    Str pwd;
    Str realm;
    bool add_auth_cookie_flag;

    Str page;
    const char* content_type;
    wc_ces charset;

    long long current_content_length;
};

void initHttpClient(struct HttpClient* c);
bool checkRedirection(struct HttpClient* c, struct Url* pu);

struct Form;
struct Content openHttp(struct HttpClient* c, const char* path, struct Url* current, struct Form* post, const char* referer, struct UserInteraction ui);
