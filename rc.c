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
#include <sys/stat.h>

#include "funcheader.h"

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

void panel_set_option(struct KeyValueList* arg)
{
    FILE* f = NULL;
    char* p;
    Str s = Strnew(), tmp;

    if (w3m_config.config_file == NULL) {
        tui_disp_message("There's no config file... config not saved", false);
    } else {
        f = fopen(w3m_config.config_file, "wt");
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

char* auxbinFile(char* base)
{
    return expandPath(Strnew_m_charp(w3m_auxbin_dir(), "/", base, NULL)->ptr)->ptr;
}


