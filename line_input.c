#include "line_input.h"
#include "LineInput.h"
#include "alloc.h"
#include "history.h"
#include "qsort_util.h"
#include "term_tty.h"
#include "global.h"
#include "ctrlcode.h"
#include "form.h"
#include "terms.h"
#include "indep.h"
#include "tab.h"
#include "url.h"
#include "etc.h"
#include "display.h"
#include "local_cgi.h"

#include "wc_util.h"
#include <libwc/charset.h>

#include <sys/stat.h>
#include <dirent.h>

#define CLEN (COLS - 2)

enum CompletionStatus {
    CPL_OK = 0,
    CPL_AMBIG = 1,
    CPL_FAIL = 2,
    CPL_MENU = 3,
};

static Str CompleteBuf;
static Str CFileName;
static Str CBeforeBuf;
static Str CAfterBuf;
static Str CDirBuf;
static char** CFileBuf = NULL;
static int NCFileBuf;
static int NCFileOffset;

// static void insertself(char c),

;
static int terminated(unsigned char c);

static void next_compl(int next);
static void next_dcompl(struct CmdArgs* args, int next);
static Str doComplete(Str ifn, enum CompletionStatus* status, int next);

// clang-format off
InputFunc InputKeymap[32] = {
    /*  C-@     C-a     C-b     C-c     C-d     C-e     C-f     C-g     */
    _compl, _mvB, _mvL, _inbrk, delC, _mvE, _mvR, _inbrk,
    /*  C-h     C-i     C-j     C-k     C-l     C-m     C-n     C-o     */
    _bs, iself, _enter, killn, iself, _enter, _next, _editor,
    /*  C-p     C-q     C-r     C-s     C-t     C-u     C-v     C-w     */
    _prev, _quo, _bsw, iself, _mvLw, killb, _quo, _bsw,
    /*  C-x     C-y     C-z     C-[     C-\     C-]     C-^     C-_     */
    _tcompl, _mvRw, iself, _esc, iself, iself, iself, iself,
};
// clang-format on

static void addPasswd(char* p, Lineprop* pr, int len, int pos, int limit);
static void addStr(char* p, Lineprop* pr, int len, int pos, int limit);
static void ins_char(struct CmdArgs* args, Str str);

static enum HistoryType CurrentHist = HistoryNone;
static Str strCurrentBuf;
static int use_hist;

static struct LineInput g;

char* inputLineHistSearch(struct CmdArgs* args, const char* prompt, const char* def_str,
    enum InputLineFlags flag, enum HistoryType hist, IncrFunc incrfunc)
{
    int opos, x, y, lpos, rpos, epos;
    char* p;
    Str tmp;

    g = LineInputInit(def_str);

    CurrentHist = hist;
    if (hist != HistoryNone) {
        use_hist = true;
        strCurrentBuf = NULL;
    } else {
        use_hist = false;
    }
    if (flag & IN_URL) {
        g.cm_mode = CPL_ALWAYS | CPL_URL;
    } else if (flag & IN_FILENAME) {
        g.cm_mode = CPL_ALWAYS;
    } else if (flag & IN_PASSWORD) {
        g.cm_mode = CPL_NEVER;
        g.is_passwd = true;
        g.move_word = false;
    } else if (flag & IN_COMMAND)
        g.cm_mode = CPL_ON;
    else
        g.cm_mode = CPL_OFF;
    opos = get_strwidth(WcOption, prompt);
    epos = CLEN - opos;
    if (epos < 0)
        epos = 0;
    lpos = epos / 3;
    rpos = epos * 2 / 3;
    g.offset = 0;

    g.i_cont = true;
    g.i_broken = false;
    g.i_quote = false;
    g.cm_next = false;
    g.cm_disp_next = -1;
    g.need_redraw = false;

