#include "w3m_rc.h"
#include "option.h"
#include "ftp.h"
#include "ssl_stream.h"
#include "tab_list.h"
#include "anchor_list.h"
#include "mysignal.h"
#include "document.h"
#include "func.h"
#include "maparea.h"
#include "mimehead.h"
#include "menu.h"
#include "cookie.h"
#include "history.h"
#include "compression.h"
#include "etc.h"
#include "mailcap.h"
#include "local_cgi.h"
#include "symbol.h"
#include "file.h"
#include "message.h"
#include "termcap_util.h"
#include "linein.h"
#include "siteconf.h"
#include "buffer.h"
#include "image.h"
#include "myctype.h"
#include "funcheader.h"
#include "parsetag.h"
#include "funcname1.h"
#include "html_form.h"
#include "siteconf.h"
#include "tab.h"
#include "buffer.h"
#include "image.h"
#include "screen.h"
#include "display.h"
#include "config.h"
#include "indep.h"
#include "myctype.h"
#include "w3m_types.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <libwc/conv.h>
#include <libwc/ucs.h>
#include <libwc/charset.h>
#include <libwc/ces.h>
#include <libwc/status.h>

#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

// static struct termios d_ioval;

// static char* getKeyData(int key)
// {
//     if (keyData == NULL)
//         return NULL;
//     return (char*)getHash_iv(keyData, key, NULL);
// }

char* searchKeyData(void)
{
    const char* data = NULL;
    if (getRuntime()->CurrentKeyData != NULL && *getRuntime()->CurrentKeyData != '\0')
        data = getRuntime()->CurrentKeyData;
    else if (getRuntime()->CurrentCmdData != NULL && *getRuntime()->CurrentCmdData != '\0')
        data = getRuntime()->CurrentCmdData;
    // else if (getRuntime()->CurrentKey >= 0)
    //     data = getKeyData(getRuntime()->CurrentKey);
    getRuntime()->CurrentKeyData = NULL;
    getRuntime()->CurrentCmdData = NULL;
    if (data == NULL || *data == '\0')
        return NULL;
    return allocStr(data, -1);
}

struct DefunContext defunContext()
{
    return (struct DefunContext) {
        .tab = CurrentTab(),
        .buf = CurrentTab()->currentBuffer,
    };
}

struct Runtime* getRuntime()
{
    return &g_runtime;
}

void parse_proxy(void)
{
    if (non_null(g_runtime.HTTP_proxy))
        parseURL(g_runtime.HTTP_proxy, &HTTP_proxy_parsed, NULL);
    if (non_null(g_runtime.HTTPS_proxy))
        parseURL(g_runtime.HTTPS_proxy, &HTTPS_proxy_parsed, NULL);
    if (non_null(g_runtime.FTP_proxy))
        parseURL(g_runtime.FTP_proxy, &FTP_proxy_parsed, NULL);
    if (non_null(g_runtime.NO_proxy))
        g_runtime.NO_proxy_domains = make_domain_list(g_runtime.NO_proxy);
}

char* url_quote_conv(const char* x, enum wc_ces c)
{
    return url_quote(wc_conv_strict(x, g_runtime.InnerCharset, c)->ptr);
}

char* conv_from_system(const char* x)
{
    return wc_conv(x, g_runtime.SystemCharset, g_runtime.InnerCharset)->ptr;
}

char* conv_to_system(const char* x)
{
    return wc_conv_strict(x, g_runtime.InnerCharset, g_runtime.SystemCharset)->ptr;
}

Str Str_conv_to_system(Str x)
{
    return wc_Str_conv_strict(x, g_runtime.InnerCharset, g_runtime.SystemCharset);
}

Str Str_conv_from_system(Str x)
{
    return wc_Str_conv((x), g_runtime.SystemCharset, g_runtime.InnerCharset);
}

#define MAXIMUM_COLS 1024
void tty_set_cols(int cols)
{
    g_runtime.cols = cols;
    if (g_runtime.cols > MAXIMUM_COLS) {
        g_runtime.cols = MAXIMUM_COLS;
    }
}

char graphchar(char c)
{
    return (((unsigned)(c) >= ' ' && (unsigned)(c) < 128) ? g_runtime.termcap.gcmap[(c) - ' '] : (c));
}

bool tty_init_termcap(void)
{
    const char* ent = getenv("TERM") ? getenv("TERM") : DEFAULT_TERM;
    if (ent == NULL) {
        fprintf(stderr, "TERM is not set\n");
        // reset_error_exit(SIGNAL_ARGLIST);
        return false;
    }

    if (!termcap_read(&g_runtime.termcap, ent)) {
        fprintf(stderr, "fail to init: %s\n", ent);
        // reset_error_exit(SIGNAL_ARGLIST);
        return false;
    }

    setlinescols();
    return true;
}

