#include "w3m_rc.h"
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

// rc
char UseGraphicChar = GRAPHIC_CHAR_CHARSET;

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
    if (UseGraphicChar != GRAPHIC_CHAR_DEC)
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

struct Buffer* loadLink(const char* url, struct FormList* request,
    const char* target, const char* referer, struct FollowOption option)
{
    message(Sprintf("loading %s", url)->ptr);

    const int* no_referer_ptr = query_SCONF_NO_REFERER_FROM(&CurrentTab()->currentBuffer->content->url);
    struct Url* base = baseURL(Currentbuf);
    if ((no_referer_ptr && *no_referer_ptr) || base == NULL || base->scheme == SCM_LOCAL || base->scheme == SCM_LOCAL_CGI)
        referer = NO_REFERER;
    if (referer == NULL)
        referer = parsedURL2RefererStr(&Currentbuf->content->url)->ptr;
    if (option.do_download) {
        download_content(url, request,
            (struct LoadOption) { .base_url = baseURL(Currentbuf), .referer = referer, .flag = 0 });
        return NULL;
    }

    struct Content* content = get_content_cache(url, request,
        (struct LoadOption) { .base_url = baseURL(Currentbuf), .referer = referer, .flag = 0 });
    if (!content) {
        char* emsg = Sprintf("Can't load %s", url)->ptr;
        disp_err_message(emsg, FALSE);
        return NULL;
    }
    struct Buffer* buf = buf_new(content);

    // struct Url pu;
    // parseURL2(url, &pu, base);
    // pushHashHist(g_runtime.URLHist, parsedURL2Str(&pu)->ptr);
    //
    // if (!option.on_target) /* open link as an indivisual page */
    //     return loadNormalBuf(buf, TRUE);
    //
    // if (option.do_download) /* download (thus no need to render frames) */
    //     return loadNormalBuf(buf, FALSE);
    //
    // if (target == NULL || /* no target specified (that means this page is not a frame page) */
    //     !strcmp(target, "_top") || /* this link is specified to be opened as an indivisual * page */
    //     !(Currentbuf->bufferprop & BP_FRAME) /* This page is not a frame page */
    // ) {
    //     return loadNormalBuf(buf, TRUE);
    // }
    // struct Buffer* nfbuf = Currentbuf->linkBuffer[LB_N_FRAME];
    // if (nfbuf == NULL) {
    //     /* original page (that contains <frameset> tag) doesn't exist */
    //     return loadNormalBuf(buf, TRUE);
    // }
    //
    // union frameset_element* f_element = search_frame(nfbuf->doc.frameset, target);
    // if (f_element == NULL) {
    //     /* specified target doesn't exist in this frameset */
    //     return loadNormalBuf(buf, TRUE);
    // }
    //
    // /* frame page */
    //
    // /* stack current frameset */
    // pushFrameTree(&(nfbuf->doc.frameQ), copyFrameSet(nfbuf->doc.frameset), Currentbuf);
    // /* delete frame view buffer */
    // delBuffer(Currentbuf);
    // Currentbuf = nfbuf;
    // /* nfbuf->frameset = copyFrameSet(nfbuf->frameset); */
    // resetFrameElement(f_element, buf, referer, request);
    // discardBuffer(buf);
    // rFrame();
    // {
    //     struct Anchor* al = NULL;
    //     char* label = pu.label;
    //
    //     if (label && f_element->element->attr == F_BODY) {
    //         al = searchAnchor(f_element->body->nameList, label);
    //     }
    //     if (!al) {
    //         label = Strnew_m_charp("_", target, NULL)->ptr;
    //         al = searchURLLabel(Currentbuf->doc, label);
    //     }
    //     if (al) {
    //         doc_gotoLine(&Currentbuf->doc, al->start.line);
    //         if (g_runtime.label_topline)
    //             Currentbuf->doc.topLine = doc_lineSkip(&Currentbuf->doc, Currentbuf->doc.topLine,
    //                 Currentbuf->doc.currentLine->linenumber - Currentbuf->doc.topLine->linenumber);
    //         Currentbuf->doc.pos = al->start.pos;
    //         doc_arrangeCursor(&Currentbuf->doc);
    //     }
    // }
    return buf;
}

static struct FormItemList*
save_submit_formlist(struct FormItemList* src)
{
    struct FormList* list;
    struct FormList* srclist;
    struct FormItemList* srcitem;
    struct FormItemList* item;
    struct FormItemList* ret = NULL;
    struct FormSelectOptionItem* opt;
    struct FormSelectOptionItem* curopt;
    struct FormSelectOptionItem* srcopt;

    if (src == NULL)
        return NULL;
    srclist = src->parent;
    list = New(struct FormList);
    list->method = srclist->method;
    list->action = Strdup(srclist->action);
    list->charset = srclist->charset;
    list->enctype = srclist->enctype;
    list->nitems = srclist->nitems;
    list->body = srclist->body;
    list->boundary = srclist->boundary;
    list->length = srclist->length;

    for (srcitem = srclist->item; srcitem; srcitem = srcitem->next) {
        item = New(struct FormItemList);
        item->type = srcitem->type;
        item->name = Strdup(srcitem->name);
        item->value = Strdup(srcitem->value);
        item->checked = srcitem->checked;
        item->accept = srcitem->accept;
        item->size = srcitem->size;
        item->rows = srcitem->rows;
        item->maxlength = srcitem->maxlength;
        item->readonly = srcitem->readonly;

        opt = curopt = NULL;
        for (srcopt = srcitem->select_option; srcopt; srcopt = srcopt->next) {
            if (!srcopt->checked)
                continue;
            opt = New(struct FormSelectOptionItem);
            opt->value = Strdup(srcopt->value);
            opt->label = Strdup(srcopt->label);
            opt->checked = srcopt->checked;
            if (item->select_option == NULL) {
                item->select_option = curopt = opt;
            } else {
                curopt->next = opt;
                curopt = curopt->next;
            }
        }
        item->select_option = opt;
        if (srcitem->label)
            item->label = Strdup(srcitem->label);

        item->parent = list;
        item->next = NULL;

        if (list->lastitem == NULL) {
            list->item = list->lastitem = item;
        } else {
            list->lastitem->next = item;
            list->lastitem = item;
        }

        if (srcitem == src)
            ret = item;
    }

    return ret;
}

static struct Buffer* do_submit(struct Buffer* buf, struct Anchor* a, struct FormItemList* fi,
    const char* p,
    struct FollowOption option)
{
    int multipart = (fi->parent->method == FORM_METHOD_POST && fi->parent->enctype == FORM_ENCTYPE_MULTIPART);
    Str tmp = query_from_followform(buf, fi, multipart);

    Str tmp2 = Strdup(fi->parent->action);
    if (!Strcmp_charp(tmp2, "!CURRENT_URL!")) {
        /* It means "current URL" */
        tmp2 = parsedURL2Str(&buf->content->url);
        if ((p = strchr(tmp2->ptr, '?')) != NULL)
            Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
    }

    if (fi->parent->method == FORM_METHOD_GET) {
        if ((p = strchr(tmp2->ptr, '?')) != NULL)
            Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
        Strcat_charp(tmp2, "?");
        Strcat(tmp2, tmp);
        return loadLink(tmp2->ptr, NULL, a->target, NULL, option);
    } else if (fi->parent->method == FORM_METHOD_POST) {
        if (multipart) {
            struct stat st;
            stat(fi->parent->body, &st);
            fi->parent->length = st.st_size;
        } else {
            fi->parent->body = tmp->ptr;
            fi->parent->length = tmp->length;
        }
        struct Buffer* new_buf = loadLink(tmp2->ptr, fi->parent, a->target, NULL, option);
        tab_push_buffer(CurrentTab(), new_buf);
        if (multipart) {
            unlink(fi->parent->body);
        }
        if (new_buf && !(new_buf->bufferprop & BP_REDIRECTED)) { /* buf must be Currentbuf */
            /* BP_REDIRECTED means that the buffer is obtained through
             * Location: header. In this case, buf->form_submit must not be set
             * because the page is not loaded by POST method but GET method.
             */
            new_buf->doc->form_submit = save_submit_formlist(fi);
        }
        return new_buf;
    } else if ((fi->parent->method == FORM_METHOD_INTERNAL
                   && (!Strcmp_charp(fi->parent->action, "map")
                       || !Strcmp_charp(fi->parent->action, "none")))
        || buf->bufferprop & BP_INTERNAL) { /* internal */
        do_internal(tmp2->ptr, tmp->ptr);
    } else {
        disp_err_message("Can't send form because of illegal method.", false);
    }
    return NULL;
}

struct FollowResult _followForm(struct Buffer* buf, struct FollowOption option, bool submit)
{
    if (!buf->doc->firstLine)
        return (struct FollowResult) { 0 };

    struct Anchor* a = doc_retrieveCurrentForm(buf->doc);
    if (!a)
        return (struct FollowResult) { 0 };

