#include "w3m_runtime.h"

#define DISPLAY_CHARSET WC_CES_UTF_8
#define DOCUMENT_CHARSET WC_CES_UTF_8
#define SYSTEM_CHARSET WC_CES_UTF_8

wc_ces InnerCharset = WC_CES_WTF; /* Don't change */
wc_ces DisplayCharset = DISPLAY_CHARSET;
wc_ces DocumentCharset = DOCUMENT_CHARSET;
wc_ces SystemCharset = SYSTEM_CHARSET;
wc_ces BookmarkCharset = SYSTEM_CHARSET;
