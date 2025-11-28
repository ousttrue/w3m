#include "w3m_runtime.h"
#include <gcstr.h>

const char* CurrentDir;
int CurrentPid;
const char* MyProgramName = ("w3m");

#define DISPLAY_CHARSET WC_CES_UTF_8
#define DOCUMENT_CHARSET WC_CES_UTF_8
#define SYSTEM_CHARSET WC_CES_UTF_8

wc_ces DisplayCharset = DISPLAY_CHARSET;
wc_ces DocumentCharset = DOCUMENT_CHARSET;
wc_ces SystemCharset = SYSTEM_CHARSET;
wc_ces BookmarkCharset = SYSTEM_CHARSET;

char* url_quote_conv(const char* x, wc_ces c)
{
    return url_quote(wc_conv_strict((x), InnerCharset, (c))->ptr)->ptr;
}
