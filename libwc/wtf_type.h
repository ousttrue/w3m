#pragma once
#include "wc_types.h"

enum WtfType : wc_uchar {
    WTF_TYPE_ASCII = 0x0,
    WTF_TYPE_CTRL = 0x1,
    WTF_TYPE_WCHAR1 = 0x2,
    WTF_TYPE_WCHAR2 = 0x4,
    WTF_TYPE_WIDE = 0x8,
    WTF_TYPE_UNKNOWN = 0x10,
    WTF_TYPE_UNDEF = 0x20,
    WTF_TYPE_WCHAR1W = (WTF_TYPE_WCHAR1 | WTF_TYPE_WIDE),
    WTF_TYPE_WCHAR2W = (WTF_TYPE_WCHAR2 | WTF_TYPE_WIDE),
};

void wtf_type_set(size_t i, enum WtfType);
enum WtfType wtf_type(const wc_uchar* p);