char* ttyname_tty(void)
{
    return ttyname(0);
    // g_runtime.tty_input);
}

// static void
// skip_escseq(void)
// {
//     int c = getch();
//     if (c == '[' || c == 'O') {
//         c = getch();
//         while (IS_DIGIT(c))
//             c = getch();
//     }
// }

// int sleep_till_anykey(int sec, bool purge)
// {
//     struct termios ioval;
//     tcgetattr(getRuntime()->tty_input, &ioval);
//     term_raw();
//
//     struct timeval tim;
//     tim.tv_sec = sec;
//     tim.tv_usec = 0;
//
//     fd_set rfd;
//     FD_ZERO(&rfd);
//     FD_SET(getRuntime()->tty_input, &rfd);
//
//     int ret = select(getRuntime()->tty_input + 1, &rfd, 0, 0, &tim);
//     if (ret > 0 && purge) {
//         int c = getch();
//         if (c == ESC_CODE)
//             skip_escseq();
//     }
//     int er = tcsetattr(getRuntime()->tty_input, TCSANOW, &ioval);
//     if (er == -1) {
//         printf("Error occurred: errno=%d\n", errno);
//         reset_error_exit(SIGNAL_ARGLIST);
//     }
//     return ret;
// }

void tty_MOVE(int line, int column)
{
    writestr(termcap_str_move(&g_runtime.termcap, (struct TermPosition) {
                                                      .column = column,
                                                      .line = line,
                                                  }));
}

void initscr(void)
{
    set_int();
    if (g_runtime.termcap._ti && !g_runtime.Do_not_use_ti_te)
        writestr(g_runtime.termcap._ti);
    screen_setup(g_runtime.lines, g_runtime.cols);
    tty_clear();
}

int graph_ok(void)
{
    if (g_runtime.UseGraphicChar != GRAPHIC_CHAR_DEC)
        return 0;
    return g_runtime.termcap._as[0] != 0 //
        && g_runtime.termcap._ae[0] != 0 //
        && g_runtime.termcap._ac[0] != 0;
}

static const char* title_str = NULL;

void term_title(const char* s)
{
    if (!fmInitialized())
        return;
    if (title_str != NULL) {
        // fprintf(tty_output_f, title_str, s);
    }
}

void bell(void)
{
    write1(7);
}

static Str
conv_form_encoding(Str val, struct FormItemList* fi, struct Buffer* buf)
{
    enum wc_ces charset = g_runtime.SystemCharset;
    if (fi->parent->charset)
        charset = fi->parent->charset;
    else if (buf->doc->charset && buf->doc->charset != WC_CES_US_ASCII)
        charset = buf->doc->charset;
    return wc_Str_conv_strict(val, g_runtime.InnerCharset, charset);
}