    struct FormItemList* fi = (struct FormItemList*)a->url;
    switch (fi->type) {
    case FORM_INPUT_TEXT: {
        if (submit) {
            return (struct FollowResult) {
                .anchor = a,
                .new_buf = do_submit(buf, a, fi, NULL, option),
            };
        }
        if (fi->readonly) {
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
        }
        char* p = inputStrHist("TEXT:", fi->value ? fi->value->ptr : NULL, g_runtime.TextHist);
        if (p == NULL || fi->readonly)
            break;
        fi->value = Strnew_charp(p);
        doc_formUpdateBuffer(buf->doc, a, fi);
        if (fi->accept || fi->parent->nitems == 1) {
            return (struct FollowResult) {
                .anchor = a,
                .new_buf = do_submit(buf, a, fi, p, option),
            };
        }
        buf->doc->lineUpdated = true;
        break;
    }
    case FORM_INPUT_FILE: {
        if (submit) {
            return (struct FollowResult) {
                .anchor = a,
                .new_buf = do_submit(buf, a, fi, NULL, option),
            };
        }
        if (fi->readonly)
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
        char* p = inputFilenameHist("Filename:", fi->value ? fi->value->ptr : NULL, NULL);
        if (p == NULL || fi->readonly)
            break;
        fi->value = Strnew_charp(p);
        doc_formUpdateBuffer(buf->doc, a, fi);
        if (fi->accept || fi->parent->nitems == 1) {
            return (struct FollowResult) {
                .anchor = a,
                .new_buf = do_submit(buf, a, fi, p, option),
            };
        }
        break;
    }
    case FORM_INPUT_PASSWORD: {
        if (submit) {
            return (struct FollowResult) {
                .anchor = a,
                .new_buf = do_submit(buf, a, fi, NULL, option),
            };
        }
        if (fi->readonly) {
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
            break;
        }
        char* p = inputLine("Password:", fi->value ? fi->value->ptr : NULL, IN_PASSWORD);
        if (p == NULL)
            break;
        fi->value = Strnew_charp(p);
        doc_formUpdateBuffer(buf->doc, a, fi);
        if (fi->accept) {
            return (struct FollowResult) {
                .anchor = a,
                .new_buf = do_submit(buf, a, fi, p, option),
            };
        }
        break;
    }
    case FORM_TEXTAREA:
        if (submit) {
            return (struct FollowResult) {
                .anchor = a,
                .new_buf = do_submit(buf, a, fi, NULL, option),
            };
        }
        if (fi->readonly)
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
        input_textarea(fi);
        doc_formUpdateBuffer(buf->doc, a, fi);
        break;

    case FORM_INPUT_RADIO:
        if (submit) {
            return (struct FollowResult) {
                .anchor = a,
                .new_buf = do_submit(buf, a, fi, NULL, option),
            };
        }
        if (fi->readonly) {
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
            break;
        }
        formRecheckRadio(buf, a, fi);
        break;

    case FORM_INPUT_CHECKBOX:
        if (submit) {
            return (struct FollowResult) {
                .anchor = a,
                .new_buf = do_submit(buf, a, fi, NULL, option),
            };
        }
        if (fi->readonly) {
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
            break;
        }
        fi->checked = !fi->checked;
        doc_formUpdateBuffer(buf->doc, a, fi);
        break;

    case FORM_SELECT:
        if (submit) {
            return (struct FollowResult) {
                .anchor = a,
                .new_buf = do_submit(buf, a, fi, NULL, option),
            };
        }
        if (!formChooseOptionByMenu(fi,
                buf->doc->cursorX - buf->doc->pos + a->start.pos + buf->doc->rootX,
                buf->doc->cursorY + buf->doc->rootY))
            break;
        doc_formUpdateBuffer(buf->doc, a, fi);
        if (fi->parent->nitems == 1) {
            return (struct FollowResult) {
                .anchor = a,
                .new_buf = do_submit(buf, a, fi, NULL, option),
            };
        }
        break;

    case FORM_INPUT_IMAGE:
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
        return (struct FollowResult) {
            .anchor = a,
            .new_buf = do_submit(buf, a, fi, NULL, option),
        };

    case FORM_INPUT_RESET:
        for (int i = 0; i < buf->doc->formitem.nanchor; i++) {
            struct Anchor* a2 = &buf->doc->formitem.anchors[i];
            struct FormItemList* f2 = (struct FormItemList*)a2->url;
            if (f2->parent == fi->parent
                && f2->name
                && f2->value
                && f2->type != FORM_INPUT_SUBMIT
                && f2->type != FORM_INPUT_HIDDEN
                && f2->type != FORM_INPUT_RESET) {
                f2->value = f2->init_value;
                f2->checked = f2->init_checked;
                f2->label = f2->init_label;
                f2->selected = f2->init_selected;
                doc_formUpdateBuffer(buf->doc, a2, f2);
            }
        }
        break;

    case FORM_INPUT_HIDDEN:
    default:
        break;
    }

    return (struct FollowResult) { 0 };
}

bool currentBufferSubmit()
{
    if (!Currentbuf->doc) {
        return false;
    }

    struct Anchor* a = Currentbuf->doc->submit;
    if (!a) {
        return false;
    }
    Currentbuf->doc->submit = NULL;
    doc_gotoLine(Currentbuf->doc, a->start.line);
    Currentbuf->doc->pos = a->start.pos;
    struct FollowResult result = _followForm(Currentbuf,
        (struct FollowOption) { .on_target = true, .do_download = false }, true);
    if (result.new_buf) {
        tab_push_buffer(CurrentTab(), result.new_buf);
    }
    return true;
}

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

struct rc_search_table {
    struct param_ptr* param;
    short uniq_pos;
};

static struct rc_search_table* RC_search_table;
static int RC_table_size;

