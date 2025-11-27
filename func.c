#include "func.h"
#include "tui.h"
#include "textlist.h"
#include "regex.h"
#include "rc.h"
#include "display.h"
#include "keybind.h"

#include <gcstr/gcstr.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "funcheader.h"
#include "funcname.c"
#include "functable.c"

#define KEYMAP_FILE "keymap"
char* keymap_file = KEYMAP_FILE;
#define KEYDATA_HASH_SIZE 16
static Hash_iv* keyData = NULL;
static char keymap_initialized = false;
static struct stat sys_current_keymap_file;
static struct stat current_keymap_file;

void setKeymap(char* p, int lineno, int verbose)
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
        tui_record_err_message(emsg);
        if (verbose)
            tui_disp_message_nsec(emsg, false, 1, true, false);
        return;
    }

    s = getWord(&p);
    int f = getFuncList(s);
    if (f < 0) {
        char* emsg;
        if (lineno > 0)
            /* FIXME: gettextize? */
            emsg = Sprintf("line %d: invalid command '%s'", lineno, s)->ptr;
        else
            /* FIXME: gettextize? */
            emsg = Sprintf("defkey: invalid command '%s'", s)->ptr;
        tui_record_err_message(emsg);
        if (verbose)
            tui_disp_message_nsec(emsg, false, 1, true, false);
        return;
    }

    unsigned char* map = NULL;
    if (c & K_MULTI) {
        unsigned char** mmap = NULL;
        int i, j, m = MULTI_KEY(c);

        if (m & K_ESCD)
            map = EscDKeymap;
        else if (m & K_ESCB)
            map = EscBKeymap;
        else if (m & K_ESC)
            map = EscKeymap;
        else
            map = GlobalKeymap;
        if (map[m & 0x7F] == FUNCNAME_multimap)
            mmap = (unsigned char**)getKeyData(m);
        else
            map[m & 0x7F] = FUNCNAME_multimap;
        if (!mmap) {
            mmap = New_N(unsigned char*, 4);
            for (i = 0; i < 4; i++) {
                mmap[i] = New_N(unsigned char, 128);
                for (j = 0; j < 128; j++)
                    mmap[i][j] = FUNCNAME_nulcmd;
            }
            mmap[0][ESC_CODE] = FUNCNAME_escmap;
            mmap[1]['['] = FUNCNAME_escbmap;
            mmap[1]['O'] = FUNCNAME_escbmap;
        }
        if (keyData == NULL)
            keyData = newHash_iv(KEYDATA_HASH_SIZE);
        putHash_iv(keyData, m, (void*)mmap);
        if (c & K_ESCD)
            map = mmap[3];
        else if (c & K_ESCB)
            map = mmap[2];
        else if (c & K_ESC)
            map = mmap[1];
        else
            map = mmap[0];
    } else {
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
interpret_keymap(FILE* kf, struct stat* current, bool force, wc_ces charset, wc_ces inner_charset)
{
    int fd;
    struct stat kstat;
    Str line;
    char *p, *s, *emsg;
    int lineno;
    int verbose = 1;
    extern int str_to_bool(char* value, int old);

    if ((fd = fileno(kf)) < 0 || fstat(fd, &kstat) || (!force && kstat.st_mtime == current->st_mtime && kstat.st_dev == current->st_dev && kstat.st_ino == current->st_ino && kstat.st_size == current->st_size))
        return;
    *current = kstat;

    lineno = 0;
    while (!feof(kf)) {
        line = Strfgets(kf);
        lineno++;
        Strchop(line);
        Strremovefirstspaces(line);
        if (line->length == 0)
            continue;
        line = wc_Str_conv(line, charset, inner_charset);
        p = line->ptr;
        s = getWord(&p);
        if (*s == '#') /* comment */
            continue;
        if (!strcmp(s, "keymap"))
            ;
        else if (!strcmp(s, "charset") || !strcmp(s, "encoding")) {
            s = getQWord(&p);
            if (*s)
                charset = wc_guess_charset(s, charset);
            continue;
        } else if (!strcmp(s, "verbose")) {
            s = getWord(&p);
            if (*s)
                verbose = str_to_bool(s, verbose);
            continue;
        } else { /* error */
            emsg = Sprintf("line %d: syntax error '%s'", lineno, s)->ptr;
            tui_record_err_message(emsg);
            if (verbose)
                tui_disp_message_nsec(emsg, false, 1, true, false);
            continue;
        }
        setKeymap(p, lineno, verbose);
    }
}

void initKeymap(wc_ces charset, wc_ces inner_charset, bool force)
{
    FILE* kf;
    if ((kf = fopen(confFile(KEYMAP_FILE), "rt")) != NULL) {
        interpret_keymap(kf, &sys_current_keymap_file,
            force || !keymap_initialized, charset, inner_charset);
        fclose(kf);
    }
    if ((kf = fopen(rcFile(keymap_file), "rt")) != NULL) {
        interpret_keymap(kf, &current_keymap_file,
            force || !keymap_initialized, charset, inner_charset);
        fclose(kf);
    }
    keymap_initialized = true;
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

int getKey(const char* s)
{
    int c = getKey2(&s);
    if (c < 0)
        return -1;
    if (*s == ' ' || *s == '-')
        s++;
    if (*s) {
        int c2 = getKey2(&s);
        if (c2 < 0)
            return -1;
        c = K_MULTI | (c << 16) | c2;
    }
    return c;
}

/* This extracts /regex/i or m@regex@i from the given string.
 * Then advances *str to the end of regex.
 * If the input does not seems to be a regex, this falls back to getQWord().
 *
 * Returns a word (no matter whether regex or not) in the give string.
 * If regex_ret is non-NULL, compiles the regex and stores there.
 *
 * XXX: Actually this is unrelated to func.c.
 */
char* getRegexWord(const char** str, Regex** regex_ret)
{
    char* word = NULL;
    const char *p, *headp, *bodyp, *tailp;
    char delimiter;
    int esc;
    int igncase = 0;

    p = *str;
    SKIP_BLANKS(&p);
    headp = p;

    /* Get the opening delimiter */
    if (p[0] == 'm' && IS_PRINT(p[1]) && !IS_ALNUM(p[1]) && p[1] != '\\') {
        delimiter = p[1];
        p += 2;
    } else if (p[0] == '/') {
        delimiter = '/';
        p += 1;
    } else {
        goto not_regex;
    }
    bodyp = p;

    /* Scan the end of the expression */
    for (esc = 0; *p; ++p) {
        if (esc) {
            esc = 0;
        } else {
            if (*p == delimiter)
                break;
            else if (*p == '\\')
                esc = 1;
        }
    }
    if (!*p && *headp == '/')
        goto not_regex;
    tailp = p;

    /* Check the modifiers */
    if (*p == delimiter) {
        while (*++p && !IS_SPACE(*p)) {
            switch (*p) {
            case 'i':
                igncase = 1;
                break;
            }
            /* ignore unknown modifiers */
        }
    }

    /* Save the expression */
    word = allocStr(headp, p - headp);

    /* Compile */
    if (regex_ret) {
        if (*tailp == delimiter)
            word[tailp - headp] = 0;
        *regex_ret = newRegex(word + (bodyp - headp), igncase, NULL, NULL);
        if (*tailp == delimiter)
            word[tailp - headp] = delimiter;
    }
    goto last;

not_regex:
    p = headp;
    word = getQWord((char**)&p);
    if (regex_ret)
        *regex_ret = NULL;

last:
    *str = p;
    return word;
}
