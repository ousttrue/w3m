#include "keymap.h"
#include "etc.h"
#include "myctype.h"
#include "rc.h"
#include "ctrlcode.h"
#include "ui.h"
#include "indep.h"
#include "istream.h"
#include "history.h"
#include "quote.h"

#include <Str.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "defun.h"

#define KEYMAP_FILE "keymap"
char* keymap_file = (KEYMAP_FILE);

struct FuncList w3mFuncList[] = {
#include "funcnamemap.h"
};

#define KEYDATA_HASH_SIZE 16
static Hash_iv* keyData = NULL;
static bool keymap_initialized = false;
static struct stat sys_current_keymap_file;
static struct stat current_keymap_file;

void setKeymap(char* p, int lineno)
{
    char* s = getQWord(&p);
    int c = getKey(s);
    if (c < 0) { /* error */
        char* emsg;
        if (lineno > 0)
            /* FIXME: gettextize? */
            emsg = Sprintf("line %d: unknown key '%s'", lineno, s)->ptr;
        else
            /* FIXME: gettextize? */
            emsg = Sprintf("defkey: unknown key '%s'", s)->ptr;
        message(getUI(), MSG_ERR, emsg);
        return;
    }
    s = getWord(&p);
    CommandFunc f = getFunc(s);
    if (f == NULL) {
        char* emsg;
        if (lineno > 0)
            /* FIXME: gettextize? */
            emsg = Sprintf("line %d: invalid command '%s'", lineno, s)->ptr;
        else
            /* FIXME: gettextize? */
            emsg = Sprintf("defkey: invalid command '%s'", s)->ptr;
        message(getUI(), MSG_ERR, emsg);
        return;
    }

    CommandFunc* map = NULL;
    // if (c & K_MULTI) {
    //     unsigned char** mmap = NULL;
    //     int i, j, m = MULTI_KEY(c);
    //
    //     if (m & K_ESCD)
    //         map = EscDKeymap;
    //     else if (m & K_ESCB)
    //         map = EscBKeymap;
    //     else if (m & K_ESC)
    //         map = EscKeymap;
    //     else
    //         map = GlobalKeymap;
    //     if (map[m & 0x7F] == FUNCNAME_multimap)
    //         mmap = (unsigned char**)getKeyData(m);
    //     else
    //         map[m & 0x7F] = FUNCNAME_multimap;
    //     if (!mmap) {
    //         mmap = New_N(unsigned char*, 4);
    //         for (i = 0; i < 4; i++) {
    //             mmap[i] = New_N(unsigned char, 128);
    //             for (j = 0; j < 128; j++)
    //                 mmap[i][j] = FUNCNAME_nulcmd;
    //         }
    //         mmap[0][ESC_CODE] = FUNCNAME_escmap;
    //         mmap[1]['['] = FUNCNAME_escbmap;
    //         mmap[1]['O'] = FUNCNAME_escbmap;
    //     }
    //     if (keyData == NULL)
    //         keyData = newHash_iv(KEYDATA_HASH_SIZE);
    //     putHash_iv(keyData, m, (void*)mmap);
    //     if (c & K_ESCD)
    //         map = mmap[3];
    //     else if (c & K_ESCB)
    //         map = mmap[2];
    //     else if (c & K_ESC)
    //         map = mmap[1];
    //     else
    //         map = mmap[0];
    // } else
    {
        if (c & K_ESCD)
            map = EscDKeymap;
        else if (c & K_ESCB)
            map = EscBKeymap;
        else if (c & K_ESC)
            map = EscKeymap;
        else
            map = GlobalKeymap;
    }
    map[c & 0x7F] = f;
    s = getQWord(&p);
    if (*s) {
        if (keyData == NULL)
            keyData = newHash_iv(KEYDATA_HASH_SIZE);
        putHash_iv(keyData, c, (void*)s);
    } else if (getKeyData(c))
        putHash_iv(keyData, c, NULL);
}

static void
interpret_keymap(FILE* kf, struct stat* current, int force)
{
    int fd = fileno(kf);
    struct stat kstat;
    if (fd < 0
        || fstat(fd, &kstat)
        || (!force && kstat.st_mtime == current->st_mtime && kstat.st_dev == current->st_dev && kstat.st_ino == current->st_ino && kstat.st_size == current->st_size)) {
        return;
    }
    *current = kstat;

    wc_ces charset = SystemCharset;
    bool verbose = true;

    for (int lineno = 1; !feof(kf); ++lineno) {
        Str line = Strfgets(kf);
        Strchop(line);
        Strremovefirstspaces(line);
        if (line->length == 0)
            continue;
        line = wc_Str_conv(line, charset, InnerCharset);

        char* p = line->ptr;

        char* s = getWord(&p);
        if (*s == '#') {
            // comment
            continue;
        }
        if (!strcmp(s, "keymap")) {
            setKeymap(p, lineno);
        } else if (!strcmp(s, "charset") || !strcmp(s, "encoding")) {
            char* q = getQWord(&p);
            if (*q)
                charset = wc_guess_charset(q, charset);
        } else if (!strcmp(s, "verbose")) {
            char* q = getWord(&p);
            if (*q)
                verbose = str_to_bool(q, verbose);
        } else { /* error */
            char* emsg = Sprintf("line %d: syntax error '%s'", lineno, s)->ptr;
            if (verbose)
                message(getUI(), MSG_ERR, emsg);
        }
    }
}