#define CMT_HELPER N_("External Viewer Setup")
#define CMT_TABSTOP N_("Tab width in characters")
#define CMT_INDENT_INCR N_("Indent for HTML rendering")
#define CMT_PIXEL_PER_CHAR N_("Number of pixels per character (4.0...32.0)")
#define CMT_PIXEL_PER_LINE N_("Number of pixels per line (4.0...64.0)")
#define CMT_PAGERLINE N_("Number of remembered lines when used as a pager")
#define CMT_HISTORY N_("Use URL history")
#define CMT_HISTSIZE N_("Number of remembered URL")
#define CMT_SAVEHIST N_("Save URL history")
#define CMT_FRAME N_("Render frames automatically")
#define CMT_ARGV_IS_URL N_("Treat argument without scheme as URL")
#define CMT_TSELF N_("Use _self as default target")
#define CMT_OPEN_TAB_BLANK N_("Open link on new tab if target is _blank or _new")
#define CMT_OPEN_TAB_DL_LIST N_("Open download list panel on new tab")
#define CMT_DISPLINK N_("Display link URL automatically")
#define CMT_DISPLINKNUMBER N_("Display link numbers")
#define CMT_DECODE_URL N_("Display decoded URL")
#define CMT_DISPLINEINFO N_("Display current line number")
#define CMT_DISP_IMAGE N_("Display inline images")
#define CMT_PSEUDO_INLINES N_("Display pseudo-ALTs for inline images with no ALT or TITLE string")
#ifdef USE_IMAGE
#define CMT_AUTO_IMAGE N_("Load inline images automatically")
#define CMT_MAX_LOAD_IMAGE N_("Maximum processes for parallel image loading")
#define CMT_EXT_IMAGE_VIEWER N_("Use external image viewer")
#define CMT_IMAGE_SCALE N_("Scale of image (%)")
#define CMT_IMGDISPLAY N_("External command to display image")
#define CMT_IMAGE_MAP_LIST N_("Use link list of image map")
#define CMT_INLINE_IMG_PROTOCOL N_("Inline image display method")
#endif
#define CMT_MULTICOL N_("Display file names in multi-column format")
#define CMT_ALT_ENTITY N_("Use ASCII equivalents to display entities")
#define CMT_GRAPHIC_CHAR N_("Character type for border of table and menu")
#define CMT_DISP_BORDERS N_("Display table borders, ignore value of BORDER")
#define CMT_DISABLE_CENTER N_("Disable center alignment")
#define CMT_FOLD_TEXTAREA N_("Fold lines in TEXTAREA")
#define CMT_DISP_INS_DEL N_("Display INS, DEL, S and STRIKE element")
#define CMT_COLOR N_("Display with color")
#define CMT_HINTENSITY_COLOR N_("Use high-intensity colors")
#define CMT_B_COLOR N_("Color of normal character")
#define CMT_A_COLOR N_("Color of anchor")
#define CMT_I_COLOR N_("Color of image link")
#define CMT_F_COLOR N_("Color of form")
#define CMT_ACTIVE_STYLE N_("Enable coloring of active link")
#define CMT_C_COLOR N_("Color of currently active link")
#define CMT_VISITED_ANCHOR N_("Use visited link color")
#define CMT_V_COLOR N_("Color of visited link")
#define CMT_BG_COLOR N_("Color of background")
#define CMT_MARK_COLOR N_("Color of mark")
#define CMT_USE_PROXY N_("Use proxy")
#define CMT_HTTP_PROXY N_("URL of HTTP proxy host")
#ifdef USE_SSL
#define CMT_HTTPS_PROXY N_("URL of HTTPS proxy host")
#endif /* USE_SSL */
#ifdef USE_GOPHER
#define CMT_GOPHER_PROXY N_("URL of GOPHER proxy host")
#endif /* USE_GOPHER */
#define CMT_FTP_PROXY N_("URL of FTP proxy host")
#define CMT_NO_PROXY N_("Domains to be accessed directly (no proxy)")
#define CMT_NOPROXY_NETADDR N_("Check noproxy by network address")
#define CMT_NO_CACHE N_("Disable cache")
#ifdef USE_NNTP
#define CMT_NNTP_SERVER N_("News server")
#define CMT_NNTP_MODE N_("Mode of news server")
#define CMT_MAX_NEWS N_("Number of news messages")
#endif
#define CMT_DNS_ORDER N_("Order of name resolution")
#define CMT_DROOT N_("Directory corresponding to / (document root)")
#define CMT_PDROOT N_("Directory corresponding to /~user")
#define CMT_CGIBIN N_("Directory corresponding to /cgi-bin")
#define CMT_TMP N_("Directory for temporary files")
#define CMT_CONFIRM_QQ N_("Confirm when quitting with q")
#define CMT_CLOSE_TAB_BACK N_("Close tab if buffer is last when back")
#ifdef USE_MARK
#define CMT_USE_MARK N_("Enable mark operations")
#endif
#define CMT_EMACS_LIKE_LINEEDIT N_("Enable Emacs-style line editing")
#define CMT_SPACE_AUTOCOMPLETE N_("Space key triggers file completion while editing URLs")
#define CMT_VI_PREC_NUM N_("Enable vi-like numeric prefix")
#define CMT_LABEL_TOPLINE N_("Move cursor to top line when going to label")
#define CMT_NEXTPAGE_TOPLINE N_("Move cursor to top line when moving to next page")
#define CMT_FOLD_LINE N_("Fold lines of plain text file")
#define CMT_SHOW_NUM N_("Show line numbers")
#define CMT_SHOW_SRCH_STR N_("Show search string")
#define CMT_MIMETYPES N_("List of mime.types files")
#define CMT_MAILCAP N_("List of mailcap files")
#define CMT_URIMETHODMAP N_("List of urimethodmap files")
#define CMT_EDITOR N_("Editor")
#define CMT_MAILER N_("Mailer")
#define CMT_MAILTO_OPTIONS N_("How to call Mailer for mailto URLs with options")
#define CMT_EXTBRZ N_("External browser")
#define CMT_EXTBRZ2 N_("2nd external browser")
#define CMT_EXTBRZ3 N_("3rd external browser")
#define CMT_EXTBRZ4 N_("4th external browser")
#define CMT_EXTBRZ5 N_("5th external browser")
#define CMT_EXTBRZ6 N_("6th external browser")
#define CMT_EXTBRZ7 N_("7th external browser")
#define CMT_EXTBRZ8 N_("8th external browser")
#define CMT_EXTBRZ9 N_("9th external browser")
#define CMT_DISABLE_SECRET_SECURITY_CHECK N_("Disable secret file security check")
#define CMT_PASSWDFILE N_("Password file")
#define CMT_PRE_FORM_FILE N_("File for setting form on loading")
#define CMT_SITECONF_FILE N_("File for preferences for each site")
#define CMT_FTPPASS N_("Password for anonymous FTP (your mail address)")
#define CMT_FTPPASS_HOSTNAMEGEN N_("Generate domain part of password for FTP")
#define CMT_USERAGENT N_("User-Agent identification string")
#define CMT_ACCEPTENCODING N_("Accept-Encoding header")
#define CMT_ACCEPTMEDIA N_("Accept header")
#define CMT_ACCEPTLANG N_("Accept-Language header")
#define CMT_MARK_ALL_PAGES N_("Treat URL-like strings as links in all pages")
#define CMT_WRAP N_("Wrap search")
#define CMT_VIEW_UNSEENOBJECTS N_("Display unseen objects (e.g. bgimage tag)")
#define CMT_AUTO_UNCOMPRESS N_("Uncompress compressed data automatically when downloading")
#ifdef __EMX__
#define CMT_BGEXTVIEW N_("Run external viewer in a separate session")
#else
#define CMT_BGEXTVIEW N_("Run external viewer in the background")
#endif
#define CMT_EXT_DIRLIST N_("Use external program for directory listing")
#define CMT_DIRLIST_CMD N_("URL of directory listing command")
#ifdef USE_DICT
#define CMT_USE_DICTCOMMAND N_("Enable dictionary lookup through CGI")
#define CMT_DICTCOMMAND N_("URL of dictionary lookup command")
#endif /* USE_DICT */
#define CMT_IGNORE_NULL_IMG_ALT N_("Display link name for images lacking ALT")
#define CMT_IFILE N_("Index file for directories")
#define CMT_RETRY_HTTP N_("Prepend http:// to URL automatically")
#define CMT_DEFAULT_URL N_("Default value for open-URL command")
#define CMT_DECODE_CTE N_("Decode Content-Transfer-Encoding when saving")
#define CMT_PRESERVE_TIMESTAMP N_("Preserve timestamp when saving")
#ifdef USE_MOUSE
#define CMT_MOUSE N_("Enable mouse")
#define CMT_REVERSE_MOUSE N_("Scroll in reverse direction of mouse drag")
#define CMT_RELATIVE_WHEEL_SCROLL N_("Behavior of wheel scroll speed")
#define CMT_RELATIVE_WHEEL_SCROLL_RATIO N_("(A only)Scroll by # (%) of screen")
#define CMT_FIXED_WHEEL_SCROLL_COUNT N_("(B only)Scroll by # lines")
#endif /* USE_MOUSE */
#define CMT_CLEAR_BUF N_("Free memory of undisplayed buffers")
#define CMT_NOSENDREFERER N_("Suppress `Referer:' header")
#define CMT_CROSSORIGINREFERER N_("Exclude pathname and query string from `Referer:' header when cross domain communication")
#define CMT_IGNORE_CASE N_("Search case-insensitively")
#define CMT_USE_LESSOPEN N_("Use LESSOPEN")
#ifdef USE_SSL
#ifdef USE_SSL_VERIFY
#define CMT_SSL_VERIFY_SERVER N_("Perform SSL server verification")
#define CMT_SSL_CERT_FILE N_("PEM encoded certificate file of client")
#define CMT_SSL_KEY_FILE N_("PEM encoded private key file of client")
#define CMT_SSL_CA_PATH N_("Path to directory for PEM encoded certificates of CAs")
#define CMT_SSL_CA_FILE N_("File consisting of PEM encoded certificates of CAs")
#define CMT_SSL_CA_DEFAULT N_("Use default locations for PEM encoded certificates of CAs")
#endif /* USE_SSL_VERIFY */
#define CMT_SSL_FORBID_METHOD N_("List of forbidden SSL methods (2: SSLv2, 3: SSLv3, t: TLSv1.0, 5: TLSv1.1, 6: TLSv1.2, 7: TLSv1.3)")
#define CMT_SSL_MIN_VERSION N_("Minimum SSL version (all, TLSv1.0, TLSv1.1, TLSv1.2, or TLSv1.3)")
#define CMT_SSL_CIPHER N_("SSL ciphers for TLSv1.2 and below (e.g. DEFAULT:@SECLEVEL=2)")
#endif /* USE_SSL */
#ifdef USE_COOKIE
#define CMT_USECOOKIE N_("Enable cookie processing")
#define CMT_SHOWCOOKIE N_("Print a message when receiving a cookie")
#define CMT_ACCEPTCOOKIE N_("Accept cookies")
#define CMT_ACCEPTBADCOOKIE N_("Action to be taken on invalid cookie")
#define CMT_COOKIE_REJECT_DOMAINS N_("Domains to reject cookies from")
#define CMT_COOKIE_ACCEPT_DOMAINS N_("Domains to accept cookies from")
#define CMT_COOKIE_AVOID_WONG_NUMBER_OF_DOTS N_("Domains to avoid [wrong number of dots]")
#endif
#define CMT_FOLLOW_REDIRECTION N_("Number of redirections to follow")
#define CMT_META_REFRESH N_("Enable processing of meta-refresh tag")
#define CMT_LOCALHOST_ONLY N_("Restrict connections only to localhost")

#ifdef USE_MIGEMO
#define CMT_USE_MIGEMO N_("Enable Migemo (Roma-ji search)")
#define CMT_MIGEMO_COMMAND N_("Migemo command")
#endif /* USE_MIGEMO */

#ifdef USE_M17N
#define CMT_DISPLAY_CHARSET N_("Display charset")
#define CMT_DOCUMENT_CHARSET N_("Default document charset")
#define CMT_AUTO_DETECT N_("Automatic charset detection when loading")
#define CMT_SYSTEM_CHARSET N_("System charset")
#define CMT_FOLLOW_LOCALE N_("System charset follows locale(LC_CTYPE)")
#define CMT_EXT_HALFDUMP N_("Output halfdump with display charset")
#define CMT_USE_WIDE N_("Use multi-column characters")
#define CMT_USE_COMBINING N_("Use combining characters")
#define CMT_EAST_ASIAN_WIDTH N_("Use double width for some Unicode characters")
#define CMT_USE_LANGUAGE_TAG N_("Use Unicode language tags")
#define CMT_UCS_CONV N_("Charset conversion using Unicode map")
#define CMT_PRE_CONV N_("Charset conversion when loading")
#define CMT_SEARCH_CONV N_("Adjust search string for document charset")
#define CMT_FIX_WIDTH_CONV N_("Fix character width when converting")
#define CMT_USE_GB12345_MAP N_("Use GB 12345 Unicode map instead of GB 2312's")
#define CMT_USE_JISX0201 N_("Use JIS X 0201 Roman for ISO-2022-JP")
#define CMT_USE_JISC6226 N_("Use JIS C 6226:1978 for ISO-2022-JP")
#define CMT_USE_JISX0201K N_("Use JIS X 0201 Katakana")
#define CMT_USE_JISX0212 N_("Use JIS X 0212:1990 (Supplemental Kanji)")
#define CMT_USE_JISX0213 N_("Use JIS X 0213:2000 (2000JIS)")
#define CMT_STRICT_ISO2022 N_("Strict ISO-2022-JP/KR/CN")
#define CMT_GB18030_AS_UCS N_("Treat 4 bytes char. of GB18030 as Unicode")
#define CMT_SIMPLE_PRESERVE_SPACE N_("Simple Preserve space")
#endif

#define CMT_KEYMAP_FILE N_("keymap file")

#define _(Text) Text
#define N_(Text) Text
#define gettext(Text) Text

static struct sel_c colorstr[] = {
    { 0, "black", N_("black") },
    { 1, "red", N_("red") },
    { 2, "green", N_("green") },
    { 3, "yellow", N_("yellow") },
    { 4, "blue", N_("blue") },
    { 5, "magenta", N_("magenta") },
    { 6, "cyan", N_("cyan") },
    { 7, "white", N_("white") },
    { 8, "terminal", N_("terminal") },
    { 0, NULL, NULL }
};

#define N_STR(x) #x
#define N_S(x) (x), N_STR(x)

static struct sel_c defaulturls[] = {
    { N_S(DEFAULT_URL_EMPTY), N_("none") },
    { N_S(DEFAULT_URL_CURRENT), N_("current URL") },
    { N_S(DEFAULT_URL_LINK), N_("link URL") },
    { 0, NULL, NULL }
};

static struct sel_c displayinsdel[] = {
    { N_S(DISPLAY_INS_DEL_SIMPLE), N_("simple") },
    { N_S(DISPLAY_INS_DEL_NORMAL), N_("use tag") },
    { N_S(DISPLAY_INS_DEL_FONTIFY), N_("fontify") },
    { 0, NULL, NULL }
};

static struct sel_c dnsorders[] = {
    { N_S(DNS_ORDER_UNSPEC), N_("unspecified") },
    { N_S(DNS_ORDER_INET_INET6), N_("inet inet6") },
    { N_S(DNS_ORDER_INET6_INET), N_("inet6 inet") },
    { N_S(DNS_ORDER_INET_ONLY), N_("inet only") },
    { N_S(DNS_ORDER_INET6_ONLY), N_("inet6 only") },
    { 0, NULL, NULL }
};