    wc_char_conv_init(wc_guess_8bit_charset(DisplayCharset), InnerCharset);
    do {
        x = calcPosition(g.strBuf->ptr, g.strProp, g.CLen, g.CPos, 0, CP_FORCE);
        if (x - rpos > g.offset) {
            y = calcPosition(g.strBuf->ptr, g.strProp, g.CLen, g.CLen, 0, CP_AUTO);
            if (y - epos > x - rpos)
                g.offset = x - rpos;
            else if (y - epos > 0)
                g.offset = y - epos;
        } else if (x - lpos < g.offset) {
            if (x - lpos > 0)
                g.offset = x - lpos;
            else
                g.offset = 0;
        }
        move((LINES - 1), 0);
        addstr(prompt);
        if (g.is_passwd)
            addPasswd(g.strBuf->ptr, g.strProp, g.CLen, g.offset, COLS - opos);
        else
            addStr(g.strBuf->ptr, g.strProp, g.CLen, g.offset, COLS - opos);
        clrtoeolx();
        move((LINES - 1), opos + x - g.offset);
        refresh();

    next_char:
        args->ch = getch(args);
        g.cm_clear = true;
        g.cm_disp_clear = true;
        if (!g.i_quote && (((g.cm_mode & CPL_ALWAYS) && (args->ch == CTRL_I || (space_autocomplete && args->ch == ' '))) || ((g.cm_mode & CPL_ON) && (args->ch == CTRL_I)))) {
            if (emacs_like_lineedit && g.cm_next) {
                _dcompl(args, &g);
                g.need_redraw = true;
            } else {
                _compl(args, &g);
                g.cm_disp_next = -1;
            }
        } else if (!g.i_quote && g.CLen == g.CPos && (g.cm_mode & CPL_ALWAYS || g.cm_mode & CPL_ON) && args->ch == CTRL_D) {
            if (!emacs_like_lineedit) {
                _dcompl(args, &g);
                g.need_redraw = true;
            }
        } else if (!g.i_quote && args->ch == DEL_CODE) {
            _bs(args, &g);
            g.cm_next = false;
            g.cm_disp_next = -1;
        } else if (!g.i_quote && args->ch < 0x20) { /* Control code */
            if (incrfunc == NULL
                || (args->ch = incrfunc(args, g.strBuf->ptr, g.strProp)) < 0x20) {
                (*InputKeymap[args->ch])(args, &g);
            }
            if (incrfunc && args->ch != (unsigned char)-1 && args->ch != CTRL_J)
                incrfunc(args, g.strBuf->ptr, g.strProp);
            if (g.cm_clear)
                g.cm_next = false;
            if (g.cm_disp_clear)
                g.cm_disp_next = -1;
        } else {
            tmp = Strnew_wc_output(wc_char_conv(WcOption, args->ch));
            if (tmp == NULL) {
                g.i_quote = true;
                goto next_char;
            }
            g.i_quote = false;
            g.cm_next = false;
            g.cm_disp_next = -1;
            if (g.CLen + tmp->length > STR_LEN || !tmp->length)
                goto next_char;
            ins_char(args, tmp);
            if (incrfunc)
                incrfunc(args, g.strBuf->ptr, g.strProp);
        }
        if (g.CLen && (flag & IN_CHAR))
            break;
    } while (g.i_cont);

    if (CurrentTab) {
        if (g.need_redraw)
            displayBuffer(args, B_FORCE_REDRAW);
    }

    if (g.i_broken)
        return NULL;

    move((LINES - 1), 0);
    refresh();
    p = g.strBuf->ptr;
    if (flag & (IN_FILENAME | IN_COMMAND)) {
        SKIP_BLANKS(p);
    }
    if (use_hist && !(flag & IN_URL) && *p != '\0') {
        const char* q = lastHist(hist);
        if (!q || strcmp(q, p))
            pushHist(hist, p);
    }
    if (flag & IN_FILENAME)
        return expandPath(p);
    else
        return allocStr(p, -1);
}

static void
addPasswd(char* p, Lineprop* pr, int len, int offset, int limit)
{
    int rcol = 0, ncol;

    ncol = calcPosition(p, pr, len, len, 0, CP_AUTO);
    if (ncol > offset + limit)
        ncol = offset + limit;
    if (offset) {
        addChar('{', 0);
        rcol = offset + 1;
    }
    for (; rcol < ncol; rcol++)
        addChar('*', 0);
}

