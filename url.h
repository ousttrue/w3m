#ifndef W3M_URL_H__
#define W3M_URL_H__

#include "Str.h"
#include "form.h"

extern TextList *NO_proxy_domains;

typedef struct http_request {
    char command;
    char flag;
    char *referer;
    FormList *request;
} HRequest;

#define HR_COMMAND_GET		0
#define HR_COMMAND_POST		1
#define HR_COMMAND_CONNECT	2
#define HR_COMMAND_HEAD		3

#define HR_FLAG_LOCAL		1
#define HR_FLAG_PROXY		2

#define HTST_UNKNOWN		255
#define HTST_MISSING		254
#define HTST_NORMAL		0
#define HTST_CONNECT		1

Str HTTPrequestMethod(HRequest *hr);
Str HTTPrequestURI(ParsedURL *pu, HRequest *hr);
URLFile openURL(char *url, ParsedURL *pu, ParsedURL *current,
		URLOption *option, FormList *request,
		TextList *extra_header, URLFile *ouf,
		HRequest *hr, unsigned char *status);

#endif
