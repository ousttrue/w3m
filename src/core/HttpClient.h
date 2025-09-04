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

struct HttpClient {
    struct URLFile f;
    enum HttpConnectionStatus status;
};

struct HttpRequest;
struct form_list;
void openURL(struct HttpClient* c, const char* url, ParsedURL* pu, ParsedURL* current,
    struct URLOption* option, struct form_list* request,
    TextList* extra_header,
    struct HttpRequest* hr);