static void
addStr(char* p, Lineprop* pr, int len, int offset, int limit)
{
    int i = 0, rcol = 0, ncol, delta = 1;

    if (offset) {
        for (i = 0; i < len; i++) {
            if (calcPosition(p, pr, len, i, 0, CP_AUTO) > offset)
                break;
        }
        if (i >= len)
            return;
        while (pr[i] & PC_WCHAR2)
            i++;
        addChar('{', 0);
        rcol = offset + 1;
        ncol = calcPosition(p, pr, len, i, 0, CP_AUTO);
        for (; rcol < ncol; rcol++)
            addChar(' ', 0);
    }
    for (; i < len; i += delta) {
        delta = wtf_len((wc_uchar*)&p[i]);
        ncol = calcPosition(p, pr, len, i + delta, 0, CP_AUTO);
        if (ncol - offset > limit)
            break;
        if (p[i] == '\t') {
            for (; rcol < ncol; rcol++)
                addChar(' ', 0);
            continue;
        } else {
            addMChar(&p[i], pr[i], delta);
        }
        rcol = ncol;
    }
}

static void
ins_char(struct CmdArgs* args, Str str)
{
    char *p = str->ptr, *ep = p + str->length;
    Lineprop ctype;
    int len;

    if (g.CLen + str->length >= STR_LEN)
        return;
    while (p < ep) {
        len = get_mclen(p);
        ctype = get_mctype(p);
        if (g.is_passwd) {
            if (ctype & PC_CTRL)
                ctype = PC_ASCII;
            if (ctype & PC_UNKNOWN)
                ctype = PC_WCHAR1;
        }
        insC(args, &g);
        g.strBuf->ptr[g.CPos] = *(p++);
        g.strProp[g.CPos] = ctype;
        g.CPos++;
        if (--len) {
            ctype = (ctype & ~PC_WCHAR1) | PC_WCHAR2;
            while (len--) {
                insC(args, &g);
                g.strBuf->ptr[g.CPos] = *(p++);
                g.strProp[g.CPos] = ctype;
                g.CPos++;
            }
        }
    }
}

int _esc(struct CmdArgs* args, struct LineInput *li)
{
    args->ch = getch(args);
    switch (args->ch) {
    case '[':
    case 'O':
        switch (args->ch = getch(args)) {
        case 'A':
            _prev(args, li);
            break;
        case 'B':
            _next(args, li);
            break;
        case 'C':
            _mvR(args, li);
            break;
        case 'D':
            _mvL(args, li);
            break;
        }
        break;
    case CTRL_I:
    case ' ':
        if (emacs_like_lineedit) {
            _rdcompl(args, li);
            g.cm_clear = false;
            g.need_redraw = true;
        } else
            _rcompl(args, li);
        break;
    case CTRL_D:
        if (!emacs_like_lineedit)
            _rdcompl(args, li);
        g.need_redraw = true;
        break;
    case 'f':
        if (emacs_like_lineedit)
            _mvRw(args, li);
        break;
    case 'b':
        if (emacs_like_lineedit)
            _mvLw(args, li);
        break;
    case CTRL_H:
        if (emacs_like_lineedit)
            _bsw(args, li);
        break;
    default:
        if (wc_char_conv(WcOption, ESC_CODE).data == NULL && wc_char_conv(WcOption, args->ch).data == NULL)
            g.i_quote = true;
    }

    return 0;
}

int insC(struct CmdArgs* args, struct LineInput *li)
{
    Strinsert_char(g.strBuf, g.CPos, ' ');
    g.CLen = g.strBuf->length;
    for (int i = g.CLen; i > g.CPos; i--) {
        g.strProp[i] = g.strProp[i - 1];
    }
    return 0;
}

int delC(struct CmdArgs* args, struct LineInput *li)
{
    if (g.CLen == g.CPos)
        return 0;

    int i = g.CPos;
    int delta = 1;
    while (i + delta < g.CLen && g.strProp[i + delta] & PC_WCHAR2)
        delta++;
    for (i = g.CPos; i < g.CLen; i++) {
        g.strProp[i] = g.strProp[i + delta];
    }
    Strdelete(g.strBuf, g.CPos, delta);
    g.CLen -= delta;
    return 0;
}

int _mvL(struct CmdArgs* args, struct LineInput *li)
{
    if (g.CPos > 0)
        g.CPos--;
    while (g.CPos > 0 && g.strProp[g.CPos] & PC_WCHAR2)
        g.CPos--;
    return 0;
}

