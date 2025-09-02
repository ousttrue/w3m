#pragma once
#include "HtmlTagAttribute.h"

enum AttributeFlag {
    AFLG_INT = 1,
};

typedef struct tag_attribute_info {
    const char* name;
    enum AttributeValueType vtype;
    enum AttributeFlag flag;
} TagAttrInfo;
extern TagAttrInfo AttrMAP[];
