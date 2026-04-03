#pragma once
#include "Str.h"
#include <wc.h>

typedef struct _Buffer Buffer;

typedef struct _ParsedURL ParsedURL;
ParsedURL* baseURL(Buffer* buf);
int openSocket(char* hostname, char* remoteport_name, unsigned short remoteport_num);
void parseURL(const char* url, ParsedURL* p_url, ParsedURL* current);
void copyParsedURL(ParsedURL* p, const ParsedURL* q);
void parseURL2(const char* url, ParsedURL* pu, ParsedURL* current);
Str parsedURL2Str(ParsedURL* pu);
Str parsedURL2RefererStr(ParsedURL* pu);
int getURLScheme(char** url);
struct URLFile;
typedef union input_stream* InputStream;
void init_stream(struct URLFile* uf, int scheme, InputStream stream);

typedef struct http_request HRequest;
Str HTTPrequestMethod(HRequest* hr);
Str HTTPrequestURI(ParsedURL* pu, HRequest* hr);

typedef struct {
    char* referer;
    int flag;
} URLOption;
struct form_list;
struct _textlist;
struct URLFile openURL(const char* url, ParsedURL* pu, ParsedURL* current,
    URLOption* option, struct form_list* request,
    struct _textlist* extra_header, struct URLFile* ouf,
    HRequest* hr, unsigned char* status);

char* filename_extension(char* patch, int is_url);
ParsedURL* schemeToProxy(int scheme);
wc_ces url_to_charset(const char* url, const ParsedURL* base, wc_ces doc_charset);
char* url_encode(const char* url, const ParsedURL* base, wc_ces doc_charset);
char* url_decode2(const char* url, const Buffer* buf);