int _mvLw(struct CmdArgs* args, struct LineInput *li)
{
    int first = 1;
    while (g.CPos > 0 && (first || !terminated(g.strBuf->ptr[g.CPos - 1]))) {
        g.CPos--;
        first = 0;
        if (g.CPos > 0 && g.strProp[g.CPos] & PC_WCHAR2)
            g.CPos--;
        if (!g.move_word)
            break;
    }
    return 0;
}

int _mvRw(struct CmdArgs* args, struct LineInput *li)
{
    int first = 1;
    while (g.CPos < g.CLen && (first || !terminated(g.strBuf->ptr[g.CPos - 1]))) {
        g.CPos++;
        first = 0;
        if (g.CPos < g.CLen && g.strProp[g.CPos] & PC_WCHAR2)
            g.CPos++;
        if (!g.move_word)
            break;
    }
    return 0;
}

int _mvR(struct CmdArgs* args, struct LineInput *li)
{
    if (g.CPos < g.CLen)
        g.CPos++;
    while (g.CPos < g.CLen && g.strProp[g.CPos] & PC_WCHAR2)
        g.CPos++;
    return 0;
}

int _bs(struct CmdArgs* args, struct LineInput *li)
{
    if (g.CPos > 0) {
        _mvL(args, li);
        delC(args, li);
    }
    return 0;
}

int _bsw(struct CmdArgs* args, struct LineInput *li)
{
    int t = 0;
    while (g.CPos > 0 && !t) {
        _mvL(args, li);
        t = (g.move_word && terminated(g.strBuf->ptr[g.CPos - 1]));
        delC(args, li);
    }
    return 0;
}

int _enter(struct CmdArgs* args, struct LineInput *li)
{
    g.i_cont = false;
    return 0;
}

int iself(struct CmdArgs* args, struct LineInput *li)
{
    if (g.CLen >= STR_LEN)
        return 0;
    insC(args, li);
    g.strBuf->ptr[g.CPos] = args->ch;
    g.strProp[g.CPos] = (g.is_passwd) ? PC_ASCII : PC_CTRL;
    g.CPos++;
    return 0;
}

int _quo(struct CmdArgs* args, struct LineInput *li)
{
    g.i_quote = true;
    return 0;
}

int _mvB(struct CmdArgs* args, struct LineInput *li)
{
    g.CPos = 0;
    return 0;
}

int _mvE(struct CmdArgs* args, struct LineInput *li)
{
    g.CPos = g.CLen;
    return 0;
}

int killn(struct CmdArgs* args, struct LineInput *li)
{
    g.CLen = g.CPos;
    Strtruncate(g.strBuf, g.CLen);
    return 0;
}

int killb(struct CmdArgs* args, struct LineInput *li)
{
    while (g.CPos > 0)
        _bs(args, li);
    return 0;
}

int _inbrk(struct CmdArgs* args, struct LineInput *li)
{
    g.i_cont = false;
    g.i_broken = true;
    return 0;
}

int _compl(struct CmdArgs* args, struct LineInput *li)
{
    next_compl(1);
    return 0;
}

int _rcompl(struct CmdArgs* args, struct LineInput *li)
{
    next_compl(-1);
    return 0;
}

int _tcompl(struct CmdArgs* args, struct LineInput *li)
{
    if (g.cm_mode & CPL_OFF)
        g.cm_mode = CPL_ON;
    else if (g.cm_mode & CPL_ON)
        g.cm_mode = CPL_OFF;
    return 0;
}

static void
next_compl(int next)
{
    enum CompletionStatus status;
    int b, a;
    Str buf;
    Str s;

    if (g.cm_mode == CPL_NEVER || g.cm_mode & CPL_OFF)
        return;
    g.cm_clear = false;
    if (!g.cm_next) {
        if (g.cm_mode & CPL_ALWAYS) {
            b = 0;
        } else {
            for (b = g.CPos - 1; b >= 0; b--) {
                if ((g.strBuf->ptr[b] == ' ' || g.strBuf->ptr[b] == CTRL_I) && !((b > 0) && g.strBuf->ptr[b - 1] == '\\'))
                    break;
            }
            b++;
        }
        a = g.CPos;
        CBeforeBuf = Strsubstr(g.strBuf, 0, b);
        buf = Strsubstr(g.strBuf, b, a - b);
        CAfterBuf = Strsubstr(g.strBuf, a, g.strBuf->length - a);
        s = doComplete(buf, &status, next);
    } else {
        s = doComplete(g.strBuf, &status, next);
    }
    if (next == 0)
        return;

    if (status != CPL_OK && status != CPL_MENU)
        bell();
    if (status == CPL_FAIL)
        return;

    g.strBuf = Strnew_m_charp(CBeforeBuf->ptr, s->ptr, CAfterBuf->ptr, NULL);
    setStrType(&g);
    g.CPos = CBeforeBuf->length + s->length;
    if (g.CPos > g.CLen)
        g.CPos = g.CLen;
}