Str query_from_followform(struct Buffer* buf, struct FormItemList* fi, bool multipart)
{
    struct FormItemList* f2;
    FILE* body = NULL;
    Str query = Strnew();

    if (multipart) {
        query = tmpfname(TMPF_DFL, NULL);
        body = fopen(query->ptr, "w");
        if (body == NULL) {
            return query;
        }
        fi->parent->body = query->ptr;
        fi->parent->boundary = Sprintf("------------------------------%d%ld%ld%ld", getRuntime()->CurrentPid,
            fi->parent, fi->parent->body, fi->parent->boundary)
                                   ->ptr;
    }
    query = Strnew();
    for (f2 = fi->parent->item; f2; f2 = f2->next) {
        if (f2->name == NULL)
            continue;
        /* <ISINDEX> is translated into single text form */
        if (f2->name->length == 0 && (multipart || f2->type != FORM_INPUT_TEXT))
            continue;
        switch (f2->type) {
        case FORM_INPUT_RESET:
            /* do nothing */
            continue;
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_IMAGE:
            if (f2 != fi || f2->value == NULL)
                continue;
            break;
        case FORM_INPUT_RADIO:
        case FORM_INPUT_CHECKBOX:
            if (!f2->checked)
                continue;
        }
        if (multipart) {
            if (f2->type == FORM_INPUT_IMAGE) {
                int x = 0, y = 0;
                getMapXY(buf->doc, doc_retrieveCurrentImg(buf->doc), &x, &y);
                query = Strdup(conv_form_encoding(f2->name, fi, buf));
                Strcat_charp(query, ".x");
                form_write_data(body, fi->parent->boundary, query->ptr,
                    Sprintf("%d", x)->ptr);
                query = Strdup(conv_form_encoding(f2->name, fi, buf));
                Strcat_charp(query, ".y");
                form_write_data(body, fi->parent->boundary, query->ptr,
                    Sprintf("%d", y)->ptr);
            } else if (f2->name && f2->name->length > 0 && f2->value != NULL) {
                /* not IMAGE */
                query = conv_form_encoding(f2->value, fi, buf);
                if (f2->type == FORM_INPUT_FILE)
                    form_write_from_file(body, fi->parent->boundary,
                        conv_form_encoding(f2->name, fi, buf)->ptr,
                        query->ptr,
                        Str_conv_to_system(f2->value)->ptr);
                else
                    form_write_data(body, fi->parent->boundary,
                        conv_form_encoding(f2->name, fi, buf)->ptr,
                        query->ptr);
            }
        } else {
            /* not multipart */
            if (f2->type == FORM_INPUT_IMAGE) {
                int x = 0, y = 0;
                getMapXY(buf->doc, doc_retrieveCurrentImg(buf->doc), &x, &y);
                Strcat(query,
                    Str_form_quote(conv_form_encoding(f2->name, fi, buf)));
                Strcat(query, Sprintf(".x=%d&", x));
                Strcat(query,
                    Str_form_quote(conv_form_encoding(f2->name, fi, buf)));
                Strcat(query, Sprintf(".y=%d", y));
            } else {
                /* not IMAGE */
                if (f2->name && f2->name->length > 0) {
                    Strcat(query,
                        Str_form_quote(conv_form_encoding(f2->name, fi, buf)));
                    Strcat_char(query, '=');
                }
                if (f2->value != NULL) {
                    if (fi->parent->method == FORM_METHOD_INTERNAL)
                        Strcat(query, Str_form_quote(f2->value));
                    else {
                        Strcat(query,
                            Str_form_quote(conv_form_encoding(f2->value, fi, buf)));
                    }
                }
            }
            if (f2->next)
                Strcat_char(query, '&');
        }
    }
    if (multipart) {
        fprintf(body, "--%s--\r\n", fi->parent->boundary);
        fclose(body);
    } else {
        /* remove trailing & */
        while (Strlastchar(query) == '&')
            Strshrink(query, 1);
    }
    return query;
}

// static struct Buffer*
// loadNormalBuf(struct Buffer* buf, int renderframe)
// {
//     pushBuffer(buf);
//     if (renderframe && g_runtime.RenderFrame && Currentbuf->doc.frameset != NULL)
//         rFrame();
//     return buf;
// }

void pushEvent(int cmd, void* data)
{
    struct Event* event = New(struct Event);
    *event = (struct Event) {
        .cmd = cmd,
        .data = data,
        .next = NULL,
    };
    if (g_runtime.CurrentEvent)
        g_runtime.LastEvent->next = event;
    else
        g_runtime.CurrentEvent = event;
    g_runtime.LastEvent = event;
}

void w3m_end_frame()
{
    g_runtime.prev_key = g_runtime.CurrentKey;
    g_runtime.CurrentKey = -1;
    g_runtime.CurrentKeyData = NULL;
}

int is_wordchar(wc_uint32 c)
{
    return wc_is_ucs_alnum(c);
}

wc_uint32 getChar(const char* p)
{
    return wc_any_to_ucs(wtf_parse1((wc_uchar**)&p));
}

const char* GetWord(struct Buffer* buf)
{
    int b, e;
    const char* p = doc_getCurWord(buf->doc, &b, &e);
    if (p) {
        return Strnew_charp_n(p, e - b)->ptr;
    }
    return NULL;
}

#ifdef __MINGW32_VERSION
#define do_mkdir(dir, mode) mkdir(dir)
#else
#define do_mkdir(dir, mode) mkdir(dir, mode)
#endif /* not __MINW32_VERSION */

int do_recursive_mkdir(const char* dir)
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

    if (faccessat(AT_FDCWD, dir, W_OK | X_OK, AT_EACCESS) < 0) {
        return -1;
    }

    return 0;
}

void sync_with_option(void)
{
    init_tmp();
    if (g_runtime.PagerMax < TTY_LINES())
        g_runtime.PagerMax = TTY_LINES();
    g_runtime.WrapSearch = g_runtime.WrapDefault;
    parse_proxy();
    parse_cookie();
    initMailcap();
    initMimeTypes();
    initURIMethods();

    if (fmInitialized() && (g_runtime.displayImage || g_runtime.enable_inline_image))
        initImage();
    loadPasswd();
    loadPreForm();
    loadSiteconf();

    if (g_runtime.AcceptLang == NULL || *g_runtime.AcceptLang == '\0') {
        /* TRANSLATORS:
         * AcceptLang default: this is used in Accept-Language: HTTP request
         * header. For example, ja.po should translate it as
         * "ja;q=1.0, en;q=0.5" like that.
         */
        g_runtime.AcceptLang = "en;q=1.0";
    }
    if (g_runtime.AcceptEncoding == NULL || *g_runtime.AcceptEncoding == '\0')
        g_runtime.AcceptEncoding = acceptableEncoding();
    if (g_runtime.AcceptMedia == NULL || *g_runtime.AcceptMedia == '\0')
        g_runtime.AcceptMedia = acceptableMimeTypes();

    update_utf8_symbol();

    wtf_init(g_runtime.DocumentCharset, g_runtime.DisplayCharset);

    if (fmInitialized()) {
        keymap_init(false);
        initMenu();
    }
}