void initKeymap(int force)
{
    {
        FILE* kf = fopen(confFile(KEYMAP_FILE), "rt");
        if (kf) {
            interpret_keymap(kf, &sys_current_keymap_file, force || !keymap_initialized);
            fclose(kf);
        }
    }

    {
        FILE* kf = fopen(rcFile(keymap_file), "rt");
        if (kf) {
            interpret_keymap(kf, &current_keymap_file, force || !keymap_initialized);
            fclose(kf);
        }
    }

    keymap_initialized = true;
}

CommandFunc getFunc(const char* id)
{
    for (int i = 0; i < sizeof(w3mFuncList) / sizeof(w3mFuncList[0]); ++i) {
        if (strcmp(w3mFuncList[i].id, id) == 0) {
            return w3mFuncList[i].func;
        }
    }
    return &nulcmd;
}

char* getKeyData(int key)
{
    if (keyData == NULL)
        return NULL;
    return (char*)getHash_iv(keyData, key, NULL);
}

static int
getKey2(char** str)
{
    char* s = *str;
    int c, esc = 0, ctrl = 0;

    if (s == NULL || *s == '\0')
        return -1;

    if (strcasecmp(s, "UP") == 0) { /* ^[[A */
        *str = s + 2;
        return K_ESCB | 'A';
    } else if (strcasecmp(s, "DOWN") == 0) { /* ^[[B */
        *str = s + 4;
        return K_ESCB | 'B';
    } else if (strcasecmp(s, "RIGHT") == 0) { /* ^[[C */
        *str = s + 5;
        return K_ESCB | 'C';
    } else if (strcasecmp(s, "LEFT") == 0) { /* ^[[D */
        *str = s + 4;
        return K_ESCB | 'D';
    }

    if (strncasecmp(s, "ESC-", 4) == 0 || strncasecmp(s, "ESC ", 4) == 0) { /* ^[ */
        s += 4;
        esc = K_ESC;
    } else if (strncasecmp(s, "M-", 2) == 0 || strncasecmp(s, "\\E", 2) == 0) { /* ^[ */
        s += 2;
        esc = K_ESC;
    } else if (*s == ESC_CODE) { /* ^[ */
        s++;
        esc = K_ESC;
    }
    if (strncasecmp(s, "C-", 2) == 0) { /* ^, ^[^ */
        s += 2;
        ctrl = 1;
    } else if (*s == '^' && *(s + 1)) { /* ^, ^[^ */
        s++;
        ctrl = 1;
    }
    if (!esc && ctrl && *s == '[') { /* ^[ */
        s++;
        ctrl = 0;
        esc = K_ESC;
    }
    if (esc && !ctrl) {
        if (*s == '[' || *s == 'O') { /* ^[[, ^[O */
            s++;
            esc = K_ESCB;
        }
        if (strncasecmp(s, "C-", 2) == 0) { /* ^[^, ^[[^ */
            s += 2;
            ctrl = 1;
        } else if (*s == '^' && *(s + 1)) { /* ^[^, ^[[^ */
            s++;
            ctrl = 1;
        }
    }

    if (ctrl) {
        *str = s + 1;
        if (*s >= '@' && *s <= '_') /* ^@ .. ^_ */
            return esc | (*s - '@');
        else if (*s >= 'a' && *s <= 'z') /* ^a .. ^z */
            return esc | (*s - 'a' + 1);
        else if (*s == '?') /* ^? */
            return esc | DEL_CODE;
        else
            return -1;
    }

    if (esc == K_ESCB && IS_DIGIT(*s)) {
        c = (int)(*s - '0');
        s++;
        if (IS_DIGIT(*s)) {
            c = c * 10 + (int)(*s - '0');
            s++;
        }
        *str = s + 1;
        if (*s == '~')
            return K_ESCD | c;
        else
            return -1;
    }

    if (strncasecmp(s, "SPC", 3) == 0) { /* ' ' */
        *str = s + 3;
        return esc | ' ';
    } else if (strncasecmp(s, "TAB", 3) == 0) { /* ^i */
        *str = s + 3;
        return esc | '\t';
    } else if (strncasecmp(s, "DEL", 3) == 0) { /* ^? */
        *str = s + 3;
        return esc | DEL_CODE;
    }

    if (*s == '\\' && *(s + 1) != '\0') {
        s++;
        *str = s + 1;
        switch (*s) {
        case 'a': /* ^g */
            return esc | CTRL_G;
        case 'b': /* ^h */
            return esc | CTRL_H;
        case 't': /* ^i */
            return esc | CTRL_I;
        case 'n': /* ^j */
            return esc | CTRL_J;
        case 'r': /* ^m */
            return esc | CTRL_M;
        case 'e': /* ^[ */
            return esc | ESC_CODE;
        case '^': /* ^ */
            return esc | '^';
        case '\\': /* \ */
            return esc | '\\';
        default:
            return -1;
        }
    }
    *str = s + 1;
    if (IS_ASCII(*s)) /* Ascii */
        return esc | *s;
    else
        return -1;
}

int getKey(char* s)
{
    int c, c2;

    c = getKey2(&s);
    if (c < 0)
        return -1;
    if (*s == ' ' || *s == '-')
        s++;
    if (*s) {
        c2 = getKey2(&s);
        if (c2 < 0)
            return -1;
        c = K_MULTI | (c << 16) | c2;
    }
    return c;
}