static struct sel_c badcookiestr[] = {
    { N_S(ACCEPT_BAD_COOKIE_DISCARD), N_("discard") },
    { N_S(ACCEPT_BAD_COOKIE_ASK), N_("ask") },
    { 0, NULL, NULL }
};

static struct sel_c mailtooptionsstr[] = {
    { N_S(MAILTO_OPTIONS_IGNORE), N_("ignore options and use only the address") },
    { N_S(MAILTO_OPTIONS_USE_MAILTO_URL), N_("use full mailto URL") },
    { 0, NULL, NULL }
};

static wc_ces_list* display_charset_str = NULL;
static wc_ces_list* document_charset_str = NULL;
static wc_ces_list* system_charset_str = NULL;
static struct sel_c auto_detect_str[] = {
    { N_S(WC_OPT_DETECT_OFF), N_("OFF") },
    { N_S(WC_OPT_DETECT_ISO_2022), N_("Only ISO 2022") },
    { N_S(WC_OPT_DETECT_ON), N_("ON") },
    { 0, NULL, NULL }
};

static struct sel_c graphic_char_str[] = {
    { N_S(GRAPHIC_CHAR_ASCII), N_("ASCII") },
    { N_S(GRAPHIC_CHAR_CHARSET), N_("charset specific") },
    { N_S(GRAPHIC_CHAR_DEC), N_("DEC special graphics") },
    { 0, NULL, NULL }
};

static struct sel_c inlineimgstr[] = {
    { N_S(INLINE_IMG_NONE), N_("external command") },
    { N_S(INLINE_IMG_OSC5379), N_("OSC 5379 (mlterm)") },
    { N_S(INLINE_IMG_SIXEL), N_("sixel (img2sixel)") },
    { N_S(INLINE_IMG_ITERM2), N_("OSC 1337 (iTerm2)") },
    { N_S(INLINE_IMG_KITTY), N_("kitty (ImageMagick)") },
    { 0, NULL, NULL }
};

struct param_ptr params1[] = {
    { "tabstop", P_NZINT, PI_TEXT, (void*)&g_runtime.Tabstop, CMT_TABSTOP, NULL },
    { "indent_incr", P_NZINT, PI_TEXT, (void*)&g_runtime.IndentIncr, CMT_INDENT_INCR,
        NULL },
    { "pixel_per_char", P_PIXELS, PI_TEXT, (void*)&g_runtime.pixel_per_char,
        CMT_PIXEL_PER_CHAR, NULL },
    { "pixel_per_line", P_PIXELS, PI_TEXT, (void*)&g_runtime.pixel_per_line,
        CMT_PIXEL_PER_LINE, NULL },
    { "frame", P_CHARINT, PI_ONOFF, (void*)&g_runtime.RenderFrame, CMT_FRAME, NULL },
    { "target_self", P_CHARINT, PI_ONOFF, (void*)&g_runtime.TargetSelf, CMT_TSELF, NULL },
    { "open_tab_blank", P_INT, PI_ONOFF, (void*)&g_runtime.open_tab_blank,
        CMT_OPEN_TAB_BLANK, NULL },
    { "open_tab_dl_list", P_INT, PI_ONOFF, (void*)&g_runtime.open_tab_dl_list,
        CMT_OPEN_TAB_DL_LIST, NULL },
    { "display_link", P_INT, PI_ONOFF, (void*)&g_runtime.displayLink, CMT_DISPLINK,
        NULL },
    { "display_link_number", P_INT, PI_ONOFF, (void*)&g_runtime.displayLinkNumber,
        CMT_DISPLINKNUMBER, NULL },
    { "decode_url", P_INT, PI_ONOFF, (void*)&g_runtime.DecodeURL, CMT_DECODE_URL, NULL },
    { "display_lineinfo", P_INT, PI_ONOFF, (void*)&g_runtime.displayLineInfo,
        CMT_DISPLINEINFO, NULL },
    { "ext_dirlist", P_INT, PI_ONOFF, (void*)&g_runtime.UseExternalDirBuffer,
        CMT_EXT_DIRLIST, NULL },
    { "dirlist_cmd", P_STRING, PI_TEXT, (void*)&g_runtime.DirBufferCommand,
        CMT_DIRLIST_CMD, NULL },
    { "use_dictcommand", P_INT, PI_ONOFF, (void*)&g_runtime.UseDictCommand,
        CMT_USE_DICTCOMMAND, NULL },
    { "dictcommand", P_STRING, PI_TEXT, (void*)&g_runtime.DictCommand,
        CMT_DICTCOMMAND, NULL },
    { "multicol", P_INT, PI_ONOFF, (void*)&g_runtime.multicolList, CMT_MULTICOL, NULL },
    { "alt_entity", P_CHARINT, PI_ONOFF, (void*)&g_runtime.UseAltEntity, CMT_ALT_ENTITY,
        NULL },
    { "graphic_char", P_CHARINT, PI_SEL_C, &UseGraphicChar,
        CMT_GRAPHIC_CHAR, (void*)graphic_char_str },
    { "display_borders", P_CHARINT, PI_ONOFF, (void*)&g_runtime.DisplayBorders,
        CMT_DISP_BORDERS, NULL },
    { "disable_center", P_CHARINT, PI_ONOFF, (void*)&g_runtime.DisableCenter,
        CMT_DISABLE_CENTER, NULL },
    { "fold_textarea", P_CHARINT, PI_ONOFF, (void*)&g_runtime.FoldTextarea,
        CMT_FOLD_TEXTAREA, NULL },
    { "display_ins_del", P_INT, PI_SEL_C, (void*)&g_runtime.displayInsDel,
        CMT_DISP_INS_DEL, displayinsdel },
    { "ignore_null_img_alt", P_INT, PI_ONOFF, (void*)&g_runtime.ignore_null_img_alt,
        CMT_IGNORE_NULL_IMG_ALT, NULL },
    { "view_unseenobject", P_INT, PI_ONOFF, (void*)&g_runtime.view_unseenobject,
        CMT_VIEW_UNSEENOBJECTS, NULL },
    /* XXX: emacs-w3m force to off display_image even if image options off */
    { "display_image", P_INT, PI_ONOFF, (void*)&g_runtime.displayImage, CMT_DISP_IMAGE,
        NULL },
    { "pseudo_inlines", P_INT, PI_ONOFF, (void*)&g_runtime.pseudoInlines,
        CMT_PSEUDO_INLINES, NULL },
    { "auto_image", P_INT, PI_ONOFF, (void*)&g_runtime.autoImage, CMT_AUTO_IMAGE, NULL },
    { "max_load_image", P_INT, PI_TEXT, (void*)&g_runtime.maxLoadImage,
        CMT_MAX_LOAD_IMAGE, NULL },
    { "ext_image_viewer", P_INT, PI_ONOFF, (void*)&g_runtime.useExtImageViewer,
        CMT_EXT_IMAGE_VIEWER, NULL },
    { "image_scale", P_SCALE, PI_TEXT, (void*)&g_runtime.image_scale, CMT_IMAGE_SCALE,
        NULL },
    { "inline_img_protocol", P_INT, PI_SEL_C, (void*)&g_runtime.enable_inline_image,
        CMT_INLINE_IMG_PROTOCOL, (void*)inlineimgstr },
    { "imgdisplay", P_STRING, PI_TEXT, (void*)&g_runtime.Imgdisplay, CMT_IMGDISPLAY,
        NULL },
    { "image_map_list", P_INT, PI_ONOFF, (void*)&g_runtime.image_map_list,
        CMT_IMAGE_MAP_LIST, NULL },
    { "fold_line", P_INT, PI_ONOFF, (void*)&g_runtime.FoldLine, CMT_FOLD_LINE, NULL },
    { "show_lnum", P_INT, PI_ONOFF, (void*)&g_runtime.showLineNum, CMT_SHOW_NUM, NULL },
    { "show_srch_str", P_INT, PI_ONOFF, (void*)&g_runtime.show_srch_str,
        CMT_SHOW_SRCH_STR, NULL },
    { "label_topline", P_INT, PI_ONOFF, (void*)&g_runtime.label_topline,
        CMT_LABEL_TOPLINE, NULL },
    { "nextpage_topline", P_INT, PI_ONOFF, (void*)&g_runtime.nextpage_topline,
        CMT_NEXTPAGE_TOPLINE, NULL },
    { NULL, 0, 0, NULL, NULL, NULL },
};

struct param_ptr params2[] = {
    { "color", P_INT, PI_ONOFF, (void*)&g_runtime.useColor, CMT_COLOR, NULL },
    { "high-intensity", P_INT, PI_ONOFF, (void*)&g_runtime.highIntensityColors, CMT_HINTENSITY_COLOR, NULL },
    { "basic_color", P_COLOR, PI_SEL_C, (void*)&g_runtime.basic_color, CMT_B_COLOR,
        (void*)colorstr },
    { "anchor_color", P_COLOR, PI_SEL_C, (void*)&g_runtime.anchor_color, CMT_A_COLOR,
        (void*)colorstr },
    { "image_color", P_COLOR, PI_SEL_C, (void*)&g_runtime.image_color, CMT_I_COLOR,
        (void*)colorstr },
    { "form_color", P_COLOR, PI_SEL_C, (void*)&g_runtime.form_color, CMT_F_COLOR,
        (void*)colorstr },
    { "mark_color", P_COLOR, PI_SEL_C, (void*)&g_runtime.mark_color, CMT_MARK_COLOR,
        (void*)colorstr },
    { "bg_color", P_COLOR, PI_SEL_C, (void*)&g_runtime.bg_color, CMT_BG_COLOR,
        (void*)colorstr },
    { "active_style", P_INT, PI_ONOFF, (void*)&g_runtime.useActiveColor,
        CMT_ACTIVE_STYLE, NULL },
    { "active_color", P_COLOR, PI_SEL_C, (void*)&g_runtime.active_color, CMT_C_COLOR,
        (void*)colorstr },
    { "visited_anchor", P_INT, PI_ONOFF, (void*)&g_runtime.useVisitedColor,
        CMT_VISITED_ANCHOR, NULL },
    { "visited_color", P_COLOR, PI_SEL_C, (void*)&g_runtime.visited_color, CMT_V_COLOR,
        (void*)colorstr },
    { NULL, 0, 0, NULL, NULL, NULL },
};

