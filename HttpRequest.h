#pragma once
#include "Url.h"
#include "textlist.h"
#include <gcstr.h>
#include <stdbool.h>

typedef int _bool;
extern _bool override_user_agent;
extern const char* UserAgent;
extern const char* AcceptLang;
extern const char* AcceptEncoding;
extern const char* AcceptMedia;
extern _bool NoCache;
extern _bool NoSendReferer;
extern _bool CrossOriginReferer;
extern _bool override_content_type;
extern Str header_string;

#define NO_REFERER ((char*)-1)

struct form_list;
struct HttpRequest {
    char command;
    char flag;
    char* referer;
    struct form_list* request;
};

#define HR_COMMAND_GET 0
#define HR_COMMAND_POST 1
#define HR_COMMAND_CONNECT 2
#define HR_COMMAND_HEAD 3

#define HR_FLAG_LOCAL 1
#define HR_FLAG_PROXY 2

#define HTST_UNKNOWN 255
#define HTST_MISSING 254
#define HTST_NORMAL 0
#define HTST_CONNECT 1

Str HTTPrequestMethod(struct HttpRequest* hr);
Str HTTPrequestURI(struct Url* pu, struct HttpRequest* hr);
Str HTTPrequest(struct Url* pu, struct Url* current, struct HttpRequest* hr, TextList* extra);
