#include "runtime.h"
#include <stdlib.h>

static const char*
w3m_dir(const char* name, char* dft)
{
    const char* value = getenv(name);
    return value ? value : dft;
}

const char* w3m_auxbin_dir()
{
    return w3m_dir("W3M_AUXBIN_DIR", AUXBIN_DIR);
}

const char* w3m_lib_dir()
{
    /* FIXME: use W3M_CGIBIN_DIR? */
    return w3m_dir("W3M_LIB_DIR", CGIBIN_DIR);
}

const char* w3m_etc_dir()
{
    return w3m_dir("W3M_ETC_DIR", ETC_DIR);
}

const char* w3m_conf_dir()
{
    return w3m_dir("W3M_CONF_DIR", CONF_DIR);
}

const char* w3m_help_dir()
{
    return w3m_dir("W3M_HELP_DIR", HELP_DIR);
}
