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

static char*
w3m_dir(const char* name, char* dft)
{
#ifdef USE_PATH_ENVVAR
    char* value = getenv(name);
    return value ? value : dft;
#else
    return dft;
#endif
}

char* w3m_auxbin_dir(void)
{
    return w3m_dir("W3M_AUXBIN_DIR", AUXBIN_DIR);
}

char* w3m_lib_dir(void)
{
    /* FIXME: use W3M_CGIBIN_DIR? */
    return w3m_dir("W3M_LIB_DIR", CGIBIN_DIR);
}

char* w3m_etc_dir(void)
{
    return w3m_dir("W3M_ETC_DIR", ETC_DIR);
}

char* w3m_conf_dir(void)
{
    return w3m_dir("W3M_CONF_DIR", CONF_DIR);
}

char* w3m_help_dir(void)
{
    return w3m_dir("W3M_HELP_DIR", HELP_DIR);
}
