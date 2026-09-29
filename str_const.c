#include "str_const.h"
#include "pathdefs.h"
#include <string.h>

int strCmp(const void* s1, const void* s2) /* helper for qsort */
{
    return strcmp(*(const char* const*)s1, *(const char* const*)s2);
}

const char* mybasename(const char* path)
{
    const char* p = strrchr(path, '/');
    return p ? p + 1 : path;
}

static const char*
w3m_dir(const char* name, const char* dft)
{
#ifdef USE_PATH_ENVVAR
    char* value = getenv(name);
    return value ? value : dft;
#else
    return dft;
#endif
}

const char*
w3m_auxbin_dir(void)
{
    return w3m_dir("W3M_AUXBIN_DIR", AUXBIN_DIR);
}

const char*
w3m_lib_dir(void)
{
    /* FIXME: use W3M_CGIBIN_DIR? */
    return w3m_dir("W3M_LIB_DIR", CGIBIN_DIR);
}

const char*
w3m_etc_dir(void)
{
    return w3m_dir("W3M_ETC_DIR", ETC_DIR);
}

const char*
w3m_conf_dir(void)
{
    return w3m_dir("W3M_CONF_DIR", CONF_DIR);
}

const char*
w3m_help_dir(void)
{
    return w3m_dir("W3M_HELP_DIR", HELP_DIR);
}