/// open config file
void open_rc()
{
    FILE* f;
    if ((f = fopen(etcFile(W3MCONFIG), "rt")) != NULL) {
        opt_load(fileno(f));
        fclose(f);
    }
    if ((f = fopen(confFile(CONFIG_FILE), "rt")) != NULL) {
        opt_load(fileno(f));
        fclose(f);
    }
    if (g_runtime.config_file && (f = fopen(g_runtime.config_file, "rt")) != NULL) {
        opt_load(fileno(f));
        fclose(f);
    }
}

void init_tmp(void)
{
    int i;

    if (g_runtime.param_tmp_dir)
        getRuntime()->tmp_dir = g_runtime.param_tmp_dir;
    if (*getRuntime()->tmp_dir == '\0')
        getRuntime()->tmp_dir = g_runtime.rc_dir;

    if (strcmp(getRuntime()->tmp_dir, g_runtime.rc_dir) == 0) {
        if (g_runtime.no_rc_dir)
            goto tmp_dir_err;
        return;
    }

    getRuntime()->tmp_dir = expandPath(getRuntime()->tmp_dir);
    i = strlen(getRuntime()->tmp_dir);
    if (i > 1 && getRuntime()->tmp_dir[i - 1] == '/')
        getRuntime()->tmp_dir[i - 1] = '\0';
    if (do_recursive_mkdir(getRuntime()->tmp_dir) == -1)
        goto tmp_dir_err;
    return;

tmp_dir_err:
#ifdef HAVE_MKDTEMP
    if (g_runtime.mkd_tmp_dir) {
        getRuntime()->tmp_dir = g_runtime.mkd_tmp_dir;
        return;
    }
#endif
    if (((getRuntime()->tmp_dir = getenv("TMPDIR")) == NULL || *getRuntime()->tmp_dir == '\0') && ((getRuntime()->tmp_dir = getenv("TMP")) == NULL || *getRuntime()->tmp_dir == '\0') && ((getRuntime()->tmp_dir = getenv("TEMP")) == NULL || *getRuntime()->tmp_dir == '\0'))
        getRuntime()->tmp_dir = "/tmp";
#ifdef HAVE_MKDTEMP
    getRuntime()->tmp_dir = mkdtemp(Strnew_m_charp(getRuntime()->tmp_dir, "/w3m-XXXXXX", NULL)->ptr);
    if (getRuntime()->tmp_dir)
        g_runtime.mkd_tmp_dir = getRuntime()->tmp_dir;
    else
        getRuntime()->tmp_dir = g_runtime.rc_dir;
#endif
    return;
}

char* rcFile(const char* base)
{
    if (base && (base[0] == '/' || (base[0] == '.' && (base[1] == '/' || (base[1] == '.' && base[2] == '/'))) || (base[0] == '~' && base[1] == '/')))
        /* /file, ./file, ../file, ~/file */
        return expandPath(base);
    return expandPath(Strnew_m_charp(g_runtime.rc_dir, "/", base, NULL)->ptr);
}

#if 0 /* not used */
char *
libFile(char *base)
{
    return expandPath(Strnew_m_charp(w3m_lib_dir(), "/", base, NULL)->ptr);
}
#endif

char* etcFile(const char* base)
{
    return expandPath(Strnew_m_charp(w3m_etc_dir(), "/", base, NULL)->ptr);
}

char* confFile(const char* base)
{
    return expandPath(Strnew_m_charp(w3m_conf_dir(), "/", base, NULL)->ptr);
}

#ifndef USE_HELP_CGI
char* helpFile(char* base)
{
    return expandPath(Strnew_m_charp(w3m_help_dir(), "/", base, NULL)->ptr);
}
#endif

void tty_clear()
{
    writestr(g_runtime.termcap._cl);
}

