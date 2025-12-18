#include "w3m_runtime.h"
#include "anchor.h"
#include "frame.h"
#include "fm.h"
#include "tab.h"
#include "buffer.h"
#include "image.h"
#include "terms.h"
#include "display.h"
#include "config.h"
#include "indep.h"
#include "myctype.h"

#include <libwc/ucs.h>

#include <signal.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <errno.h>
#include <termcap.h>
#include <unistd.h>

static struct termios d_ioval;

// rc
char UseGraphicChar = GRAPHIC_CHAR_CHARSET;

struct Runtime g_runtime = {
    .lines = 0,
    .cols = 0,
    .Do_not_use_ti_te = false,

    .CurrentTab = 0,
    .FirstTab = 0,
    .LastTab = 0,
    .nTab = 0,

    .CurrentKey = -1,
    .CurrentKeyData = 0,
    .CurrentCmdData = 0,
    .prec_num = 0,
    .prev_key = -1,

    .CurrentEvent = 0,
    .LastEvent = 0,
};
struct Runtime* getRuntime()
{
    return &g_runtime;
}
struct TabBuffer* CurrentTab()
{
    return g_runtime.CurrentTab;
}
struct TabBuffer* FirstTab()
{
    return g_runtime.FirstTab;
}
struct TabBuffer* LastTab()
{
    return g_runtime.LastTab;
}
int nTab()
{
    return g_runtime.nTab;
}

#define MAXIMUM_COLS 1024
void tty_set_cols(int cols)
{
    g_runtime.cols = cols;
    if (g_runtime.cols > MAXIMUM_COLS) {
        g_runtime.cols = MAXIMUM_COLS;
    }
}

static MySignalHandler reset_exit_with_value(SIGNAL_ARG, int rval)
{
    exitRawMode();
    w3m_exit(rval);
    SIGNAL_RETURN;
}

MySignalHandler reset_error_exit(SIGNAL_ARG)
{
    reset_exit_with_value(SIGNAL_ARGLIST, 1);
}

MySignalHandler
reset_exit(SIGNAL_ARG)
{
    reset_exit_with_value(SIGNAL_ARGLIST, 0);
}

MySignalHandler
error_dump(SIGNAL_ARG)
{
    mySignal(SIGIOT, SIG_DFL);
    exitRawMode();
    abort();
    SIGNAL_RETURN;
}

void set_int(void)
{
    mySignal(SIGHUP, reset_exit);
    mySignal(SIGINT, reset_exit);
    mySignal(SIGQUIT, reset_exit);
    mySignal(SIGTERM, reset_exit);
    mySignal(SIGILL, error_dump);
    mySignal(SIGIOT, error_dump);
    mySignal(SIGFPE, error_dump);
#ifdef SIGBUS
    mySignal(SIGBUS, error_dump);
#endif /* SIGBUS */
    /* mySignal(SIGSEGV, error_dump); */
}

static void
setgraphchar(void)
{

    for (int c = 0; c < 96; c++)
        g_runtime.gcmap[c] = (char)(c + ' ');

    if (!getRuntime()->T_ac)
        return;

    int n = strlen(getRuntime()->T_ac);
    for (int i = 0; i < n - 1; i += 2) {
        int c = (unsigned)getRuntime()->T_ac[i] - ' ';
        if (c >= 0 && c < 96)
            g_runtime.gcmap[c] = getRuntime()->T_ac[i + 1];
    }
}

char graphchar(char c)
{
    return (((unsigned)(c) >= ' ' && (unsigned)(c) < 128) ? g_runtime.gcmap[(c) - ' '] : (c));
}

#define GETSTR(v, s)               \
    {                              \
        v = pt;                    \
        suc = tgetstr(s, &pt);     \
        if (!suc)                  \
            v = "";                \
        else                       \
            v = allocStr(suc, -1); \
    }

static char bp[1024], funcstr[256];

