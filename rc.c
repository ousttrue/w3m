#include "rc.h"
#include "fm.h"
#include "terms.h"
#include "w3m_runtime.h"
#include "Url.h"
#include "tui.h"
#include "symbol.h"
#include "file.h"
#include "form.h"
#include "mimetype.h"
#include "func.h"
#include "mailcap.h"
#include "menu.h"
#include "cookie.h"
#include "image.h"
#include "http_auth.h"
#include <wc/wtf.h>
#include <errno.h>
#include "KeyValueList.h"
#include "w3m_config.h"
#include <sys/stat.h>

#include "funcheader.h"

#define CONFIG_FILE "config"
#define W3MCONFIG "w3mconfig"

#define set_no_proxy(domains) (NO_proxy_domains = make_domain_list(domains))

static void
parse_proxy(void)
{
    if (non_null(HTTP_proxy))
        parseURL(HTTP_proxy, &HTTP_proxy_parsed, NULL);
    if (non_null(HTTPS_proxy))
        parseURL(HTTPS_proxy, &HTTPS_proxy_parsed, NULL);
    if (non_null(GOPHER_proxy))
        parseURL(GOPHER_proxy, &GOPHER_proxy_parsed, NULL);
    if (non_null(FTP_proxy))
        parseURL(FTP_proxy, &FTP_proxy_parsed, NULL);
    if (non_null(NO_proxy))
        set_no_proxy(NO_proxy);
}

static void
parse_cookie(void)
{
    if (non_null(cookie_reject_domains))
        Cookie_reject_domains = make_domain_list(cookie_reject_domains);
    if (non_null(cookie_accept_domains))
        Cookie_accept_domains = make_domain_list(cookie_accept_domains);
    if (non_null(cookie_avoid_wrong_number_of_dots))
        Cookie_avoid_wrong_number_of_dots_domains
            = make_domain_list(cookie_avoid_wrong_number_of_dots);
}

#define do_mkdir(dir, mode) mkdir(dir, mode)