void showProgress(int64_t* linelen, int64_t* trbyte, size_t current_content_length)
{
    int i, j, rate, duration, eta, pos;
    static time_t last_time, start_time;
    time_t cur_time;
    Str messages;
    char *fmtrbyte, *fmrate;

    if (!fmInitialized())
        return;

    if (*linelen < 1024)
        return;
    if (current_content_length > 0) {
        double ratio;
        cur_time = time(0);
        if (*trbyte == 0) {
            screen_move((struct Vec2) { .y = LASTLINE(), .x = 0 });
            screen_clrtoeolx();
            start_time = cur_time;
        }
        *trbyte += *linelen;
        *linelen = 0;
        if (cur_time == last_time)
            return;
        last_time = cur_time;
        screen_move((struct Vec2) { .y = LASTLINE(), .x = 0 });
        ratio = 100.0 * (*trbyte) / current_content_length;
        fmtrbyte = convert_size2(*trbyte, current_content_length, 1);
        duration = cur_time - start_time;
        if (duration) {
            rate = *trbyte / duration;
            fmrate = convert_size(rate, 1);
            eta = rate ? (current_content_length - *trbyte) / rate : -1;
            messages = Sprintf("%11s %3.0f%% "
                               "%7s/s "
                               "eta %02d:%02d:%02d     ",
                fmtrbyte, ratio,
                fmrate,
                eta / (60 * 60), (eta / 60) % 60, eta % 60);
        } else {
            messages = Sprintf("%11s %3.0f%%                          ",
                fmtrbyte, ratio);
        }
        screen_wc_addstr(messages->ptr);
        pos = 42;
        i = pos + (TTY_COLS() - pos - 1) * (*trbyte) / current_content_length;
        screen_move((struct Vec2) { .y = LASTLINE(), .x = pos });
        screen_standout();
        screen_addch(' ', 1);
        for (j = pos + 1; j <= i; j++)
            screen_addch('|', 1);
        screen_standend();
        /* no_clrtoeol(); */
    } else {
        cur_time = time(0);
        if (*trbyte == 0) {
            screen_move((struct Vec2) { .y = LASTLINE(), .x = 0 });
            screen_clrtoeolx();
            start_time = cur_time;
        }
        *trbyte += *linelen;
        *linelen = 0;
        if (cur_time == last_time)
            return;
        last_time = cur_time;
        screen_move((struct Vec2) { .y = LASTLINE(), .x = 0 });
        fmtrbyte = convert_size(*trbyte, 1);
        duration = cur_time - start_time;
        if (duration) {
            fmrate = convert_size(*trbyte / duration, 1);
            messages = Sprintf("%7s loaded %7s/s", fmtrbyte, fmrate);
        } else {
            messages = Sprintf("%7s loaded", fmtrbyte);
        }
        message(messages->ptr);
    }
}

int searchKeyNum(void)
{
    char* d;
    int n = 1;

    d = searchKeyData();
    if (d != NULL)
        n = atoi(d);
    return n * PREC_NUM;
}

void _quitfm(bool confirm)
{
    const char* ans = "y";
    if (confirm)
        ans = inputChar("Do you want to exit w3m? (y/n)");
    if (!(ans && TOLOWER(*ans) == 'y')) {
        return;
    }

    term_title(""); /* XXX */
    if (getRuntime()->activeImage)
        termImage();
    exitRawMode();
    save_cookies();

    if (getRuntime()->UseHistory && getRuntime()->SaveURLHist)
        saveHistory(getRuntime()->URLHist, getRuntime()->URLHistSize);

    w3m_exit(0);
}

struct FollowResult gotoLabel(struct Buffer* buf, const char* label)
{
    struct FollowResult res = {
        .anchor = doc_searchURLLabel(buf->doc, label),
        0
    };
    if (!res.anchor) {
        disp_message(Sprintf("%s is not found", label)->ptr, TRUE);
        return res;
    }

    res.new_buf = buf_new(NULL);
    buf_copy(res.new_buf, buf);
    for (int i = 0; i < MAX_LB; i++)
        res.new_buf->linkBuffer[i] = NULL;
    res.new_buf->content->url.label = allocStr(label, -1);
    pushHashHist(getRuntime()->URLHist, parsedURL2Str(&res.new_buf->content->url)->ptr);
    (*res.new_buf->clone)++;
    // tab_push_buffer(getRuntime()->CurrentTab, buf);
    doc_gotoLine(buf->doc, res.anchor->start.line);
    if (getRuntime()->label_topline)
        buf->doc->topLine = doc_lineSkip(buf->doc, buf->doc->topLine,
            buf->doc->currentLine->linenumber
                - buf->doc->topLine->linenumber);
    buf->doc->pos = res.anchor->start.pos;
    doc_arrangeCursor(buf->doc);
    return res;
}