struct param_ptr params3[] = {
    { "pagerline", P_NZINT, PI_TEXT, (void*)&g_runtime.PagerMax, CMT_PAGERLINE, NULL },
    { "use_history", P_INT, PI_ONOFF, (void*)&g_runtime.UseHistory, CMT_HISTORY, NULL },
    { "history", P_INT, PI_TEXT, (void*)&g_runtime.URLHistSize, CMT_HISTSIZE, NULL },
    { "save_hist", P_INT, PI_ONOFF, (void*)&g_runtime.SaveURLHist, CMT_SAVEHIST, NULL },
    { "confirm_qq", P_INT, PI_ONOFF, (void*)&g_runtime.confirm_on_quit, CMT_CONFIRM_QQ,
        NULL },
    { "close_tab_back", P_INT, PI_ONOFF, (void*)&g_runtime.close_tab_back,
        CMT_CLOSE_TAB_BACK, NULL },
    { "mark", P_INT, PI_ONOFF, (void*)&g_runtime.use_mark, CMT_USE_MARK, NULL },
    { "emacs_like_lineedit", P_INT, PI_ONOFF, (void*)&g_runtime.emacs_like_lineedit,
        CMT_EMACS_LIKE_LINEEDIT, NULL },
    { "space_autocomplete", P_INT, PI_ONOFF, (void*)&g_runtime.space_autocomplete,
        CMT_SPACE_AUTOCOMPLETE, NULL },
    { "vi_prec_num", P_INT, PI_ONOFF, (void*)&g_runtime.vi_prec_num, CMT_VI_PREC_NUM,
        NULL },
    { "mark_all_pages", P_INT, PI_ONOFF, (void*)&g_runtime.MarkAllPages,
        CMT_MARK_ALL_PAGES, NULL },
    { "wrap_search", P_INT, PI_ONOFF, (void*)&g_runtime.WrapDefault, CMT_WRAP, NULL },
    { "ignorecase_search", P_INT, PI_ONOFF, (void*)&g_runtime.IgnoreCase,
        CMT_IGNORE_CASE, NULL },
#ifdef USE_MIGEMO
    { "use_migemo", P_INT, PI_ONOFF, (void*)&use_migemo, CMT_USE_MIGEMO,
        NULL },
    { "migemo_command", P_STRING, PI_TEXT, (void*)&migemo_command,
        CMT_MIGEMO_COMMAND, NULL },
#endif /* USE_MIGEMO */
    { "clear_buffer", P_INT, PI_ONOFF, (void*)&g_runtime.clear_buffer, CMT_CLEAR_BUF,
        NULL },
    { "auto_uncompress", P_CHARINT, PI_ONOFF, (void*)&g_runtime.AutoUncompress,
        CMT_AUTO_UNCOMPRESS, NULL },
    { "preserve_timestamp", P_CHARINT, PI_ONOFF, (void*)&g_runtime.PreserveTimestamp,
        CMT_PRESERVE_TIMESTAMP, NULL },
    { "keymap_file", P_STRING, PI_TEXT, (void*)&g_runtime.keymap_file, CMT_KEYMAP_FILE,
        NULL },
    { NULL, 0, 0, NULL, NULL, NULL },
};

struct param_ptr params4[] = {
    { "use_proxy", P_CHARINT, PI_ONOFF, (void*)&g_runtime.use_proxy, CMT_USE_PROXY,
        NULL },
    { "http_proxy", P_STRING, PI_TEXT, (void*)&g_runtime.HTTP_proxy, CMT_HTTP_PROXY,
        NULL },
    { "https_proxy", P_STRING, PI_TEXT, (void*)&g_runtime.HTTPS_proxy, CMT_HTTPS_PROXY,
        NULL },
    { "ftp_proxy", P_STRING, PI_TEXT, (void*)&g_runtime.FTP_proxy, CMT_FTP_PROXY, NULL },
    { "no_proxy", P_STRING, PI_TEXT, (void*)&g_runtime.NO_proxy, CMT_NO_PROXY, NULL },
    { "noproxy_netaddr", P_INT, PI_ONOFF, (void*)&g_runtime.NOproxy_netaddr,
        CMT_NOPROXY_NETADDR, NULL },
    { "no_cache", P_CHARINT, PI_ONOFF, (void*)&g_runtime.NoCache, CMT_NO_CACHE, NULL },

    { NULL, 0, 0, NULL, NULL, NULL },
};

struct param_ptr params5[] = {
    { "document_root", P_STRING, PI_TEXT, (void*)&g_runtime.document_root, CMT_DROOT,
        NULL },
    { "personal_document_root", P_STRING, PI_TEXT,
        (void*)&g_runtime.personal_document_root, CMT_PDROOT, NULL },
    { "cgi_bin", P_STRING, PI_TEXT, (void*)&g_runtime.cgi_bin, CMT_CGIBIN, NULL },
    { "index_file", P_STRING, PI_TEXT, (void*)&g_runtime.index_file, CMT_IFILE, NULL },
    { "tmp_dir", P_STRING, PI_TEXT, (void*)&g_runtime.param_tmp_dir, CMT_TMP, NULL },
    { NULL, 0, 0, NULL, NULL, NULL },
};

struct param_ptr params6[] = {
    { "mime_types", P_STRING, PI_TEXT, (void*)&g_runtime.mimetypes_files, CMT_MIMETYPES,
        NULL },
    { "mailcap", P_STRING, PI_TEXT, (void*)&g_runtime.mailcap_files, CMT_MAILCAP, NULL },
    { "urimethodmap", P_STRING, PI_TEXT, (void*)&g_runtime.urimethodmap_files,
        CMT_URIMETHODMAP, NULL },
    { "editor", P_STRING, PI_TEXT, (void*)&g_runtime.Editor, CMT_EDITOR, NULL },
    { "mailto_options", P_INT, PI_SEL_C, (void*)&g_runtime.MailtoOptions,
        CMT_MAILTO_OPTIONS, (void*)mailtooptionsstr },
    { "mailer", P_STRING, PI_TEXT, (void*)&g_runtime.Mailer, CMT_MAILER, NULL },
    { "extbrowser", P_STRING, PI_TEXT, (void*)&g_runtime.ExtBrowser, CMT_EXTBRZ, NULL },
    { "extbrowser2", P_STRING, PI_TEXT, (void*)&g_runtime.ExtBrowser2, CMT_EXTBRZ2,
        NULL },
    { "extbrowser3", P_STRING, PI_TEXT, (void*)&g_runtime.ExtBrowser3, CMT_EXTBRZ3,
        NULL },
    { "extbrowser4", P_STRING, PI_TEXT, (void*)&g_runtime.ExtBrowser4, CMT_EXTBRZ4,
        NULL },
    { "extbrowser5", P_STRING, PI_TEXT, (void*)&g_runtime.ExtBrowser5, CMT_EXTBRZ5,
        NULL },
    { "extbrowser6", P_STRING, PI_TEXT, (void*)&g_runtime.ExtBrowser6, CMT_EXTBRZ6,
        NULL },
    { "extbrowser7", P_STRING, PI_TEXT, (void*)&g_runtime.ExtBrowser7, CMT_EXTBRZ7,
        NULL },
    { "extbrowser8", P_STRING, PI_TEXT, (void*)&g_runtime.ExtBrowser8, CMT_EXTBRZ8,
        NULL },
    { "extbrowser9", P_STRING, PI_TEXT, (void*)&g_runtime.ExtBrowser9, CMT_EXTBRZ9,
        NULL },
    { "bgextviewer", P_INT, PI_ONOFF, (void*)&g_runtime.BackgroundExtViewer,
        CMT_BGEXTVIEW, NULL },
    { NULL, 0, 0, NULL, NULL, NULL },
};

struct param_ptr params7[] = {
    { "ssl_forbid_method", P_STRING, PI_TEXT, (void*)&g_runtime.ssl_forbid_method,
        CMT_SSL_FORBID_METHOD, NULL },
    { "ssl_min_version", P_STRING, PI_TEXT, (void*)&g_runtime.ssl_min_version,
        CMT_SSL_MIN_VERSION, NULL },
    { "ssl_cipher", P_STRING, PI_TEXT, (void*)&g_runtime.ssl_cipher, CMT_SSL_CIPHER,
        NULL },
    { "ssl_verify_server", P_INT, PI_ONOFF, (void*)&g_runtime.ssl_verify_server,
        CMT_SSL_VERIFY_SERVER, NULL },
    { "ssl_cert_file", P_SSLPATH, PI_TEXT, (void*)&g_runtime.ssl_cert_file,
        CMT_SSL_CERT_FILE, NULL },
    { "ssl_key_file", P_SSLPATH, PI_TEXT, (void*)&g_runtime.ssl_key_file,
        CMT_SSL_KEY_FILE, NULL },
    { "ssl_ca_path", P_SSLPATH, PI_TEXT, (void*)&g_runtime.ssl_ca_path, CMT_SSL_CA_PATH,
        NULL },
    { "ssl_ca_file", P_SSLPATH, PI_TEXT, (void*)&g_runtime.ssl_ca_file, CMT_SSL_CA_FILE,
        NULL },
    { "ssl_ca_default", P_INT, PI_ONOFF, (void*)&g_runtime.ssl_ca_default,
        CMT_SSL_CA_DEFAULT, NULL },
    { NULL, 0, 0, NULL, NULL, NULL },
};