static void getTCstr(void)
{
    char* ent = getenv("TERM") ? getenv("TERM") : DEFAULT_TERM;
    if (ent == NULL) {
        fprintf(stderr, "TERM is not set\n");
        reset_error_exit(SIGNAL_ARGLIST);
    }

    int r = tgetent(bp, ent);
    if (r != 1) {
        /* Can't find termcap entry */
        fprintf(stderr, "Can't find termcap entry %s\n", ent);
        reset_error_exit(SIGNAL_ARGLIST);
    }

    char* suc;
    char* pt = funcstr;
    GETSTR(g_runtime.T_ce, "ce"); /* clear to the end of line */
    GETSTR(g_runtime.T_cd, "cd"); /* clear to the end of display */
    GETSTR(g_runtime.T_kr, "nd"); /* cursor right */
    if (suc == NULL)
        GETSTR(g_runtime.T_kr, "kr");
    if (tgetflag("bs"))
        g_runtime.T_kl = "\b"; /* cursor left */
    else {
        GETSTR(g_runtime.T_kl, "le");
        if (suc == NULL)
            GETSTR(g_runtime.T_kl, "kb");
        if (suc == NULL)
            GETSTR(g_runtime.T_kl, "kl");
    }
    GETSTR(g_runtime.T_cr, "cr"); /* carriage return */
    GETSTR(g_runtime.T_ta, "ta"); /* tab */
    GETSTR(g_runtime.T_sc, "sc"); /* save cursor */
    GETSTR(g_runtime.T_rc, "rc"); /* restore cursor */
    GETSTR(g_runtime.T_so, "so"); /* standout mode */
    GETSTR(g_runtime.T_se, "se"); /* standout mode end */
    GETSTR(g_runtime.T_us, "us"); /* underline mode */
    GETSTR(g_runtime.T_ue, "ue"); /* underline mode end */
    GETSTR(g_runtime.T_md, "md"); /* bold mode */
    GETSTR(g_runtime.T_me, "me"); /* bold mode end */
    GETSTR(g_runtime.T_cl, "cl"); /* clear screen */
    GETSTR(g_runtime.T_cm, "cm"); /* cursor move */
    GETSTR(g_runtime.T_al, "al"); /* append line */
    GETSTR(g_runtime.T_sr, "sr"); /* scroll reverse */
    GETSTR(g_runtime.T_ti, "ti"); /* terminal init */
    GETSTR(g_runtime.T_te, "te"); /* terminal end */
    GETSTR(g_runtime.T_nd, "nd"); /* move right one space */
    GETSTR(g_runtime.T_eA, "eA"); /* enable alternative charset */
    GETSTR(g_runtime.T_as, "as"); /* alternative (graphic) charset start */
    GETSTR(g_runtime.T_ae, "ae"); /* alternative (graphic) charset end */
    GETSTR(g_runtime.T_ac, "ac"); /* graphics charset pairs */
    GETSTR(g_runtime.T_op, "op"); /* set default color pair to its original value */
#if defined(CYGWIN) && CYGWIN < 1
    /* for TERM=pcansi on MS-DOS prompt. */
#if 0
    T_eA = "";
    T_as = "\033[12m";
    T_ae = "\033[10m";
    T_ac = "l\001k\002m\003j\004x\005q\006n\020a\024v\025w\026u\027t\031";
#endif
    T_eA = "";
    T_as = "";
    T_ae = "";
    T_ac = "";
#endif /* CYGWIN */

    setlinescols();
    setgraphchar();
}

void init_tty()
{
    getTCstr();
}

char* ttyname_tty(void)
{
    return ttyname(0);
    // g_runtime.tty_input);
}

static void
skip_escseq(void)
{
    int c = getch();
    if (c == '[' || c == 'O') {
        c = getch();
        while (IS_DIGIT(c))
            c = getch();
    }
}

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
    writestr(tgoto(g_runtime.T_cm, column, line));
}

void initscr(void)
{
    set_int();
    if (g_runtime.T_ti && !g_runtime.Do_not_use_ti_te)
        writestr(g_runtime.T_ti);
    setupscreen();
}

