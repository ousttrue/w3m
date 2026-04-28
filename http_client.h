#pragma once

enum UrlOptionFlags {
    RG_NOCACHE = 1,
    RG_FRAME = 2,
    RG_FRAME_SRC = 4,
};

struct HttpClient {
    struct Url* current;
    const char* referer;
    enum UrlOptionFlags flag;
    struct Form* post;
};