struct param_ptr params8[] = {
    { "use_cookie", P_INT, PI_ONOFF, (void*)&g_runtime.use_cookie, CMT_USECOOKIE, NULL },
    { "show_cookie", P_INT, PI_ONOFF, (void*)&g_runtime.show_cookie,
        CMT_SHOWCOOKIE, NULL },
    { "accept_cookie", P_INT, PI_ONOFF, (void*)&g_runtime.accept_cookie,
        CMT_ACCEPTCOOKIE, NULL },
    { "accept_bad_cookie", P_INT, PI_SEL_C, (void*)&g_runtime.accept_bad_cookie,
        CMT_ACCEPTBADCOOKIE, (void*)badcookiestr },
    { "cookie_reject_domains", P_STRING, PI_TEXT,
        (void*)&g_runtime.cookie_reject_domains, CMT_COOKIE_REJECT_DOMAINS, NULL },
    { "cookie_accept_domains", P_STRING, PI_TEXT,
        (void*)&g_runtime.cookie_accept_domains, CMT_COOKIE_ACCEPT_DOMAINS, NULL },
    { "cookie_avoid_wrong_number_of_dots", P_STRING, PI_TEXT,
        (void*)&g_runtime.cookie_avoid_wrong_number_of_dots,
        CMT_COOKIE_AVOID_WONG_NUMBER_OF_DOTS, NULL },
    { NULL, 0, 0, NULL, NULL, NULL },
};

struct param_ptr params9[] = {
    { "passwd_file", P_STRING, PI_TEXT, (void*)&g_runtime.passwd_file, CMT_PASSWDFILE,
        NULL },
    { "disable_secret_security_check", P_INT, PI_ONOFF,
        (void*)&g_runtime.disable_secret_security_check, CMT_DISABLE_SECRET_SECURITY_CHECK,
        NULL },
    { "ftppasswd", P_STRING, PI_TEXT, (void*)&g_runtime.ftppasswd, CMT_FTPPASS, NULL },
    { "ftppass_hostnamegen", P_INT, PI_ONOFF, (void*)&g_runtime.ftppass_hostnamegen,
        CMT_FTPPASS_HOSTNAMEGEN, NULL },
    { "pre_form_file", P_STRING, PI_TEXT, (void*)&g_runtime.pre_form_file,
        CMT_PRE_FORM_FILE, NULL },
    { "siteconf_file", P_STRING, PI_TEXT, (void*)&siteconf_file,
        CMT_SITECONF_FILE, NULL },
    { "user_agent", P_STRING, PI_TEXT, (void*)&g_runtime.UserAgent, CMT_USERAGENT, NULL },
    { "no_referer", P_INT, PI_ONOFF, (void*)&g_runtime.NoSendReferer, CMT_NOSENDREFERER,
        NULL },
    { "cross_origin_referer", P_INT, PI_ONOFF, (void*)&g_runtime.CrossOriginReferer,
        CMT_CROSSORIGINREFERER, NULL },
    { "accept_language", P_STRING, PI_TEXT, (void*)&g_runtime.AcceptLang, CMT_ACCEPTLANG,
        NULL },
    { "accept_encoding", P_STRING, PI_TEXT, (void*)&g_runtime.AcceptEncoding,
        CMT_ACCEPTENCODING,
        NULL },
    { "accept_media", P_STRING, PI_TEXT, (void*)&g_runtime.AcceptMedia, CMT_ACCEPTMEDIA,
        NULL },
    { "argv_is_url", P_CHARINT, PI_ONOFF, (void*)&g_runtime.ArgvIsURL, CMT_ARGV_IS_URL,
        NULL },
    { "retry_http", P_INT, PI_ONOFF, (void*)&g_runtime.retryAsHttp, CMT_RETRY_HTTP,
        NULL },
    { "default_url", P_INT, PI_SEL_C, (void*)&g_runtime.DefaultURLString,
        CMT_DEFAULT_URL, (void*)defaulturls },
    { "follow_redirection", P_INT, PI_TEXT, &g_runtime.FollowRedirection,
        CMT_FOLLOW_REDIRECTION, NULL },
    { "meta_refresh", P_CHARINT, PI_ONOFF, (void*)&g_runtime.MetaRefresh,
        CMT_META_REFRESH, NULL },
    { "localhost_only", P_CHARINT, PI_ONOFF, (void*)&g_runtime.LocalhostOnly,
        CMT_LOCALHOST_ONLY, NULL },
    { "dns_order", P_INT, PI_SEL_C, (void*)&g_runtime.DNS_order, CMT_DNS_ORDER,
        (void*)dnsorders },
    { NULL, 0, 0, NULL, NULL, NULL },
};

struct param_ptr params10[] = {
    { "display_charset", P_CODE, PI_CODE, (void*)&g_runtime.DisplayCharset,
        CMT_DISPLAY_CHARSET, (void*)&display_charset_str },
    { "document_charset", P_CODE, PI_CODE, (void*)&g_runtime.DocumentCharset,
        CMT_DOCUMENT_CHARSET, (void*)&document_charset_str },
    { "auto_detect", P_CHARINT, PI_SEL_C, (void*)&WcOption.auto_detect,
        CMT_AUTO_DETECT, (void*)auto_detect_str },
    { "system_charset", P_CODE, PI_CODE, (void*)&g_runtime.SystemCharset,
        CMT_SYSTEM_CHARSET, (void*)&system_charset_str },
    { "follow_locale", P_CHARINT, PI_ONOFF, (void*)&g_runtime.FollowLocale,
        CMT_FOLLOW_LOCALE, NULL },
    { "use_wide", P_CHARINT, PI_ONOFF, (void*)&WcOption.use_wide, CMT_USE_WIDE,
        NULL },
    { "use_combining", P_CHARINT, PI_ONOFF, (void*)&WcOption.use_combining,
        CMT_USE_COMBINING, NULL },
    { "east_asian_width", P_CHARINT, PI_ONOFF,
        (void*)&WcOption.east_asian_width, CMT_EAST_ASIAN_WIDTH, NULL },
    { "use_language_tag", P_CHARINT, PI_ONOFF,
        (void*)&WcOption.use_language_tag, CMT_USE_LANGUAGE_TAG, NULL },
    { "ucs_conv", P_CHARINT, PI_ONOFF, (void*)&WcOption.ucs_conv, CMT_UCS_CONV,
        NULL },
    { "pre_conv", P_CHARINT, PI_ONOFF, (void*)&WcOption.pre_conv, CMT_PRE_CONV,
        NULL },
    { "search_conv", P_CHARINT, PI_ONOFF, (void*)&g_runtime.SearchConv, CMT_SEARCH_CONV,
        NULL },
    { "fix_width_conv", P_CHARINT, PI_ONOFF, (void*)&WcOption.fix_width_conv,
        CMT_FIX_WIDTH_CONV, NULL },
    { "use_gb12345_map", P_CHARINT, PI_ONOFF, (void*)&WcOption.use_gb12345_map,
        CMT_USE_GB12345_MAP, NULL },
    { "use_jisx0201", P_CHARINT, PI_ONOFF, (void*)&WcOption.use_jisx0201,
        CMT_USE_JISX0201, NULL },
    { "use_jisc6226", P_CHARINT, PI_ONOFF, (void*)&WcOption.use_jisc6226,
        CMT_USE_JISC6226, NULL },
    { "use_jisx0201k", P_CHARINT, PI_ONOFF, (void*)&WcOption.use_jisx0201k,
        CMT_USE_JISX0201K, NULL },
    { "use_jisx0212", P_CHARINT, PI_ONOFF, (void*)&WcOption.use_jisx0212,
        CMT_USE_JISX0212, NULL },
    { "use_jisx0213", P_CHARINT, PI_ONOFF, (void*)&WcOption.use_jisx0213,
        CMT_USE_JISX0213, NULL },
    { "strict_iso2022", P_CHARINT, PI_ONOFF, (void*)&WcOption.strict_iso2022,
        CMT_STRICT_ISO2022, NULL },
    { "gb18030_as_ucs", P_CHARINT, PI_ONOFF, (void*)&WcOption.gb18030_as_ucs,
        CMT_GB18030_AS_UCS, NULL },
    { "simple_preserve_space", P_CHARINT, PI_ONOFF, (void*)&g_runtime.SimplePreserveSpace,
        CMT_SIMPLE_PRESERVE_SPACE, NULL },
    { NULL, 0, 0, NULL, NULL, NULL },
};

struct param_section sections[] = {
    { N_("Display Settings"), params1 },
#ifdef USE_COLOR
    { N_("Color Settings"), params2 },
#endif /* USE_COLOR */
    { N_("Miscellaneous Settings"), params3 },
    { N_("Directory Settings"), params5 },
    { N_("External Program Settings"), params6 },
    { N_("Network Settings"), params9 },
    { N_("Proxy Settings"), params4 },
#ifdef USE_SSL
    { N_("SSL Settings"), params7 },
#endif
#ifdef USE_COOKIE
    { N_("Cookie Settings"), params8 },
#endif
#ifdef USE_M17N
    { N_("Charset Settings"), params10 },
#endif
    { NULL, NULL }
};

static Str to_str(struct param_ptr* p);

static int
compare_table(struct rc_search_table* a, struct rc_search_table* b)
{
    return strcmp(a->param->name, b->param->name);
}

static void
create_option_search_table()
{
    int i, j, k;
    int diff1, diff2;
    const char *p, *q;

    /* count table size */
    RC_table_size = 0;
    for (j = 0; sections[j].name != NULL; j++) {
        i = 0;
        while (sections[j].params[i].name) {
            i++;
            RC_table_size++;
        }
    }

    RC_search_table = New_N(struct rc_search_table, RC_table_size);
    k = 0;
    for (j = 0; sections[j].name != NULL; j++) {
        i = 0;
        while (sections[j].params[i].name) {
            RC_search_table[k].param = &sections[j].params[i];
            k++;
            i++;
        }
    }

    qsort(RC_search_table, RC_table_size, sizeof(struct rc_search_table),
        (int (*)(const void*, const void*))compare_table);

    diff2 = 0;
    for (i = 0; i < RC_table_size - 1; i++) {
        p = RC_search_table[i].param->name;
        q = RC_search_table[i + 1].param->name;
        for (j = 0; p[j] != '\0' && q[j] != '\0' && p[j] == q[j]; j++)
            ;
        diff1 = j;
        if (diff1 > diff2)
            RC_search_table[i].uniq_pos = diff1 + 1;
        else
            RC_search_table[i].uniq_pos = diff2 + 1;
        diff2 = diff1;
    }
}

