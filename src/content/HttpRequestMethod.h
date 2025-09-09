#pragma once

enum HttpMethod {
    HTTP_METHOD_GET = 0,
    HTTP_METHOD_POST = 1,
    HTTP_METHOD_CONNECT = 2,
    HTTP_METHOD_HEAD = 3,
};
static inline const char* httpRequestMethodStr(enum HttpMethod method)
{
    switch (method) {
    case HTTP_METHOD_CONNECT:
        return "CONNECT";
    case HTTP_METHOD_POST:
        return "POST";
    case HTTP_METHOD_HEAD:
        return "HEAD";
    case HTTP_METHOD_GET:
    default:
        break;
    }
    return "GET";
}
