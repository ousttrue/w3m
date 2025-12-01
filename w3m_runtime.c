#include "w3m_runtime.h"
#include "w3m_config.h"
#include <gcstr.h>
#include <pwd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#define CONFIG_FILE "config"
#define W3MCONFIG "w3mconfig"

struct w3m w3m = { 0 };

#define DISPLAY_CHARSET WC_CES_UTF_8
#define DOCUMENT_CHARSET WC_CES_UTF_8
#define SYSTEM_CHARSET WC_CES_UTF_8

wc_ces DisplayCharset = DISPLAY_CHARSET;
wc_ces DocumentCharset = DOCUMENT_CHARSET;
wc_ces SystemCharset = SYSTEM_CHARSET;
wc_ces BookmarkCharset = SYSTEM_CHARSET;

#define do_mkdir(dir, mode) mkdir(dir, mode)

static int do_recursive_mkdir(const char* dir)
{
    if (*dir == '\0')
        return -1;

    char* dircpy = Strnew_charp(dir)->ptr;
    char* ch = dircpy + 1;
    do {
        while (!(*ch == '/' || *ch == '\0')) {
            ch++;
        }

        char tmp = *ch;
        *ch = '\0';

        struct stat st;
        if (stat(dircpy, &st) < 0) {
            if (errno != ENOENT) { /* no directory */
                return -1;
            }
            if (do_mkdir(dircpy, 0700) < 0) {
                return -1;
            }
            stat(dircpy, &st);
        }
        if (!S_ISDIR(st.st_mode)) {
            /* not a directory */
            return -1;
        }
        if (!(st.st_mode & S_IWUSR)) {
            return -1;
        }

        *ch = tmp;

    } while (*ch++ != '\0');
#ifdef HAVE_FACCESSAT
    if (faccessat(AT_FDCWD, dir, W_OK | X_OK, AT_EACCESS) < 0) {
        return -1;
    }
#endif

    return 0;
}

static Str etcFile(const char* base)
{
    return expandPath(Strnew_m_charp(w3m_etc_dir(), "/", base, NULL)->ptr);
}

Str confFile(const char* base)
{
    return expandPath(Strnew_m_charp(w3m_conf_dir(), "/", base, NULL)->ptr);
}

void w3m_initialize()
{
    if (!w3m.rc_dir) {

        w3m.rc_dir = allocStr(getenv("W3M_DIR"), -1);
        if (w3m.rc_dir == NULL || *w3m.rc_dir == '\0')
            w3m.rc_dir = allocStr(RC_DIR, -1);
        if (w3m.rc_dir == NULL || *w3m.rc_dir == '\0') {
            exit(1);
        }
        w3m.rc_dir = expandPath(w3m.rc_dir)->ptr;

        int i = strlen(w3m.rc_dir);
        if (i > 1 && w3m.rc_dir[i - 1] == '/')
            w3m.rc_dir[i - 1] = '\0';

        w3m.tmp_dir = w3m.rc_dir;

        if (do_recursive_mkdir(w3m.rc_dir) == -1) {
            exit(1);
        }

        if (w3m_config.config_file == NULL)
            w3m_config.config_file = rcFile(CONFIG_FILE)->ptr;

        config_initialize();
    }

    /* open config file */
    FILE* f;
    if ((f = fopen(etcFile(W3MCONFIG)->ptr, "rt")) != NULL) {
        config_load(fileno(f));
        fclose(f);
    }
    if ((f = fopen(confFile(CONFIG_FILE)->ptr, "rt")) != NULL) {
        config_load(fileno(f));
        fclose(f);
    }
    if (w3m_config.config_file && (f = fopen(w3m_config.config_file, "rt")) != NULL) {
        config_load(fileno(f));
        fclose(f);
    }

    init_tmp();
}

void init_tmp()
{
    int i;

    if (w3m_config.param_tmp_dir)
        w3m.tmp_dir = w3m_config.param_tmp_dir;
    if (*w3m.tmp_dir == '\0')
        w3m.tmp_dir = w3m.rc_dir;

    if (strcmp(w3m.tmp_dir, w3m.rc_dir) == 0) {
        return;
    }

    w3m.tmp_dir = expandPath(w3m.tmp_dir)->ptr;
    i = strlen(w3m.tmp_dir);
    if (i > 1 && w3m.tmp_dir[i - 1] == '/')
        w3m.tmp_dir[i - 1] = '\0';
    if (do_recursive_mkdir(w3m.tmp_dir) == -1)
        goto tmp_dir_err;
    return;

tmp_dir_err:
    if (((w3m.tmp_dir = getenv("TMPDIR")) == NULL || *w3m.tmp_dir == '\0') && ((w3m.tmp_dir = getenv("TMP")) == NULL || *w3m.tmp_dir == '\0') && ((w3m.tmp_dir = getenv("TEMP")) == NULL || *w3m.tmp_dir == '\0'))
        w3m.tmp_dir = "/tmp";
    w3m.tmp_dir = mkdtemp(Strnew_m_charp(w3m.tmp_dir, "/w3m-XXXXXX", NULL)->ptr);
    if (w3m.tmp_dir)
        ;
    else
        w3m.tmp_dir = w3m.rc_dir;
    return;
}

Str rcFile(const char* base)
{
    if (base && (base[0] == '/' || (base[0] == '.' && (base[1] == '/' || (base[1] == '.' && base[2] == '/'))) || (base[0] == '~' && base[1] == '/')))
        /* /file, ./file, ../file, ~/file */
        return expandPath(base);
    return expandPath(Strnew_m_charp(w3m.rc_dir, "/", base, NULL)->ptr);
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

Str expandPath(const char* name)
{
    if (name == NULL)
        return NULL;

    const char* p = name;
    if (*p == '~') {
        p++;
        Str extpath = NULL;
        if (IS_ALPHA(*p)) {
            struct passwd* passent;
            char* q = strchr(p, '/');
            if (q) { /* ~user/dir... */
                passent = getpwnam(allocStr(p, q - p));
                p = q;
            } else { /* ~user */
                passent = getpwnam(p);
                p = "";
            }
            if (!passent)
                goto rest;
            extpath = Strnew_charp(passent->pw_dir);
        } else if (*p == '/' || *p == '\0') { /* ~/dir... or ~ */
            extpath = Strnew_charp(getenv("HOME"));
        } else
            goto rest;
        if (Strcmp_charp(extpath, "/") == 0 && *p == '/')
            p++;
        Strcat_charp(extpath, p);
        return extpath;
    }
rest:
    return Strnew_charp(name);
}