static struct param_ptr*
search_param(const char* name)
{
    size_t b, e, i;
    int cmp;
    int len = strlen(name);

    for (b = 0, e = RC_table_size - 1; b <= e;) {
        i = (b + e) / 2;
        cmp = strncmp(name, RC_search_table[i].param->name, len);

        if (!cmp) {
            if (len >= RC_search_table[i].uniq_pos) {
                return RC_search_table[i].param;
            } else {
                while ((cmp = strcmp(name, RC_search_table[i].param->name)) <= 0)
                    if (!cmp)
                        return RC_search_table[i].param;
                    else if (i == 0)
                        return NULL;
                    else
                        i--;
                /* ambiguous */
                return NULL;
            }
        } else if (cmp < 0) {
            if (i == 0)
                return NULL;
            e = i - 1;
        } else
            b = i + 1;
    }
    return NULL;
}

/* show parameter with bad options invokation */
void show_params(FILE* fp)
{
    int i, j, l;
    const char* t = "";
    const char* cmt;

    g_runtime.OptionCharset = g_runtime.SystemCharset; /* FIXME */

    fputs("\nconfiguration parameters\n", fp);
    for (j = 0; sections[j].name != NULL; j++) {
        if (!g_runtime.OptionEncode)
            cmt = wc_conv(_(sections[j].name), g_runtime.OptionCharset,
                g_runtime.InnerCharset)
                      ->ptr;
        else
            cmt = sections[j].name;
        fprintf(fp, "  section[%d]: %s\n", j, conv_to_system(cmt));
        i = 0;
        while (sections[j].params[i].name) {
            switch (sections[j].params[i].type) {
            case P_INT:
            case P_SHORT:
            case P_CHARINT:
            case P_NZINT:
                t = (sections[j].params[i].inputtype == PI_ONOFF) ? "bool" : "number";
                break;
            case P_CHAR:
                t = "char";
                break;
            case P_STRING:
                t = "string";
                break;

            case P_SSLPATH:
                t = "path";
                break;

            case P_COLOR:
                t = "color";
                break;

            case P_CODE:
                t = "charset";
                break;

            case P_PIXELS:
                t = "number";
                break;
            case P_SCALE:
                t = "percent";
                break;
            }

            if (!g_runtime.OptionEncode)
                cmt = wc_conv(_(sections[j].params[i].comment),
                    g_runtime.OptionCharset, g_runtime.InnerCharset)
                          ->ptr;
            else

                cmt = sections[j].params[i].comment;
            l = 30 - (strlen(sections[j].params[i].name) + strlen(t));
            if (l < 0)
                l = 1;
            fprintf(fp, "    -o %s=<%s>%*s%s\n",
                sections[j].params[i].name, t, l, " ",
                conv_to_system(cmt));
            i++;
        }
    }
}

static int
str_to_color(const char* value)
{
    if (value == NULL)
        return 8; /* terminal */
    switch (TOLOWER(*value)) {
    case '0':
        return 0; /* black */
    case '1':
    case 'r':
        return 1; /* red */
    case '2':
    case 'g':
        return 2; /* green */
    case '3':
    case 'y':
        return 3; /* yellow */
    case '4':
        return 4; /* blue */
    case '5':
    case 'm':
        return 5; /* magenta */
    case '6':
    case 'c':
        return 6; /* cyan */
    case '7':
    case 'w':
        return 7; /* white */
    case '8':
    case 't':
        return 8; /* terminal */
    case 'b':
        if (!strncasecmp(value, "blu", 3))
            return 4; /* blue */
        else
            return 0; /* black */
    }
    return 8; /* terminal */
}

static int
set_param(const char* name, const char* value)
{
    struct param_ptr* p;
    double ppc;

    if (value == NULL)
        return 0;
    p = search_param(name);
    if (p == NULL)
        return 0;
    switch (p->type) {
    case P_INT:
        if (atoi(value) >= 0)
            *(int*)p->varptr = (p->inputtype == PI_ONOFF)
                ? str_to_bool(value, *(int*)p->varptr)
                : atoi(value);
        break;
    case P_NZINT:
        if (atoi(value) > 0)
            *(int*)p->varptr = atoi(value);
        break;
    case P_SHORT:
        *(short*)p->varptr = (p->inputtype == PI_ONOFF)
            ? str_to_bool(value, *(short*)p->varptr)
            : atoi(value);
        break;
    case P_CHARINT:
        *(char*)p->varptr = (p->inputtype == PI_ONOFF)
            ? str_to_bool(value, *(char*)p->varptr)
            : atoi(value);
        break;
    case P_CHAR:
        *(char*)p->varptr = value[0];
        break;
    case P_STRING:
        *(const char**)p->varptr = value;
        break;

    case P_SSLPATH:
        if (value != NULL && value[0] != '\0')
            *(char**)p->varptr = rcFile(value);
        else
            *(char**)p->varptr = NULL;
        g_runtime.ssl_path_modified = 1;
        break;

    case P_COLOR:
        *(int*)p->varptr = str_to_color(value);
        break;

    case P_CODE:
        *(enum wc_ces*)p->varptr = wc_guess_charset_short(value, *(enum wc_ces*)p->varptr);
        break;

    case P_PIXELS:
        ppc = atof(value);
        if (ppc >= MINIMUM_PIXEL_PER_CHAR && ppc <= MAXIMUM_PIXEL_PER_CHAR * 2)
            *(double*)p->varptr = ppc;
        break;
    case P_SCALE:
        ppc = atof(value);
        if (ppc >= 10 && ppc <= 1000)
            *(double*)p->varptr = ppc;
        break;
    }
    return 1;
}

int set_param_option(const char* option)
{
    Str tmp = Strnew();
    const char *p = option, *q;

    while (*p && !IS_SPACE(*p) && *p != '=')
        Strcat_char(tmp, *p++);
    while (*p && IS_SPACE(*p))
        p++;
    if (*p == '=') {
        p++;
        while (*p && IS_SPACE(*p))
            p++;
    }
    Strlower(tmp);
    if (set_param(tmp->ptr, p))
        goto option_assigned;
    q = tmp->ptr;
    if (!strncmp(q, "no", 2)) { /* -o noxxx, -o no-xxx, -o no_xxx */
        q += 2;
        if (*q == '-' || *q == '_')
            q++;
    } else if (tmp->ptr[0] == '-') /* -o -xxx */
        q++;
    else
        return 0;
    if (set_param(q, "0"))
        goto option_assigned;
    return 0;
option_assigned:
    return 1;
}

char* get_param_option(const char* name)
{
    struct param_ptr* p;

    p = search_param(name);
    return p ? to_str(p)->ptr : NULL;
}

static void
interpret_rc(FILE* f)
{
    Str line;
    Str tmp;
    char* p;

    for (;;) {
        line = Strfgets(f);
        if (line->length == 0) /* end of file */
            break;
        Strchop(line);
        if (line->length == 0) /* blank line */
            continue;
        Strremovefirstspaces(line);
        if (line->ptr[0] == '#') /* comment */
            continue;
        tmp = Strnew();
        p = line->ptr;
        while (*p && !IS_SPACE(*p))
            Strcat_char(tmp, *p++);
        while (*p && IS_SPACE(*p))
            p++;
        Strlower(tmp);
        set_param(tmp->ptr, p);
    }
}

#ifdef __MINGW32_VERSION
#define do_mkdir(dir, mode) mkdir(dir)
#else
#define do_mkdir(dir, mode) mkdir(dir, mode)
#endif /* not __MINW32_VERSION */

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
        g_runtime.AcceptLang = _("en;q=1.0");
    }
    if (g_runtime.AcceptEncoding == NULL || *g_runtime.AcceptEncoding == '\0')
        g_runtime.AcceptEncoding = acceptableEncoding();
    if (g_runtime.AcceptMedia == NULL || *g_runtime.AcceptMedia == '\0')
        g_runtime.AcceptMedia = acceptableMimeTypes();

    update_utf8_symbol();

    wtf_init(g_runtime.DocumentCharset, g_runtime.DisplayCharset);

    if (fmInitialized()) {
        initKeymap(FALSE);
        initMenu();
    }
}

