#pragma once
#include "HtmlTag.h"

enum HtmlTagFlag {
    TFLG_NONE = 0,
    TFLG_END = 1,
    TFLG_INT = 2,
};

typedef struct html_tag_info {
    const char* name;
    enum HtmlTagAttribute* accept_attribute;
    int max_attribute;
    enum HtmlTagFlag flag;
} TagInfo;
extern TagInfo TagMAP[];