int graph_ok(void)
{
    if (UseGraphicChar != GRAPHIC_CHAR_DEC)
        return 0;
    return g_runtime.T_as[0] != 0 && g_runtime.T_ae[0] != 0 && g_runtime.T_ac[0] != 0;
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

//
// tab
//
void _newT(void)
{
    struct TabBuffer* tag = newTab();
    if (!tag)
        return;

    Buffer* buf = newBuffer(Currentbuf->width);
    copyBuffer(buf, Currentbuf);
    buf->nextBuffer = NULL;
    for (int i = 0; i < MAX_LB; i++)
        buf->linkBuffer[i] = NULL;
    (*buf->clone)++;
    tag->firstBuffer = tag->currentBuffer = buf;

    tag->nextTab = g_runtime.CurrentTab->nextTab;
    tag->prevTab = g_runtime.CurrentTab;
    if (g_runtime.CurrentTab->nextTab)
        g_runtime.CurrentTab->nextTab->prevTab = tag;
    else
        g_runtime.LastTab = tag;
    g_runtime.CurrentTab->nextTab = tag;
    g_runtime.CurrentTab = tag;
    g_runtime.nTab++;
}

void tabs_prepare()
{
    g_runtime.CurrentTab = g_runtime.LastTab;
    if (!g_runtime.FirstTab) {
        g_runtime.FirstTab = g_runtime.LastTab = g_runtime.CurrentTab = newTab();
        g_runtime.nTab = 1;
    }
}

void calcTabPos(void)
{
    struct TabBuffer* tab;
    int lcol = 0, rcol = 0, col;
    int n1, n2, na, nx, ny, ix, iy;

    if (nTab <= 0)
        return;
    n1 = (TTY_COLS() - rcol - lcol) / TabCols;
    if (n1 >= g_runtime.nTab) {
        n2 = 1;
        ny = 1;
    } else {
        if (n1 < 0)
            n1 = 0;
        n2 = TTY_COLS() / TabCols;
        if (n2 == 0)
            n2 = 1;
        ny = (g_runtime.nTab - n1 - 1) / n2 + 2;
    }
    na = n1 + n2 * (ny - 1);
    n1 -= (na - g_runtime.nTab) / ny;
    if (n1 < 0)
        n1 = 0;
    na = n1 + n2 * (ny - 1);
    tab = g_runtime.FirstTab;
    for (iy = 0; iy < ny && tab; iy++) {
        if (iy == 0) {
            nx = n1;
            col = TTY_COLS() - rcol - lcol;
        } else {
            nx = n2 - (na - g_runtime.nTab + (iy - 1)) / (ny - 1);
            col = TTY_COLS();
        }
        for (ix = 0; ix < nx && tab; ix++, tab = tab->nextTab) {
            tab->x1 = col * ix / nx;
            tab->x2 = col * (ix + 1) / nx - 1;

            tab->y = iy;
            if (iy == 0) {
                tab->x1 += lcol;
                tab->x2 += lcol;
            }
        }
    }
}

static Str
conv_form_encoding(Str val, FormItemList* fi, Buffer* buf)
{
    wc_ces charset = SystemCharset;

    if (fi->parent->charset)
        charset = fi->parent->charset;
    else if (buf->document_charset && buf->document_charset != WC_CES_US_ASCII)
        charset = buf->document_charset;
    return wc_Str_conv_strict(val, InnerCharset, charset);
}

void query_from_followform(Str* query, FormItemList* fi, int multipart)
{
    FormItemList* f2;
    FILE* body = NULL;

    if (multipart) {
        *query = tmpfname(TMPF_DFL, NULL);
        body = fopen((*query)->ptr, "w");
        if (body == NULL) {
            return;
        }
        fi->parent->body = (*query)->ptr;
        fi->parent->boundary = Sprintf("------------------------------%d%ld%ld%ld", CurrentPid,
            fi->parent, fi->parent->body, fi->parent->boundary)
                                   ->ptr;
    }
    *query = Strnew();
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
#ifdef USE_IMAGE
                getMapXY(Currentbuf, retrieveCurrentImg(Currentbuf), &x, &y);
#endif
                *query = Strdup(conv_form_encoding(f2->name, fi, Currentbuf));
                Strcat_charp(*query, ".x");
                form_write_data(body, fi->parent->boundary, (*query)->ptr,
                    Sprintf("%d", x)->ptr);
                *query = Strdup(conv_form_encoding(f2->name, fi, Currentbuf));
                Strcat_charp(*query, ".y");
                form_write_data(body, fi->parent->boundary, (*query)->ptr,
                    Sprintf("%d", y)->ptr);
            } else if (f2->name && f2->name->length > 0 && f2->value != NULL) {
                /* not IMAGE */
                *query = conv_form_encoding(f2->value, fi, Currentbuf);
                if (f2->type == FORM_INPUT_FILE)
                    form_write_from_file(body, fi->parent->boundary,
                        conv_form_encoding(f2->name, fi,
                            Currentbuf)
                            ->ptr,
                        (*query)->ptr,
                        Str_conv_to_system(f2->value)->ptr);
                else
                    form_write_data(body, fi->parent->boundary,
                        conv_form_encoding(f2->name, fi,
                            Currentbuf)
                            ->ptr,
                        (*query)->ptr);
            }
        } else {
            /* not multipart */
            if (f2->type == FORM_INPUT_IMAGE) {
                int x = 0, y = 0;
#ifdef USE_IMAGE
                getMapXY(Currentbuf, retrieveCurrentImg(Currentbuf), &x, &y);
#endif
                Strcat(*query,
                    Str_form_quote(conv_form_encoding(f2->name, fi, Currentbuf)));
                Strcat(*query, Sprintf(".x=%d&", x));
                Strcat(*query,
                    Str_form_quote(conv_form_encoding(f2->name, fi, Currentbuf)));
                Strcat(*query, Sprintf(".y=%d", y));
            } else {
                /* not IMAGE */
                if (f2->name && f2->name->length > 0) {
                    Strcat(*query,
                        Str_form_quote(conv_form_encoding(f2->name, fi, Currentbuf)));
                    Strcat_char(*query, '=');
                }
                if (f2->value != NULL) {
                    if (fi->parent->method == FORM_METHOD_INTERNAL)
                        Strcat(*query, Str_form_quote(f2->value));
                    else {
                        Strcat(*query,
                            Str_form_quote(conv_form_encoding(f2->value, fi, Currentbuf)));
                    }
                }
            }
            if (f2->next)
                Strcat_char(*query, '&');
        }
    }
    if (multipart) {
        fprintf(body, "--%s--\r\n", fi->parent->boundary);
        fclose(body);
    } else {
        /* remove trailing & */
        while (Strlastchar(*query) == '&')
            Strshrink(*query, 1);
    }
}