int _dcompl(struct CmdArgs* args, struct LineInput *li)
{
    next_dcompl(args, 1);
    return 0;
}

int _rdcompl(struct CmdArgs* args, struct LineInput *li)
{
    next_dcompl(args, -1);
    return 0;
}

static void
next_dcompl(struct CmdArgs* args, int next)
{
    static int col, row;
    static unsigned int len;
    static Str d;
    int i, j, n, y;
    Str f;
    char* p;
    struct stat st;
    int comment, nline;

    if (g.cm_mode == CPL_NEVER || g.cm_mode & CPL_OFF)
        return;
    g.cm_disp_clear = false;
    if (CurrentTab)
        displayBuffer(args, B_FORCE_REDRAW);
    if ((LINES - 1) >= 3) {
        comment = true;
        nline = (LINES - 1) - 2;
    } else if ((LINES - 1)) {
        comment = false;
        nline = (LINES - 1);
    } else {
        return;
    }

    if (g.cm_disp_next >= 0) {
        if (next == 1) {
            g.cm_disp_next += col * nline;
            if (g.cm_disp_next >= NCFileBuf)
                g.cm_disp_next = 0;
        } else if (next == -1) {
            g.cm_disp_next -= col * nline;
            if (g.cm_disp_next < 0)
                g.cm_disp_next = 0;
        }
        row = (NCFileBuf - g.cm_disp_next + col - 1) / col;
        goto disp_next;
    }

    g.cm_next = false;
    next_compl(0);
    if (NCFileBuf == 0)
        return;
    g.cm_disp_next = 0;

    d = Str_conv_to_system(CDirBuf->ptr, CDirBuf->length);
    if (d->length > 0 && Strlastchar(d) != '/')
        Strcat_char(d, '/');
    if (g.cm_mode & CPL_URL && d->ptr[0] == 'f') {
        p = d->ptr;
        if (strncmp(p, "file://localhost/", 17) == 0)
            p = &p[16];
        else if (strncmp(p, "file:///", 8) == 0)
            p = &p[7];
        else if (strncmp(p, "file:/", 6) == 0 && p[6] != '/')
            p = &p[5];
        d = Strnew_charp(p);
    }

    len = 0;
    for (i = 0; i < NCFileBuf; i++) {
        n = strlen(CFileBuf[i]) + 3;
        if (len < n)
            len = n;
    }
    if (len > 0 && COLS > len)
        col = COLS / len;
    else
        col = 1;
    row = (NCFileBuf + col - 1) / col;

disp_next:
    if (comment) {
        if (row > nline) {
            row = nline;
            y = 0;
        } else
            y = nline - row + 1;
    } else {
        if (row >= nline) {
            row = nline;
            y = 0;
        } else
            y = nline - row - 1;
    }
    if (y) {
        move(y - 1, 0);
        clrtoeolx();
    }
    if (comment) {
        move(y, 0);
        clrtoeolx();
        bold();
        /* FIXME: gettextize? */
        addstr("----- Completion list -----");
        boldend();
        y++;
    }
    for (i = 0; i < row; i++) {
        for (j = 0; j < col; j++) {
            n = g.cm_disp_next + j * row + i;
            if (n >= NCFileBuf)
                break;
            move(y, j * len);
            clrtoeolx();
            f = Strdup(d);
            Strcat_charp(f, CFileBuf[n]);
            addstr(conv_from_system(CFileBuf[n]));
            if (stat(expandPath(f->ptr), &st) != -1 && S_ISDIR(st.st_mode))
                addstr("/");
        }
        y++;
    }
    if (comment && y == (LINES - 1) - 1) {
        move(y, 0);
        clrtoeolx();
        bold();
        if (emacs_like_lineedit)
            /* FIXME: gettextize? */
            addstr("----- Press TAB to continue -----");
        else
            /* FIXME: gettextize? */
            addstr("----- Press CTRL-D to continue -----");
        boldend();
    }
}

