#include "func.h"
#include "tab.h"
#include "tab_list.h"
#include "w3m_rc.h"
#include "alloc.h"
#include "ctrlcode.h"
#include "myctype.h"
#include "etc.h"
#include "buffer.h"
#include "local_cgi.h"
#include "html_form.h"
#include "message.h"

#include <libwc/charset.h>
#include <libwc/conv.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "funcheader.h"
#include "functable.c"
#include "funcname.c"

enum KeyMapFlags : uint32_t {
    K_ESC = 0x100,
    K_ESCB = 0x200,
};

DefunFunc keymap_fromName(const char* name)
{
    for (struct FuncList* f = &w3mFuncList[0]; f; ++f) {
        if (0 == strcmp(name, f->name)) {
            return f->func;
        }
    }
    return 0;
}

// keybind.c
extern DefunFunc GlobalKeymap[];
extern DefunFunc EscKeymap[];
extern DefunFunc EscBKeymap[];
enum KeyMapTypes {
    KEYMAP_Global,
    KEYMAP_Esc,
    // escape sequence ^[[
    KEYMAP_EscB,
};

#define KEYDATA_HASH_SIZE 16
static Hash_iv* keyData = NULL;
static char keymap_initialized = false;
static struct stat sys_current_keymap_file;
static struct stat current_keymap_file;

void keyPressEventProc(int c)
{
    g_runtime.CurrentKey = c;
    struct KeyRegister f = keymap_fromKey(c);
    f.func((struct DefunContext) {
        .tab = CurrentTab(),
        .buf = CurrentTab()->currentBuffer,
    });
}

#define PREC_LIMIT 10000

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

