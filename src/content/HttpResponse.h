#pragma once
#include "textlist.h"
#include "url.h"
#include "istream.h"

struct HttpResponse {
    int status_code;
    TextList* headers;
};

struct HttpResponse readHttpResponse(struct Url* pu, union input_stream* stream);