static Str
escape_spaces(Str s)
{
    Str tmp = NULL;
    char* p;

    if (s == NULL)
        return s;
    for (p = s->ptr; *p; p++) {
        if (*p == ' ' || *p == CTRL_I) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(s->ptr, (int)(p - s->ptr));
            Strcat_char(tmp, '\\');
        }
        if (tmp)
            Strcat_char(tmp, *p);
    }
    if (tmp)
        return tmp;
    return s;
}

Str unescape_spaces(Str s)
{
    Str tmp = NULL;
    char* p;

    if (s == NULL)
        return s;
    for (p = s->ptr; *p; p++) {
        if (*p == '\\' && (*(p + 1) == ' ' || *(p + 1) == CTRL_I)) {
            if (tmp == NULL)
                tmp = Strnew_charp_n(s->ptr, (int)(p - s->ptr));
        } else {
            if (tmp)
                Strcat_char(tmp, *p);
        }
    }
    if (tmp)
        return tmp;
    return s;
}

static Str
doComplete(Str ifn, enum CompletionStatus* status, int next)
{
    int fl, i;
    char *fn, *p;
    DIR* d;
    Directory* dir;
    struct stat st;

    if (!g.cm_next) {
        NCFileBuf = 0;
        ifn = Str_conv_to_system(ifn->ptr, ifn->length);
        if (g.cm_mode & CPL_ON)
            ifn = unescape_spaces(ifn);
        CompleteBuf = Strdup(ifn);
        while (Strlastchar(CompleteBuf) != '/' && CompleteBuf->length > 0)
            Strshrink(CompleteBuf, 1);
        CDirBuf = Strdup(CompleteBuf);
        if (g.cm_mode & CPL_URL) {
            if (strncmp(CompleteBuf->ptr, "file://localhost/", 17) == 0)
                Strdelete(CompleteBuf, 0, 16);
            else if (strncmp(CompleteBuf->ptr, "file:///", 8) == 0)
                Strdelete(CompleteBuf, 0, 7);
            else if (strncmp(CompleteBuf->ptr, "file:/", 6) == 0 && CompleteBuf->ptr[6] != '/')
                Strdelete(CompleteBuf, 0, 5);
            else {
                CompleteBuf = Strdup(ifn);
                *status = CPL_FAIL;
                return Str_conv_to_system(CompleteBuf->ptr, CompleteBuf->length);
            }
        }
        if (CompleteBuf->length == 0) {
            Strcat_char(CompleteBuf, '.');
        }
        if (Strlastchar(CompleteBuf) == '/' && CompleteBuf->length > 1) {
            Strshrink(CompleteBuf, 1);
        }
        if ((d = opendir(expandPath(CompleteBuf->ptr))) == NULL) {
            CompleteBuf = Strdup(ifn);
            *status = CPL_FAIL;
            if (g.cm_mode & CPL_ON)
                CompleteBuf = escape_spaces(CompleteBuf);
            return CompleteBuf;
        }
        fn = lastFileName(ifn->ptr);
        fl = strlen(fn);
        CFileName = Strnew();
        for (;;) {
            dir = readdir(d);
            if (dir == NULL)
                break;
            if (fl == 0
                && (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")))
                continue;
            if (!strncmp(dir->d_name, fn, fl)) { /* match */
                NCFileBuf++;
                CFileBuf = New_Reuse(char*, CFileBuf, NCFileBuf);
                CFileBuf[NCFileBuf - 1] = NewAtom_N(char, strlen(dir->d_name) + 1);
                strcpy(CFileBuf[NCFileBuf - 1], dir->d_name);
                if (NCFileBuf == 1) {
                    CFileName = Strnew_charp(dir->d_name);
                } else {
                    for (i = 0; CFileName->ptr[i] == dir->d_name[i]; i++)
                        ;
                    Strtruncate(CFileName, i);
                }
            }
        }
        closedir(d);
        if (NCFileBuf == 0) {
            CompleteBuf = Strdup(ifn);
            *status = CPL_FAIL;
            if (g.cm_mode & CPL_ON)
                CompleteBuf = escape_spaces(CompleteBuf);
            return CompleteBuf;
        }
        qsort(CFileBuf, NCFileBuf, sizeof(CFileBuf[0]), strCmp);
        NCFileOffset = 0;
        if (NCFileBuf >= 2) {
            g.cm_next = true;
            *status = CPL_AMBIG;
        } else {
            *status = CPL_OK;
        }
    } else {
        CFileName = Strnew_charp(CFileBuf[NCFileOffset]);
        NCFileOffset = (NCFileOffset + next + NCFileBuf) % NCFileBuf;
        *status = CPL_MENU;
    }
    CompleteBuf = Strdup(CDirBuf);
    if (CompleteBuf->length && Strlastchar(CompleteBuf) != '/')
        Strcat_char(CompleteBuf, '/');
    Strcat(CompleteBuf, CFileName);
    if (*status != CPL_AMBIG) {
        p = CompleteBuf->ptr;
        if (g.cm_mode & CPL_URL) {
            if (strncmp(p, "file://localhost/", 17) == 0)
                p = &p[16];
            else if (strncmp(p, "file:///", 8) == 0)
                p = &p[7];
            else if (strncmp(p, "file:/", 6) == 0 && p[6] != '/')
                p = &p[5];
        }
        if (stat(expandPath(p), &st) != -1 && S_ISDIR(st.st_mode))
            Strcat_char(CompleteBuf, '/');
    }
    if (g.cm_mode & CPL_ON)
        CompleteBuf = escape_spaces(CompleteBuf);
    return Str_conv_from_system(CompleteBuf->ptr, CompleteBuf->length);
}

int _prev(struct CmdArgs* args, struct LineInput *li)
{
    enum HistoryType hist = CurrentHist;
    if (!use_hist)
        return 0;
    const char* p;
    if (strCurrentBuf) {
        p = prevHist(hist);
        if (p == NULL)
            return 0;
    } else {
        p = lastHist(hist);
        if (p == NULL)
            return 0;
        strCurrentBuf = g.strBuf;
    }
    if (DecodeURL && (g.cm_mode & CPL_URL))
        p = url_decode2(p, NULL);
    g.strBuf = Strnew_charp(p);
    setStrType(&g);
    g.offset = 0;
    return 0;
}

int _next(struct CmdArgs* args, struct LineInput *li)
{
    enum HistoryType hist = CurrentHist;

    if (!use_hist)
        return 0;

    if (strCurrentBuf == NULL)
        return 0;

    const char* p = nextHist(hist);
    if (p) {
        if (DecodeURL && (g.cm_mode & CPL_URL))
            p = url_decode2(p, NULL);
        g.strBuf = Strnew_charp(p);
    } else {
        g.strBuf = strCurrentBuf;
        strCurrentBuf = NULL;
    }
    setStrType(&g);
    g.offset = 0;
    return 0;
}

static int
terminated(unsigned char c)
{
    int termchar[] = { '/', '&', '?', ' ', -1 };
    int* tp;

    for (tp = termchar; *tp > 0; tp++) {
        if (c == *tp) {
            return 1;
        }
    }

    return 0;
}

int _editor(struct CmdArgs* args, struct LineInput *li)
{
    if (g.is_passwd)
        return 0;

    struct FormItem fi;
    fi.readonly = false;
    fi.value = Strdup(g.strBuf);
    Strcat_char(fi.value, '\n');

    input_textarea(args, &fi);

    g.strBuf = Strnew();
    for (char* p = fi.value->ptr; *p; p++) {
        if (*p == '\r' || *p == '\n')
            continue;
        Strcat_char(g.strBuf, *p);
    }
    setStrType(&g);
    if (CurrentTab)
        displayBuffer(args, B_FORCE_REDRAW);
    return 0;
}

char* inputAnswer(struct CmdArgs* args, const char* prompt)
{
    if (QuietMessage)
        return "n";

    if (fmInitialized) {
        term_raw();
        return inputChar(args, prompt);
    } else {
        printf("%s", prompt);
        fflush(stdout);
        return Strfgets(stdin)->ptr;
    }
}