int handleMailto(const char* url)
{
    Str to;
    char* pos;

    if (strncasecmp(url, "mailto:", 7))
        return 0;
    if (!non_null(getRuntime()->Mailer)) {
        /* FIXME: gettextize? */
        disp_err_message("no mailer is specified", TRUE);
        return 1;
    }

    /* invoke external mailer */
    if (getRuntime()->MailtoOptions == MAILTO_OPTIONS_USE_MAILTO_URL) {
        to = Strnew_charp(html_unquote(url));
    } else {
        to = Strnew_charp(url + 7);
        if ((pos = strchr(to->ptr, '?')) != NULL)
            Strtruncate(to, pos - to->ptr);
    }
    exec_cmd(myExtCommand(getRuntime()->Mailer, shell_quote(file_unquote(to->ptr)), FALSE)->ptr);
    pushHashHist(getRuntime()->URLHist, url);
    return 1;
}

void _followI(bool do_download)
{
    if (Currentbuf->doc->firstLine == NULL)
        return;

    struct Anchor* a = doc_retrieveCurrentImg(Currentbuf->doc);
    if (a == NULL)
        return;
    message(Sprintf("loading %s", a->url)->ptr);
    if (do_download) {
        download_content(a->url, NULL,
            (struct LoadOption) { .base_url = buf_baseUrl(Currentbuf), .referer = NULL, .flag = 0 });
        return;
    }

    struct Content* content = get_content_cache(a->url, NULL,
        (struct LoadOption) { .base_url = buf_baseUrl(Currentbuf), .referer = NULL, .flag = 0 });
    if (!content) {
        char* emsg = Sprintf("Can't load %s", a->url)->ptr;
        disp_err_message(emsg, FALSE);
        return;
    }

    struct Buffer* buf = buf_new(content);
    tab_push_buffer(CurrentTab(), buf);
}

static char* tmpf_base[MAX_TMPF_TYPE] = {
    "tmp",
    "src",
    "frame",
    "cache",
    "cookie",
    "hist",
};
static unsigned int tmpf_seq[MAX_TMPF_TYPE];

Str tmpfname(enum TmpFileTypes type, const char* ext)
{
    Str tmpf;
    const char* dir;

    switch (type) {
    case TMPF_HIST:
        dir = getRuntime()->rc_dir;
        break;
    case TMPF_DFL:
    case TMPF_COOKIE:
    case TMPF_SRC:
    case TMPF_FRAME:
    case TMPF_CACHE:
    default:
        dir = getRuntime()->tmp_dir;
    }

    tmpf = Sprintf("%s/w3m%s%d-%d%s",
        dir,
        tmpf_base[type],
        getRuntime()->CurrentPid, tmpf_seq[type]++, (ext) ? ext : "");
    pushText(getRuntime()->fileToDelete, tmpf->ptr);
    return tmpf;
}

struct Content* goURL0(struct Buffer* buf, const char* prompt, bool relative)
{
    const char* url = searchKeyData();
    if (!url) {
        struct Hist* hist = copyHist(getRuntime()->URLHist);
        struct Anchor* a;

        struct Url* current = buf_baseUrl(buf);
        if (current) {
            char* c_url = parsedURL2Str(current)->ptr;
            if (getRuntime()->DefaultURLString == DEFAULT_URL_CURRENT)
                url = url_decode2(NULL, NULL, c_url);
            else
                pushHist(hist, c_url);
        }
        a = doc_retrieveCurrentAnchor(buf->doc);
        if (a) {
            struct Url p_url;
            parseURL2(a->url, &p_url, current);
            const char* a_url = parsedURL2Str(&p_url)->ptr;
            if (getRuntime()->DefaultURLString == DEFAULT_URL_LINK)
                url = url_decode2(buf_baseUrl(buf), buf->doc, a_url);
            else
                pushHist(hist, a_url);
        }
        url = inputLineHist(prompt, url, IN_URL, hist);
        if (url != NULL)
            url = skip_blanks(url);
    }

    struct Url* current;
    const char* referer;
    if (relative) {
        const int* no_referer_ptr = query_SCONF_NO_REFERER_FROM(&buf->content->url);
        current = buf_baseUrl(buf);
        if ((no_referer_ptr && *no_referer_ptr) || current == NULL || current->scheme == SCM_LOCAL || current->scheme == SCM_LOCAL_CGI)
            referer = NO_REFERER;
        else
            referer = parsedURL2RefererStr(&buf->content->url)->ptr;
        url = url_encode(url, current, buf->doc->charset);
    } else {
        current = NULL;
        referer = NULL;
        url = url_encode(url, NULL, 0);
    }
    if (url == NULL || *url == '\0') {
        return NULL;
    }
    if (*url == '#') {
        return gotoLabel(buf, url + 1).new_buf->content;
    }
    struct Url p_url;
    parseURL2(url, &p_url, current);
    pushHashHist(getRuntime()->URLHist, parsedURL2Str(&p_url)->ptr);
    return get_content_cache(url, NULL,
        (struct LoadOption) { .base_url = current, .referer = referer });
}