static Buffer*
loadNormalBuf(Buffer* buf, int renderframe)
{
    pushBuffer(buf);
    if (renderframe && RenderFrame && Currentbuf->frameset != NULL)
        rFrame();
    return buf;
}

Buffer* loadLink(char* url, char* target, char* referer, FormList* request, bool on_target, bool do_download)
{
    Buffer *buf, *nfbuf;
    union frameset_element* f_element = NULL;
    int flag = 0;
    struct Url *base, pu;
    const int* no_referer_ptr;

    message(Sprintf("loading %s", url)->ptr, 0, 0);
    refresh();

    no_referer_ptr = query_SCONF_NO_REFERER_FROM(&Currentbuf->currentURL);
    base = baseURL(Currentbuf);
    if ((no_referer_ptr && *no_referer_ptr) || base == NULL || base->scheme == SCM_LOCAL || base->scheme == SCM_LOCAL_CGI || base->scheme == SCM_DATA)
        referer = NO_REFERER;
    if (referer == NULL)
        referer = parsedURL2RefererStr(&Currentbuf->currentURL)->ptr;
    buf = loadGeneralFile(url, baseURL(Currentbuf), referer, flag, request, do_download);
    if (buf == NULL) {
        char* emsg = Sprintf("Can't load %s", url)->ptr;
        disp_err_message(emsg, FALSE);
        return NULL;
    }

    parseURL2(url, &pu, base);
    pushHashHist(URLHist, parsedURL2Str(&pu)->ptr);

    if (buf == NO_BUFFER) {
        return NULL;
    }
    if (!on_target) /* open link as an indivisual page */
        return loadNormalBuf(buf, TRUE);

    if (do_download) /* download (thus no need to render frames) */
        return loadNormalBuf(buf, FALSE);

    if (target == NULL || /* no target specified (that means this page is not a frame page) */
        !strcmp(target, "_top") || /* this link is specified to be opened as an indivisual * page */
        !(Currentbuf->bufferprop & BP_FRAME) /* This page is not a frame page */
    ) {
        return loadNormalBuf(buf, TRUE);
    }
    nfbuf = Currentbuf->linkBuffer[LB_N_FRAME];
    if (nfbuf == NULL) {
        /* original page (that contains <frameset> tag) doesn't exist */
        return loadNormalBuf(buf, TRUE);
    }

    f_element = search_frame(nfbuf->frameset, target);
    if (f_element == NULL) {
        /* specified target doesn't exist in this frameset */
        return loadNormalBuf(buf, TRUE);
    }

    /* frame page */

    /* stack current frameset */
    pushFrameTree(&(nfbuf->frameQ), copyFrameSet(nfbuf->frameset), Currentbuf);
    /* delete frame view buffer */
    delBuffer(Currentbuf);
    Currentbuf = nfbuf;
    /* nfbuf->frameset = copyFrameSet(nfbuf->frameset); */
    resetFrameElement(f_element, buf, referer, request);
    discardBuffer(buf);
    rFrame();
    {
        struct Anchor* al = NULL;
        char* label = pu.label;

        if (label && f_element->element->attr == F_BODY) {
            al = searchAnchor(f_element->body->nameList, label);
        }
        if (!al) {
            label = Strnew_m_charp("_", target, NULL)->ptr;
            al = searchURLLabel(Currentbuf, label);
        }
        if (al) {
            gotoLine(Currentbuf, al->start.line);
            if (label_topline)
                Currentbuf->topLine = lineSkip(Currentbuf, Currentbuf->topLine,
                    Currentbuf->currentLine->linenumber - Currentbuf->topLine->linenumber,
                    FALSE);
            Currentbuf->pos = al->start.pos;
            arrangeCursor(Currentbuf);
        }
    }
    displayBuffer(Currentbuf, B_NORMAL);
    return buf;
}