static void set_buffer_environ(struct Buffer* buf)
{
    if (buf == NULL)
        return;

    static struct Buffer* prev_buf = NULL;
    static struct Line* prev_line = NULL;
    static int prev_pos = -1;

    if (buf != prev_buf) {
        if (buf->content) {
            set_environ("W3M_SOURCEFILE", buf->content->sourcefile);
            set_environ("W3M_FILENAME", buf->content->filename);
            set_environ("W3M_URL", parsedURL2Str(&buf->content->url)->ptr);
        }
        set_environ("W3M_TITLE", buf->doc->title);
        set_environ("W3M_CHARSET", wc_ces_to_charset(buf->doc->charset));
        set_environ("W3M_TYPE", "unknown");
    }
    struct Line* l = buf->doc->currentLine;
    if (l && (buf != prev_buf || l != prev_line || buf->doc->pos != prev_pos)) {
        struct Anchor* a;
        struct Url pu;
        const char* s = GetWord(buf);
        set_environ("W3M_CURRENT_WORD", s ? s : "");
        a = doc_retrieveCurrentAnchor(buf->doc);
        if (a) {
            parseURL2(a->url, &pu, baseURL(buf));
            set_environ("W3M_CURRENT_LINK", parsedURL2Str(&pu)->ptr);
        } else
            set_environ("W3M_CURRENT_LINK", "");
        a = doc_retrieveCurrentImg(buf->doc);
        if (a) {
            parseURL2(a->url, &pu, baseURL(buf));
            set_environ("W3M_CURRENT_IMG", parsedURL2Str(&pu)->ptr);
        } else
            set_environ("W3M_CURRENT_IMG", "");
        a = doc_retrieveCurrentForm(buf->doc);
        if (a)
            set_environ("W3M_CURRENT_FORM", form2str((struct FormItemList*)a->url));
        else
            set_environ("W3M_CURRENT_FORM", "");
        set_environ("W3M_CURRENT_LINE", Sprintf("%ld", l->real_linenumber)->ptr);
        set_environ("W3M_CURRENT_COLUMN", Sprintf("%d", buf->doc->currentColumn + buf->doc->cursorX + 1)->ptr);
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
    prev_pos = buf->doc->pos;
}

void w3m_on_key(uint8_t ch)
{
    if (IS_ASCII(ch)) {
        if (('0' <= ch) && (ch <= '9') && (g_runtime.prec_num || (GlobalKeymap[ch] == nulcmd))) {
            g_runtime.prec_num = g_runtime.prec_num * 10 + (int)(ch - '0');
            if (g_runtime.prec_num > PREC_LIMIT)
                g_runtime.prec_num = PREC_LIMIT;
        } else {
            set_buffer_environ(Currentbuf);
            doc_save_buffer_position(Currentbuf->doc);
            keyPressEventProc(ch);
            g_runtime.prec_num = 0;
        }
    }
}

static int
getKey2(const char* s)
{
    if (!s || !s[0])
        return -1;

    if (strcasecmp(s, "UP") == 0) { // ^[[A
        return K_ESCB | 'A';
    } else if (strcasecmp(s, "DOWN") == 0) { // ^[[B
        return K_ESCB | 'B';
    } else if (strcasecmp(s, "RIGHT") == 0) { // ^[[C
        return K_ESCB | 'C';
    } else if (strcasecmp(s, "LEFT") == 0) { // ^[[D
        return K_ESCB | 'D';
    }

    enum KeyMapFlags esc = 0;
    bool ctrl = false;
    if (strncasecmp(s, "ESC-", 4) == 0 || strncasecmp(s, "ESC ", 4) == 0) { // ^[
        s += 4;
        esc = K_ESC;
    } else if (strncasecmp(s, "M-", 2) == 0 || strncasecmp(s, "\\E", 2) == 0) { // ^[
        s += 2;
        esc = K_ESC;
    } else if (*s == ESC_CODE) { // ^[
        s++;
        esc = K_ESC;
    }
    if (strncasecmp(s, "C-", 2) == 0) { // ^, ^[^
        s += 2;
        ctrl = true;
    } else if (*s == '^' && *(s + 1)) { // ^, ^[^
        s++;
        ctrl = true;
    }
    if (!esc && ctrl && *s == '[') { // ^[
        s++;
        ctrl = false;
        esc = K_ESC;
    }
    if (esc && !ctrl) {
        if (*s == '[' || *s == 'O') { // ^[[, ^[O
            s++;
            esc = K_ESCB;
        }
        if (strncasecmp(s, "C-", 2) == 0) { // ^[^, ^[[^
            s += 2;
            ctrl = true;
        } else if (*s == '^' && *(s + 1)) { // ^[^, ^[[^
            s++;
            ctrl = true;
        }
    }

    if (ctrl) {
        if (*s >= '@' && *s <= '_') // ^@ .. ^_
            return esc | (*s - '@');
        else if (*s >= 'a' && *s <= 'z') // ^a .. ^z
            return esc | (*s - 'a' + 1);
        else if (*s == '?') // ^?
            return esc | DEL_CODE;
        else
            return -1;
    }

    if (esc == K_ESCB && IS_DIGIT(*s)) {
        int n = (int)(*s - '0');
        s++;
        if (IS_DIGIT(*s)) {
            n = n * 10 + (int)(*s - '0');
            s++;
        }
        return -1;
    }

    if (strncasecmp(s, "SPC", 3) == 0) { // ' '
        return esc | ' ';
    } else if (strncasecmp(s, "TAB", 3) == 0) { // ^i
        return esc | '\t';
    } else if (strncasecmp(s, "DEL", 3) == 0) { // ^?
        return esc | DEL_CODE;
    }

    if (*s == '\\' && *(s + 1) != '\0') {
        s++;
        switch (*s) {
        case 'a': // ^g
            return esc | CTRL_G;
        case 'b': // ^h
            return esc | CTRL_H;
        case 't': // ^i
            return esc | CTRL_I;
        case 'n': // ^j
            return esc | CTRL_J;
        case 'r': // ^m
            return esc | CTRL_M;
        case 'e': // ^[
            return esc | ESC_CODE;
        case '^': // ^
            return esc | '^';
        case '\\':
            return esc | '\\';
        default:
            return -1;
        }
    }
    if (IS_ASCII(*s)) // Ascii
        return esc | *s;
    else
        return -1;
}

void keymap_parseLine(const char* p, int lineno, bool verbose)
{
    const char* s = getQWord(&p);
    int c = getKey2(s);
    if (c < 0) { /* error */
        const char* emsg;
        if (lineno > 0)
            emsg = Sprintf("line %d: unknown key '%s'", lineno, s)->ptr;
        else
            emsg = Sprintf("defkey: unknown key '%s'", s)->ptr;
        record_err_message(emsg);
        if (verbose)
            disp_message_nsec(emsg, FALSE, 1, TRUE, FALSE);
        return;
    }
    const char* name = getWord(&p);

    // int f = getFuncList(s);
    // if (f < 0) {
    //     const char* emsg;
    //     if (lineno > 0)
    //         emsg = Sprintf("line %d: invalid command '%s'", lineno, s)->ptr;
    //     else
    //         emsg = Sprintf("defkey: invalid command '%s'", s)->ptr;
    //     record_err_message(emsg);
    //     if (verbose)
    //         disp_message_nsec(emsg, FALSE, 1, TRUE, FALSE);
    //     return;
    // }

    // map[c & 0x7F] = f;
    s = getQWord(&p);
    const char* data = 0;
    if (*s) {
        if (keyData == NULL)
            keyData = newHash_iv(KEYDATA_HASH_SIZE);
        // putHash_iv(keyData, c, (void*)s);
        data = s;
    } else if (getKeyData(c)) {
        // putHash_iv(keyData, c, NULL);
        data = 0;
    }

    keymap_register(c,
        (struct KeyRegister) {
            .func = keymap_fromName(name),
            .data = data,
        });
}

static void
keymap_load(FILE* kf, struct stat* current, int force)
{
    enum wc_ces charset = getRuntime()->SystemCharset;

    for (int i = 0; i < 128; ++i) {
        keymap_register(i,
            (struct KeyRegister) {
                .func = GlobalKeymap[i],
                .data = 0,
            });
    }
    for (int i = 0; i < 128; ++i) {
        keymap_register(i | K_ESC,
            (struct KeyRegister) {
                .func = EscKeymap[i],
                .data = 0,
            });
    }
    for (int i = 0; i < 128; ++i) {
        keymap_register(i | K_ESCB,
            (struct KeyRegister) {
                .func = EscBKeymap[i],
                .data = 0,
            });
    }

    int fd;
    struct stat kstat;
    if ((fd = fileno(kf)) < 0 || fstat(fd, &kstat))
        return;
    if ((!force
            && kstat.st_mtime == current->st_mtime
            && kstat.st_dev == current->st_dev
            && kstat.st_ino == current->st_ino
            && kstat.st_size == current->st_size))
        return;
    *current = kstat;

    int lineno = 0;
    while (!feof(kf)) {
        Str line = Strfgets(kf);
        if (line->length == 0) {
            break;
        }
        lineno++;
        Strchop(line);
        Strremovefirstspaces(line);
        if (line->length == 0)
            continue;

        line = wc_Str_conv(line, charset, getRuntime()->InnerCharset);

        const char* p = line->ptr;
        const char* s = getWord(&p);
        bool verbose = true;
        if (*s == '#') /* comment */
            continue;
        if (!strcmp(s, "keymap"))
            ;

        else if (!strcmp(s, "charset") || !strcmp(s, "encoding")) {
            s = getQWord(&p);
            if (*s)
                charset = wc_guess_charset(s, charset);
            continue;
        }

        else if (!strcmp(s, "verbose")) {
            s = getWord(&p);
            if (*s)
                verbose = str_to_bool(s, verbose);
            continue;
        } else { /* error */
            const char* emsg = Sprintf("line %d: syntax error '%s'", lineno, s)->ptr;
            record_err_message(emsg);
            if (verbose)
                disp_message_nsec(emsg, FALSE, 1, TRUE, FALSE);
            continue;
        }
        keymap_parseLine(p, lineno, verbose);
    }
}

void keymap_init(bool force)
{
    FILE* kf;

    if ((kf = fopen(confFile(KEYMAP_FILE), "rt")) != NULL) {
        keymap_load(kf, &sys_current_keymap_file,
            force || !keymap_initialized);
        fclose(kf);
    }
    if ((kf = fopen(rcFile(getRuntime()->keymap_file), "rt")) != NULL) {
        keymap_load(kf, &current_keymap_file,
            force || !keymap_initialized);
        fclose(kf);
    }
    keymap_initialized = TRUE;
}

int getFuncList(const char* id)
{
    return getHash_si(&functable, id, -1);
}

char* getKeyData(int key)
{
    if (keyData == NULL)
        return NULL;
    return (char*)getHash_iv(keyData, key, NULL);
}