static int
do_recursive_mkdir(const char* dir)
{
    char *ch, *dircpy, tmp;
    struct stat st;

    if (*dir == '\0')
        return -1;

    dircpy = Strnew_charp(dir)->ptr;
    ch = dircpy + 1;
    do {
        while (!(*ch == '/' || *ch == '\0')) {
            ch++;
        }

        tmp = *ch;
        *ch = '\0';

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

void sync_with_option(void)
{
    init_tmp();
    if (PagerMax < LINES)
        PagerMax = LINES;
    WrapSearch = WrapDefault;
    parse_proxy();
    parse_cookie();
    initMailcap();
    initMimeTypes();
    if ((displayImage || enable_inline_image))
        initImage();
    loadPasswd();
    loadPreForm();

    if (AcceptLang == NULL || *AcceptLang == '\0') {
        /* TRANSLATORS:
         * AcceptLang default: this is used in Accept-Language: HTTP request
         * header. For example, ja.po should translate it as
         * "ja;q=1.0, en;q=0.5" like that.
         */
        AcceptLang = "en;q=1.0";
    }
    if (AcceptEncoding == NULL || *AcceptEncoding == '\0')
        AcceptEncoding = acceptableEncoding();
    if (AcceptMedia == NULL || *AcceptMedia == '\0')
        AcceptMedia = acceptableMimeTypes();
    update_utf8_symbol();
    wtf_init(DocumentCharset, DisplayCharset);

    initKeymap(SystemCharset, InnerCharset, false);
    initMenu();
}

void init_rc(void)
{
    int i;
    FILE* f;

    if (rc_dir != NULL)
        goto open_rc;

    rc_dir = allocStr(getenv("W3M_DIR"), -1);
    if (rc_dir == NULL || *rc_dir == '\0')
        rc_dir = allocStr(RC_DIR, -1);
    if (rc_dir == NULL || *rc_dir == '\0') {
        exit(1);
    }
    rc_dir = expandPath(rc_dir)->ptr;

    i = strlen(rc_dir);
    if (i > 1 && rc_dir[i - 1] == '/')
        rc_dir[i - 1] = '\0';

    tmp_dir = rc_dir;

    if (do_recursive_mkdir(rc_dir) == -1) {
        exit(1);
    }

    if (config_file == NULL)
        config_file = rcFile(CONFIG_FILE);

    config_initialize();

open_rc:
    /* open config file */
    if ((f = fopen(etcFile(W3MCONFIG), "rt")) != NULL) {
        config_load(f);
        fclose(f);
    }
    if ((f = fopen(confFile(CONFIG_FILE), "rt")) != NULL) {
        config_load(f);
        fclose(f);
    }
    if (config_file && (f = fopen(config_file, "rt")) != NULL) {
        config_load(f);
        fclose(f);
    }
}

void init_tmp(void)
{
    int i;

    if (param_tmp_dir)
        tmp_dir = param_tmp_dir;
    if (*tmp_dir == '\0')
        tmp_dir = rc_dir;

    if (strcmp(tmp_dir, rc_dir) == 0) {
        return;
    }

    tmp_dir = expandPath(tmp_dir)->ptr;
    i = strlen(tmp_dir);
    if (i > 1 && tmp_dir[i - 1] == '/')
        tmp_dir[i - 1] = '\0';
    if (do_recursive_mkdir(tmp_dir) == -1)
        goto tmp_dir_err;
    return;

tmp_dir_err:
    if (((tmp_dir = getenv("TMPDIR")) == NULL || *tmp_dir == '\0') && ((tmp_dir = getenv("TMP")) == NULL || *tmp_dir == '\0') && ((tmp_dir = getenv("TEMP")) == NULL || *tmp_dir == '\0'))
        tmp_dir = "/tmp";
    tmp_dir = mkdtemp(Strnew_m_charp(tmp_dir, "/w3m-XXXXXX", NULL)->ptr);
    if (tmp_dir)
        ;
    else
        tmp_dir = rc_dir;
    return;
}

void panel_set_option(struct KeyValueList* arg)
{
    FILE* f = NULL;
    char* p;
    Str s = Strnew(), tmp;

    if (config_file == NULL) {
        tui_disp_message("There's no config file... config not saved", false);
    } else {
        f = fopen(config_file, "wt");
        if (f == NULL) {
            tui_disp_message("Can't write option!", false);
        }
    }
    while (arg) {
        /*  InnerCharset -> SystemCharset */
        if (arg->value) {
            p = conv_to_system(arg->value);
            if (config_set_param(arg->arg, p)) {
                tmp = Sprintf("%s %s\n", arg->arg, p);
                Strcat(tmp, s);
                s = tmp;
            }
        }
        arg = arg->next;
    }
    if (f) {
        fputs(s->ptr, f);
        fclose(f);
    }
    sync_with_option();
    backBf();
}

char* rcFile(char* base)
{
    if (base && (base[0] == '/' || (base[0] == '.' && (base[1] == '/' || (base[1] == '.' && base[2] == '/'))) || (base[0] == '~' && base[1] == '/')))
        /* /file, ./file, ../file, ~/file */
        return expandPath(base)->ptr;
    return expandPath(Strnew_m_charp(rc_dir, "/", base, NULL)->ptr)->ptr;
}

char* auxbinFile(char* base)
{
    return expandPath(Strnew_m_charp(w3m_auxbin_dir(), "/", base, NULL)->ptr)->ptr;
}

char* etcFile(char* base)
{
    return expandPath(Strnew_m_charp(w3m_etc_dir(), "/", base, NULL)->ptr)->ptr;
}

char* confFile(char* base)
{
    return expandPath(Strnew_m_charp(w3m_conf_dir(), "/", base, NULL)->ptr)->ptr;
}