static FormItemList*
save_submit_formlist(FormItemList* src)
{
    FormList* list;
    FormList* srclist;
    FormItemList* srcitem;
    FormItemList* item;
    FormItemList* ret = NULL;
#ifdef MENU_SELECT
    FormSelectOptionItem* opt;
    FormSelectOptionItem* curopt;
    FormSelectOptionItem* srcopt;
#endif /* MENU_SELECT */

    if (src == NULL)
        return NULL;
    srclist = src->parent;
    list = New(FormList);
    list->method = srclist->method;
    list->action = Strdup(srclist->action);
#ifdef USE_M17N
    list->charset = srclist->charset;
#endif
    list->enctype = srclist->enctype;
    list->nitems = srclist->nitems;
    list->body = srclist->body;
    list->boundary = srclist->boundary;
    list->length = srclist->length;

    for (srcitem = srclist->item; srcitem; srcitem = srcitem->next) {
        item = New(FormItemList);
        item->type = srcitem->type;
        item->name = Strdup(srcitem->name);
        item->value = Strdup(srcitem->value);
        item->checked = srcitem->checked;
        item->accept = srcitem->accept;
        item->size = srcitem->size;
        item->rows = srcitem->rows;
        item->maxlength = srcitem->maxlength;
        item->readonly = srcitem->readonly;
#ifdef MENU_SELECT
        opt = curopt = NULL;
        for (srcopt = srcitem->select_option; srcopt; srcopt = srcopt->next) {
            if (!srcopt->checked)
                continue;
            opt = New(FormSelectOptionItem);
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
#endif /* MENU_SELECT */
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

void _followForm(bool submit, bool on_target, bool do_download)
{
    struct Anchor *a, *a2;
    char* p;
    FormItemList *fi, *f2;
    Str tmp, tmp2;
    int multipart = 0, i;

    if (Currentbuf->firstLine == NULL)
        return;

    a = retrieveCurrentForm(Currentbuf);
    if (a == NULL)
        return;
    fi = (FormItemList*)a->url;
    switch (fi->type) {
    case FORM_INPUT_TEXT:
        if (submit)
            goto do_submit;
        if (fi->readonly)
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
        /* FIXME: gettextize? */
        p = inputStrHist("TEXT:", fi->value ? fi->value->ptr : NULL, TextHist);
        if (p == NULL || fi->readonly)
            break;
        fi->value = Strnew_charp(p);
        formUpdateBuffer(a, Currentbuf, fi);
        if (fi->accept || fi->parent->nitems == 1)
            goto do_submit;
        break;
    case FORM_INPUT_FILE:
        if (submit)
            goto do_submit;
        if (fi->readonly)
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
        /* FIXME: gettextize? */
        p = inputFilenameHist("Filename:", fi->value ? fi->value->ptr : NULL,
            NULL);
        if (p == NULL || fi->readonly)
            break;
        fi->value = Strnew_charp(p);
        formUpdateBuffer(a, Currentbuf, fi);
        if (fi->accept || fi->parent->nitems == 1)
            goto do_submit;
        break;
    case FORM_INPUT_PASSWORD:
        if (submit)
            goto do_submit;
        if (fi->readonly) {
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
            break;
        }
        /* FIXME: gettextize? */
        p = inputLine("Password:", fi->value ? fi->value->ptr : NULL,
            IN_PASSWORD);
        if (p == NULL)
            break;
        fi->value = Strnew_charp(p);
        formUpdateBuffer(a, Currentbuf, fi);
        if (fi->accept)
            goto do_submit;
        break;
    case FORM_TEXTAREA:
        if (submit)
            goto do_submit;
        if (fi->readonly)
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
        input_textarea(fi);
        formUpdateBuffer(a, Currentbuf, fi);
        break;
    case FORM_INPUT_RADIO:
        if (submit)
            goto do_submit;
        if (fi->readonly) {
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
            break;
        }
        formRecheckRadio(a, Currentbuf, fi);
        break;
    case FORM_INPUT_CHECKBOX:
        if (submit)
            goto do_submit;
        if (fi->readonly) {
            /* FIXME: gettextize? */
            disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
            break;
        }
        fi->checked = !fi->checked;
        formUpdateBuffer(a, Currentbuf, fi);
        break;

    case FORM_SELECT:
        if (submit)
            goto do_submit;
        if (!formChooseOptionByMenu(fi,
                Currentbuf->cursorX - Currentbuf->pos + a->start.pos + Currentbuf->rootX,
                Currentbuf->cursorY + Currentbuf->rootY))
            break;
        formUpdateBuffer(a, Currentbuf, fi);
        if (fi->parent->nitems == 1)
            goto do_submit;
        break;

    case FORM_INPUT_IMAGE:
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
    do_submit:
        tmp = Strnew();
        multipart = (fi->parent->method == FORM_METHOD_POST && fi->parent->enctype == FORM_ENCTYPE_MULTIPART);
        query_from_followform(&tmp, fi, multipart);

        tmp2 = Strdup(fi->parent->action);
        if (!Strcmp_charp(tmp2, "!CURRENT_URL!")) {
            /* It means "current URL" */
            tmp2 = parsedURL2Str(&Currentbuf->currentURL);
            if ((p = strchr(tmp2->ptr, '?')) != NULL)
                Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
        }

        if (fi->parent->method == FORM_METHOD_GET) {
            if ((p = strchr(tmp2->ptr, '?')) != NULL)
                Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
            Strcat_charp(tmp2, "?");
            Strcat(tmp2, tmp);
            loadLink(tmp2->ptr, a->target, NULL, NULL, on_target, do_download);
        } else if (fi->parent->method == FORM_METHOD_POST) {
            Buffer* buf;
            if (multipart) {
                struct stat st;
                stat(fi->parent->body, &st);
                fi->parent->length = st.st_size;
            } else {
                fi->parent->body = tmp->ptr;
                fi->parent->length = tmp->length;
            }
            buf = loadLink(tmp2->ptr, a->target, NULL, fi->parent, on_target, do_download);
            if (multipart) {
                unlink(fi->parent->body);
            }
            if (buf && !(buf->bufferprop & BP_REDIRECTED)) { /* buf must be Currentbuf */
                /* BP_REDIRECTED means that the buffer is obtained through
                 * Location: header. In this case, buf->form_submit must not be set
                 * because the page is not loaded by POST method but GET method.
                 */
                buf->form_submit = save_submit_formlist(fi);
            }
        } else if ((fi->parent->method == FORM_METHOD_INTERNAL && (!Strcmp_charp(fi->parent->action, "map") || !Strcmp_charp(fi->parent->action, "none"))) || Currentbuf->bufferprop & BP_INTERNAL) { /* internal */
            do_internal(tmp2->ptr, tmp->ptr);
        } else {
            disp_err_message("Can't send form because of illegal method.",
                FALSE);
        }
        break;
    case FORM_INPUT_RESET:
        for (i = 0; i < Currentbuf->formitem->nanchor; i++) {
            a2 = &Currentbuf->formitem->anchors[i];
            f2 = (FormItemList*)a2->url;
            if (f2->parent == fi->parent && f2->name && f2->value && f2->type != FORM_INPUT_SUBMIT && f2->type != FORM_INPUT_HIDDEN && f2->type != FORM_INPUT_RESET) {
                f2->value = f2->init_value;
                f2->checked = f2->init_checked;
                f2->label = f2->init_label;
                f2->selected = f2->init_selected;
                formUpdateBuffer(a2, Currentbuf, f2);
            }
        }
        break;
    case FORM_INPUT_HIDDEN:
    default:
        break;
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

bool currentBufferSubmit()
{
    struct Anchor* a = Currentbuf->submit;
    if (!a) {
        return false;
    }
    Currentbuf->submit = NULL;
    gotoLine(Currentbuf, a->start.line);
    Currentbuf->pos = a->start.pos;
    _followForm(TRUE, true, false);
    return true;
}

bool eventUpdate()
{
    if (!g_runtime.CurrentEvent) {
        return false;
    }
    g_runtime.CurrentKey = -1;
    g_runtime.CurrentKeyData = NULL;
    g_runtime.CurrentCmdData = (char*)g_runtime.CurrentEvent->data;
    w3mFuncList[g_runtime.CurrentEvent->cmd].func();
    g_runtime.CurrentCmdData = NULL;
    g_runtime.CurrentEvent = g_runtime.CurrentEvent->next;
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

void keyPressEventProc(int c)
{
    g_runtime.CurrentKey = c;
    w3mFuncList[(int)GlobalKeymap[c]].func();
}

void escKeyProc(int c, int esc, unsigned char* map)
{
    if (g_runtime.CurrentKey >= 0 && g_runtime.CurrentKey & K_MULTI) {
        unsigned char** mmap;
        mmap = (unsigned char**)getKeyData(MULTI_KEY(g_runtime.CurrentKey));
        if (!mmap)
            return;
        switch (esc) {
        case K_ESCD:
            map = mmap[3];
            break;
        case K_ESCB:
            map = mmap[2];
            break;
        case K_ESC:
            map = mmap[1];
            break;
        default:
            map = mmap[0];
            break;
        }
        esc |= (g_runtime.CurrentKey & ~0xFFFF);
    }
    g_runtime.CurrentKey = esc | c;
    if (map)
        w3mFuncList[(int)map[c]].func();
}

void w3m_end_frame()
{
    g_runtime.prev_key = g_runtime.CurrentKey;
    g_runtime.CurrentKey = -1;
    g_runtime.CurrentKeyData = NULL;
}

#define PREC_LIMIT 10000

int is_wordchar(wc_uint32 c)
{
    return wc_is_ucs_alnum(c);
}

wc_uint32 getChar(char* p)
{
    return wc_any_to_ucs(wtf_parse1((wc_uchar**)&p));
}

char* getCurWord(Buffer* buf, int* spos, int* epos)
{
    char* p;
    struct Line* l = buf->currentLine;
    int b, e;

    *spos = 0;
    *epos = 0;
    if (l == NULL)
        return NULL;
    p = l->lineBuf;
    e = buf->pos;
    while (e > 0 && !is_wordchar(getChar(&p[e])))
        prevChar(e, l);
    if (!is_wordchar(getChar(&p[e])))
        return NULL;
    b = e;
    while (b > 0) {
        int tmp = b;
        prevChar(tmp, l);
        if (!is_wordchar(getChar(&p[tmp])))
            break;
        b = tmp;
    }
    while (e < l->len && is_wordchar(getChar(&p[e])))
        nextChar(e, l);
    *spos = b;
    *epos = e;
    return &p[b];
}

char* GetWord(Buffer* buf)
{
    int b, e;
    char* p;
    if ((p = getCurWord(buf, &b, &e)) != NULL) {
        return Strnew_charp_n(p, e - b)->ptr;
    }
    return NULL;
}

static void set_buffer_environ(Buffer* buf)
{
    static Buffer* prev_buf = NULL;
    static struct Line* prev_line = NULL;
    static int prev_pos = -1;
    struct Line* l;

    if (buf == NULL)
        return;
    if (buf != prev_buf) {
        set_environ("W3M_SOURCEFILE", buf->sourcefile);
        set_environ("W3M_FILENAME", buf->filename);
        set_environ("W3M_TITLE", buf->buffername);
        set_environ("W3M_URL", parsedURL2Str(&buf->currentURL)->ptr);
        set_environ("W3M_TYPE", buf->real_type ? buf->real_type : "unknown");
        set_environ("W3M_CHARSET", wc_ces_to_charset(buf->document_charset));
    }
    l = buf->currentLine;
    if (l && (buf != prev_buf || l != prev_line || buf->pos != prev_pos)) {
        struct Anchor* a;
        struct Url pu;
        char* s = GetWord(buf);
        set_environ("W3M_CURRENT_WORD", s ? s : "");
        a = retrieveCurrentAnchor(buf);
        if (a) {
            parseURL2(a->url, &pu, baseURL(buf));
            set_environ("W3M_CURRENT_LINK", parsedURL2Str(&pu)->ptr);
        } else
            set_environ("W3M_CURRENT_LINK", "");
        a = retrieveCurrentImg(buf);
        if (a) {
            parseURL2(a->url, &pu, baseURL(buf));
            set_environ("W3M_CURRENT_IMG", parsedURL2Str(&pu)->ptr);
        } else
            set_environ("W3M_CURRENT_IMG", "");
        a = retrieveCurrentForm(buf);
        if (a)
            set_environ("W3M_CURRENT_FORM", form2str((FormItemList*)a->url));
        else
            set_environ("W3M_CURRENT_FORM", "");
        set_environ("W3M_CURRENT_LINE", Sprintf("%ld", l->real_linenumber)->ptr);
        set_environ("W3M_CURRENT_COLUMN", Sprintf("%d", buf->currentColumn + buf->cursorX + 1)->ptr);
    } else if (!l) {
        set_environ("W3M_CURRENT_WORD", "");
        set_environ("W3M_CURRENT_LINK", "");
        set_environ("W3M_CURRENT_IMG", "");
        set_environ("W3M_CURRENT_FORM", "");
        set_environ("W3M_CURRENT_LINE", "0");
        set_environ("W3M_CURRENT_COLUMN", "0");
    }
    prev_buf = buf;
    prev_line = l;
    prev_pos = buf->pos;
}

static void
save_buffer_position(Buffer* buf)
{
    BufferPos* b = buf->undo;

    if (!buf->firstLine)
        return;
    if (b && b->top_linenumber == TOP_LINENUMBER(buf) && b->cur_linenumber == CUR_LINENUMBER(buf) && b->currentColumn == buf->currentColumn && b->pos == buf->pos)
        return;
    b = New(BufferPos);
    b->top_linenumber = TOP_LINENUMBER(buf);
    b->cur_linenumber = CUR_LINENUMBER(buf);
    b->currentColumn = buf->currentColumn;
    b->pos = buf->pos;
    b->bpos = buf->currentLine ? buf->currentLine->bpos : 0;
    b->next = NULL;
    b->prev = buf->undo;
    if (buf->undo)
        buf->undo->next = b;
    buf->undo = b;
}

void w3m_on_key(uint8_t ch)
{
    if (IS_ASCII(ch)) {
        if (('0' <= ch) && (ch <= '9') && (g_runtime.prec_num || (GlobalKeymap[ch] == FUNCNAME_nulcmd))) {
            g_runtime.prec_num = g_runtime.prec_num * 10 + (int)(ch - '0');
            if (g_runtime.prec_num > PREC_LIMIT)
                g_runtime.prec_num = PREC_LIMIT;
        } else {
            set_buffer_environ(Currentbuf);
            save_buffer_position(Currentbuf);
            keyPressEventProc(ch);
            g_runtime.prec_num = 0;
        }
    }
}