void init_rc(void)
{
    g_runtime.LoadHist = newHist();
    g_runtime.SaveHist = newHist();
    g_runtime.ShellHist = newHist();
    g_runtime.TextHist = newHist();
    g_runtime.URLHist = newHist();
    if (g_runtime.UseHistory)
        loadHistory(g_runtime.URLHist);

    int i;
    FILE* f;

    if (g_runtime.rc_dir != NULL)
        goto open_rc;

    g_runtime.rc_dir = allocStr(getenv("W3M_DIR"), -1);
    if (g_runtime.rc_dir == NULL || *g_runtime.rc_dir == '\0')
        g_runtime.rc_dir = allocStr(RC_DIR, -1);
    if (g_runtime.rc_dir == NULL || *g_runtime.rc_dir == '\0')
        goto rc_dir_err;
    g_runtime.rc_dir = expandPath(g_runtime.rc_dir);

    i = strlen(g_runtime.rc_dir);
    if (i > 1 && g_runtime.rc_dir[i - 1] == '/')
        g_runtime.rc_dir[i - 1] = '\0';

    display_charset_str = wc_get_ces_list();
    document_charset_str = display_charset_str;
    system_charset_str = display_charset_str;

    getRuntime()->tmp_dir = g_runtime.rc_dir;

    if (do_recursive_mkdir(g_runtime.rc_dir) == -1)
        goto rc_dir_err;

    g_runtime.no_rc_dir = FALSE;

    if (g_runtime.config_file == NULL)
        g_runtime.config_file = rcFile(CONFIG_FILE);

    create_option_search_table();

open_rc:
    /* open config file */
    if ((f = fopen(etcFile(W3MCONFIG), "rt")) != NULL) {
        interpret_rc(f);
        fclose(f);
    }
    if ((f = fopen(confFile(CONFIG_FILE), "rt")) != NULL) {
        interpret_rc(f);
        fclose(f);
    }
    if (g_runtime.config_file && (f = fopen(g_runtime.config_file, "rt")) != NULL) {
        interpret_rc(f);
        fclose(f);
    }
    return;

rc_dir_err:
    g_runtime.no_rc_dir = TRUE;
    create_option_search_table();
    goto open_rc;
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

static char optionpanel_src1[] = "<html><head><title>Option Setting Panel</title></head><body>\
<h1 align=center>Option Setting Panel<br>(w3m version %s)</b></h1>\
<form method=post action=\"file:///$LIB/" W3MHELPERPANEL_CMDNAME "\">\
<input type=hidden name=mode value=panel>\
<input type=hidden name=cookie value=\"%s\">\
<input type=submit value=\"%s\">\
</form><br>\
<form method=internal action=option>";

static Str optionpanel_str = NULL;

static Str
to_str(struct param_ptr* p)
{
    switch (p->type) {
    case P_INT:
    case P_COLOR:
    case P_CODE:
        return Sprintf("%d", (int)(*(enum wc_ces*)p->varptr));
    case P_NZINT:
        return Sprintf("%d", *(int*)p->varptr);
    case P_SHORT:
        return Sprintf("%d", *(short*)p->varptr);
    case P_CHARINT:
        return Sprintf("%d", *(char*)p->varptr);
    case P_CHAR:
        return Sprintf("%c", *(char*)p->varptr);
    case P_STRING:
    case P_SSLPATH:
        /*  SystemCharset -> InnerCharset */
        return Strnew_charp(conv_from_system(*(char**)p->varptr));
    case P_PIXELS:
    case P_SCALE:
        return Sprintf("%g", *(double*)p->varptr);
    }
    /* not reached */
    return NULL;
}

struct Buffer*
load_option_panel(void)
{
    Str src;
    struct param_ptr* p;
    struct sel_c* s;
    wc_ces_list* c;
    int x, i;
    Str tmp;
    struct Buffer* buf;

    if (optionpanel_str == NULL)
        optionpanel_str = Sprintf(optionpanel_src1, w3m_version,
            html_quote(localCookie()->ptr), _(CMT_HELPER));

    g_runtime.OptionCharset = g_runtime.SystemCharset; /* FIXME */
    if (!g_runtime.OptionEncode) {
        optionpanel_str = wc_Str_conv(optionpanel_str, g_runtime.OptionCharset, g_runtime.InnerCharset);
        for (i = 0; sections[i].name != NULL; i++) {
            sections[i].name = wc_conv(_(sections[i].name), g_runtime.OptionCharset,
                g_runtime.InnerCharset)
                                   ->ptr;
            for (p = sections[i].params; p->name; p++) {
                p->comment = wc_conv(_(p->comment), g_runtime.OptionCharset,
                    g_runtime.InnerCharset)
                                 ->ptr;
                if (p->inputtype == PI_SEL_C
                    && p->select != colorstr) {
                    for (s = (struct sel_c*)p->select; s->text != NULL; s++) {
                        s->text = wc_conv(_(s->text), g_runtime.OptionCharset,
                            g_runtime.InnerCharset)
                                      ->ptr;
                    }
                }
            }
        }

        for (s = colorstr; s->text; s++)
            s->text = wc_conv(_(s->text), g_runtime.OptionCharset,
                g_runtime.InnerCharset)
                          ->ptr;

        g_runtime.OptionEncode = TRUE;
    }

    src = Strdup(optionpanel_str);

    Strcat_charp(src, "<table><tr><td>");
    for (i = 0; sections[i].name != NULL; i++) {
        Strcat_m_charp(src, "<h1>", sections[i].name, "</h1>", NULL);
        p = sections[i].params;
        Strcat_charp(src, "<table width=100% cellpadding=0>");
        while (p->name) {
            Strcat_m_charp(src, "<tr><td>", p->comment, NULL);
            Strcat(src, Sprintf("</td><td width=%d>", (int)(28 * g_runtime.pixel_per_char)));
            switch (p->inputtype) {
            case PI_TEXT:
                Strcat_m_charp(src, "<input type=text name=",
                    p->name,
                    " value=\"",
                    html_quote(to_str(p)->ptr), "\">", NULL);
                break;
            case PI_ONOFF:
                x = atoi(to_str(p)->ptr);
                Strcat_m_charp(src, "<input type=radio name=",
                    p->name,
                    " value=1",
                    (x ? " checked" : ""),
                    ">YES&nbsp;&nbsp;<input type=radio name=",
                    p->name,
                    " value=0", (x ? "" : " checked"), ">NO", NULL);
                break;
            case PI_SEL_C:
                tmp = to_str(p);
                Strcat_m_charp(src, "<select name=", p->name, ">", NULL);
                for (s = (struct sel_c*)p->select; s->text != NULL; s++) {
                    Strcat_charp(src, "<option value=");
                    Strcat(src, Sprintf("%s\n", s->cvalue));
                    if ((p->type != P_CHAR && s->value == atoi(tmp->ptr)) || (p->type == P_CHAR && (char)s->value == *(tmp->ptr)))
                        Strcat_charp(src, " selected");
                    Strcat_char(src, '>');
                    Strcat_charp(src, s->text);
                }
                Strcat_charp(src, "</select>");
                break;
            case PI_CODE:
                tmp = to_str(p);
                Strcat_m_charp(src, "<select name=", p->name, ">", NULL);
                for (c = *(wc_ces_list**)p->select; c->desc != NULL; c++) {
                    Strcat_charp(src, "<option value=");
                    Strcat(src, Sprintf("%s\n", c->name));
                    if (c->id == atoi(tmp->ptr))
                        Strcat_charp(src, " selected");
                    Strcat_char(src, '>');
                    Strcat_charp(src, c->desc);
                }
                Strcat_charp(src, "</select>");
                break;
            }
            Strcat_charp(src, "</td></tr>\n");
            p++;
        }
        Strcat_charp(src,
            "<tr><td></td><td><p><input type=submit value=\"OK\"></td></tr>");
        Strcat_charp(src, "</table><hr width=50%>");
    }
    Strcat_charp(src, "</table></form></body></html>");
    buf = loadHTMLString(src);
    if (buf)
        buf->doc->charset = g_runtime.OptionCharset;
    return buf;
}

void panel_set_option(struct parsed_tagarg* arg)
{
    FILE* f = NULL;
    char* p;
    Str s = Strnew(), tmp;

    if (g_runtime.config_file == NULL) {
        disp_message("There's no config file... config not saved", FALSE);
    } else {
        f = fopen(g_runtime.config_file, "wt");
        if (f == NULL) {
            disp_message("Can't write option!", FALSE);
        }
    }
    while (arg) {
        /*  InnerCharset -> SystemCharset */
        if (arg->value) {
            p = conv_to_system(arg->value);
            if (set_param(arg->arg, p)) {
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
    backBf((struct DefunContext) { 0 });
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

struct FollowResult _followA(struct Buffer* buf, struct FollowOption option)
{
    if (Currentbuf->doc->firstLine == NULL) {
        return (struct FollowResult) { 0 };
    }

    struct Anchor* a = doc_retrieveCurrentImg(Currentbuf->doc);
    if (a && a->image && a->image->map) {
        return _followForm(buf, option, false);
    }

    int x = 0, y = 0, map = 0;
    if (a && a->image && a->image->ismap) {
        getMapXY(Currentbuf->doc, a, &x, &y);
        map = 1;
    }

    a = doc_retrieveCurrentAnchor(Currentbuf->doc);
    if (a == NULL) {
        return _followForm(buf, option, false);
    }
    if (*a->url == '#') { /* index within this buffer */
        return gotoLabel(Currentbuf, a->url + 1);
    }

    struct Url u;
    parseURL2(a->url, &u, baseURL(Currentbuf));
    if (Strcmp(parsedURL2Str(&u), parsedURL2Str(&Currentbuf->content->url)) == 0) {
        /* index within this buffer */
        if (u.label) {
            return gotoLabel(Currentbuf, u.label);
        }
    }
    if (handleMailto(a->url))
        return (struct FollowResult) { 0 };

    const char* url = a->url;
    if (map)
        url = Sprintf("%s?%d,%d", a->url, x, y)->ptr;

    return (struct FollowResult) {
        .anchor = a,
        .new_buf = loadLink(url, NULL, a->target, a->referer, option),
    };
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
    copyBuffer(res.new_buf, buf);
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
            (struct LoadOption) { .base_url = baseURL(Currentbuf), .referer = NULL, .flag = 0 });
        return;
    }

    struct Content* content = get_content_cache(a->url, NULL,
        (struct LoadOption) { .base_url = baseURL(Currentbuf), .referer = NULL, .flag = 0 });
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

        struct Url* current = baseURL(buf);
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
                url = url_decode2(baseURL(buf), buf->doc, a_url);
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
        current = baseURL(buf);
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
        parseURL2(a->url, &pu, baseURL(buf));
        s = parsedURL2Str(&pu);
    }
    if (getRuntime()->DecodeURL)
        s = Strnew_charp(url_decode2(baseURL(buf), buf->doc, s->ptr));
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
            buf = CurrentTab->firstBuffer->nextBuffer;
            discardBuffer(CurrentTab->firstBuffer);
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

char* searchKeyData(void)
{
    const char* data = NULL;
    if (getRuntime()->CurrentKeyData != NULL && *getRuntime()->CurrentKeyData != '\0')
        data = getRuntime()->CurrentKeyData;
    else if (getRuntime()->CurrentCmdData != NULL && *getRuntime()->CurrentCmdData != '\0')
        data = getRuntime()->CurrentCmdData;
    else if (getRuntime()->CurrentKey >= 0)
        data = getKeyData(getRuntime()->CurrentKey);
    getRuntime()->CurrentKeyData = NULL;
    getRuntime()->CurrentCmdData = NULL;
    if (data == NULL || *data == '\0')
        return NULL;
    return allocStr(data, -1);
}