void _peekURL(struct Buffer* buf, bool only_img)
{
    struct Anchor* a;
    struct Url pu;
    static Str s = NULL;
    static Lineprop* p = NULL;
    Lineprop* pp;

    static int offset = 0, n;

    if (buf->doc->firstLine == NULL)
        return;

    if (getRuntime()->CurrentKey == getRuntime()->prev_key && s != NULL) {
        if (s->length - offset >= TTY_COLS())
            offset++;
        else if (s->length <= offset) /* bug ? */
            offset = 0;
        goto disp;
    } else {
        offset = 0;
    }
    s = NULL;
    a = (only_img ? NULL : doc_retrieveCurrentAnchor(buf->doc));
    if (a == NULL) {
        a = (only_img ? NULL : doc_retrieveCurrentForm(buf->doc));
        if (a == NULL) {
            a = doc_retrieveCurrentImg(buf->doc);
            if (a == NULL)
                return;
        } else
            s = Strnew_charp(form2str((struct FormItemList*)a->url));
    }
    if (s == NULL) {
        parseURL2(a->url, &pu, buf_baseUrl(buf));
        s = parsedURL2Str(&pu);
    }
    if (getRuntime()->DecodeURL)
        s = Strnew_charp(url_decode2(buf_baseUrl(buf), buf->doc, s->ptr));
    s = checkType(s, &pp, NULL);
    p = NewAtom_N(Lineprop, s->length);
    bcopy((void*)pp, (void*)p, s->length * sizeof(Lineprop));
disp:
    n = searchKeyNum();
    if (n > 1 && s->length > (n - 1) * (TTY_COLS() - 1))
        offset = (n - 1) * (TTY_COLS() - 1);
    while (offset < s->length && p[offset] & PC_WCHAR2)
        offset++;
    disp_message(&s->ptr[offset], TRUE);
}

Str currentURL(struct Buffer* buf)
{
    if (buf->bufferprop & BP_INTERNAL)
        return Strnew_size(0);
    return parsedURL2Str(&buf->content->url);
}

static void deleteFiles()
{
    struct Buffer* buf;
    char* f;

    for (struct TabBuffer* CurrentTab = FirstTab(); CurrentTab; CurrentTab = CurrentTab->nextTab) {
        while (CurrentTab->firstBuffer) {
            buf = CurrentTab->firstBuffer->back;
            buf_discard(CurrentTab->firstBuffer);
            CurrentTab->firstBuffer = buf;
        }
    }
    while ((f = popText(getRuntime()->fileToDelete)) != NULL) {
        unlink(f);
        if (getRuntime()->enable_inline_image == INLINE_IMG_SIXEL && strcmp(f + strlen(f) - 4, ".gif") == 0) {
            Str firstframe = Strnew_charp(f);
            Strcat_charp(firstframe, "-1");
            unlink(firstframe->ptr);
        }
    }
}

void w3m_exit(int i)
{
    deleteFiles();
    free_ssl_ctx();
    disconnectFTP();
    if (getRuntime()->mkd_tmp_dir)
        if (rmdir(getRuntime()->mkd_tmp_dir) != 0) {
            fprintf(stderr, "Can't remove temporary directory (%s)!\n", getRuntime()->mkd_tmp_dir);
            exit(1);
        }
    exit(i);
}

void _docCSet(enum wc_ces charset)
{
    if (Currentbuf->bufferprop & BP_INTERNAL)
        return;
    if (Currentbuf->content->sourcefile == NULL) {
        disp_message("Can't reload...", FALSE);
        return;
    }
    Currentbuf->doc->charset = charset;
}

