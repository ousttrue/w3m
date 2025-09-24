#pragma once
#include "Content.h"
#include "Document.h"

struct Buffer {
    struct Content content;
    struct Document document;
    struct Buffer* nextBuffer;

    int* clone;
    bool check_url;
    wc_uint8 auto_detect;
    char* savecache;
    struct _AlarmEvent* event;
};