/* spawn external browser */
void invoke_browser(const char* url)
{
    getRuntime()->CurrentKeyData = NULL; /* not allowed in w3m-control: */
    char* browser = searchKeyData();
    if (browser == NULL || *browser == '\0') {
        switch (getRuntime()->prec_num) {
        case 0:
        case 1:
            browser = getRuntime()->ExtBrowser;
            break;
        case 2:
            browser = getRuntime()->ExtBrowser2;
            break;
        case 3:
            browser = getRuntime()->ExtBrowser3;
            break;
        case 4:
            browser = getRuntime()->ExtBrowser4;
            break;
        case 5:
            browser = getRuntime()->ExtBrowser5;
            break;
        case 6:
            browser = getRuntime()->ExtBrowser6;
            break;
        case 7:
            browser = getRuntime()->ExtBrowser7;
            break;
        case 8:
            browser = getRuntime()->ExtBrowser8;
            break;
        case 9:
            browser = getRuntime()->ExtBrowser9;
            break;
        }
        if (browser == NULL || *browser == '\0') {
            browser = inputStr("Browse command: ", NULL);
            if (browser != NULL)
                browser = conv_to_system(browser);
        }
    } else {
        browser = conv_to_system(browser);
    }
    if (browser == NULL || *browser == '\0') {
        return;
    }

    int bg = 0, len;
    if ((len = strlen(browser)) >= 2 && browser[len - 1] == '&' && browser[len - 2] != '\\') {
        browser = allocStr(browser, len - 2);
        bg = 1;
    }
    Str cmd = myExtCommand(browser, shell_quote(url), FALSE);
    Strremovetrailingspaces(cmd);
    exitRawMode();
    mySystem(cmd->ptr, bg);
    enterRawMode();
}

void follow_map(struct Buffer* buf, struct parsed_tagarg* arg)
{
    char* name = tag_get_value(arg, "link");
    int x, y;
    struct Url p_url;

    struct Anchor* an = doc_retrieveCurrentImg(Currentbuf->doc);
    x = Currentbuf->doc->cursorX + Currentbuf->doc->rootX;
    y = Currentbuf->doc->cursorY + Currentbuf->doc->rootY;
    struct MapArea* a = follow_map_menu(Currentbuf->doc, name, an, x, y);
    if (a == NULL || a->url == NULL || *(a->url) == '\0') {
        return;
    }
    if (*(a->url) == '#') {
        gotoLabel(Currentbuf, a->url + 1);
        return;
    }
    parseURL2(a->url, &p_url, buf_baseUrl(Currentbuf));
    pushHashHist(getRuntime()->URLHist, parsedURL2Str(&p_url)->ptr);
    struct Content* content = get_content_cache(a->url, NULL,
        (struct LoadOption) {
            .base_url = buf_baseUrl(Currentbuf),
            .referer = parsedURL2Str(&Currentbuf->content->url)->ptr });
    if (!content) {
        return;
    }

    {
        struct Buffer* new_buf = buf_new(content);
        if (getRuntime()->check_target
            && getRuntime()->open_tab_blank
            && a->target
            && (!strcasecmp(a->target, "_new") || !strcasecmp(a->target, "_blank"))) {
            tabs_append(new_buf);
        } else {
            tab_push_buffer(CurrentTab(), new_buf);
        }
    }
}

void change_charset(struct Buffer* _buf, struct parsed_tagarg* arg)
{
    struct Buffer* buf = _buf->linkBuffer[LB_N_INFO];
    if (buf == NULL)
        return;

    tab_delBuffer(CurrentTab(), Currentbuf);
    Currentbuf = buf;
    if (Currentbuf->bufferprop & BP_INTERNAL)
        return;

    enum wc_ces charset = Currentbuf->doc->charset;
    for (; arg; arg = arg->next) {
        if (!strcmp(arg->arg, "charset"))
            charset = atoi(arg->value);
    }
    _docCSet(charset);
}

/* mark URL-like patterns as anchors */
void chkURLBuffer(struct Buffer* buf)
{
    static char* url_like_pat[] = {
        "https?://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./?=~_\\&+@#,\\$;]*[a-zA-Z0-9_/=\\-]",
        "file:/[a-zA-Z0-9:%\\-\\./=_\\+@#,\\$;]*",
        "ftp://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*[a-zA-Z0-9_/]",
        "https?://[a-zA-Z0-9:%\\-\\./_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./?=~_\\&+@#,\\$;]*",
        "ftp://[a-zA-Z0-9:%\\-\\./_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*",
        NULL
    };
    for (int i = 0; url_like_pat[i]; i++) {
        doc_reAnchor(buf_baseUrl(buf), buf->doc, url_like_pat[i]);
    }
    chkExternalURIBuffer(buf);
    buf->check_url |= CHK_URL;
}

void tmpClearBuffer(struct Buffer* buf)
{
}

bool eventUpdate()
{
    struct Runtime* g = getRuntime();
    if (!g->CurrentEvent) {
        return false;
    }
    g->CurrentKey = -1;
    g->CurrentKeyData = NULL;
    g->CurrentCmdData = (char*)g->CurrentEvent->data;
    DefunFunc func = keymap_fromName(g->CurrentEvent->cmd);
    func((struct DefunContext) {
        .tab = CurrentTab(),
        .buf = CurrentTab()->currentBuffer,
    });
    g->CurrentCmdData = NULL;
    g->CurrentEvent = g->CurrentEvent->next;
    return true;
}
