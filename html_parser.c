#include "html_parser.h"
#include "html.h"
#include "myctype.h"
#include "hash.h"

#define MAX_CMD_LEN 128

extern Hash_si tagtable;

int gethtmlcmd(const char* s)
{
    char cmdstr[MAX_CMD_LEN];
    char* p = cmdstr;
    int cmd;

    s++;
    /* first character */
    if (IS_ALNUM(*s) || *s == '_' || *s == '/') {
        *(p++) = TOLOWER(*s);
        s++;
    } else
        return HTML_UNKNOWN;
    if (p[-1] == '/')
        SKIP_BLANKS(s);
    while ((IS_ALNUM(*s) || *s == '_') && p - cmdstr < MAX_CMD_LEN) {
        *(p++) = TOLOWER(*s);
        s++;
    }
    if (p - cmdstr == MAX_CMD_LEN) {
        /* buffer overflow: perhaps caused by bad HTML source */
        return HTML_UNKNOWN;
    }
    *p = '\0';

    /* hash search */
    cmd = getHash_si(&tagtable, cmdstr, HTML_UNKNOWN);
    while (*s && *s != '>')
        s++;
    if (*s == '>')
        s++;
    return cmd;
}

int next_status(const char c, int* status)
{
    switch (*status) {
    case R_ST_NORMAL:
        if (c == '<') {
            *status = R_ST_TAG0;
            return 0;
        } else if (c == '&') {
            *status = R_ST_AMP;
            return 1;
        } else
            return 1;
        break;
    case R_ST_TAG0:
        if (c == '!') {
            *status = R_ST_CMNT1;
            return 0;
        }
        *status = R_ST_TAG;
        /* continues to next case */
    case R_ST_TAG:
        if (c == '>')
            *status = R_ST_NORMAL;
        else if (c == '=')
            *status = R_ST_EQL;
        return 0;
    case R_ST_EQL:
        if (c == '"')
            *status = R_ST_DQUOTE;
        else if (c == '\'')
            *status = R_ST_QUOTE;
        else if (IS_SPACE(c))
            *status = R_ST_EQL;
        else if (c == '>')
            *status = R_ST_NORMAL;
        else
            *status = R_ST_VALUE;
        return 0;
    case R_ST_QUOTE:
        if (c == '\'')
            *status = R_ST_TAG;
        return 0;
    case R_ST_DQUOTE:
        if (c == '"')
            *status = R_ST_TAG;
        return 0;
    case R_ST_VALUE:
        if (c == '>')
            *status = R_ST_NORMAL;
        else if (IS_SPACE(c))
            *status = R_ST_TAG;
        return 0;
    case R_ST_AMP:
        if (c == ';') {
            *status = R_ST_NORMAL;
            return 0;
        } else if (c != '#' && !IS_ALNUM(c) && c != '_') {
            /* something's wrong! */
            *status = R_ST_NORMAL;
            return 0;
        } else
            return 0;
    case R_ST_CMNT1:
        switch (c) {
        case '-':
            *status = R_ST_CMNT2;
            break;
        case '>':
            *status = R_ST_NORMAL;
            break;
        case 'D':
        case 'd':
            /* could be a !doctype */
            *status = R_ST_TAG;
            break;
        default:
            *status = R_ST_IRRTAG;
        }
        return 0;
    case R_ST_CMNT2:
        switch (c) {
        case '-':
            *status = R_ST_CMNT;
            break;
        case '>':
            *status = R_ST_NORMAL;
            break;
        default:
            *status = R_ST_IRRTAG;
        }
        return 0;
    case R_ST_CMNT:
        if (c == '-')
            *status = R_ST_NCMNT1;
        return 0;
    case R_ST_NCMNT1:
        if (c == '-')
            *status = R_ST_NCMNT2;
        else
            *status = R_ST_CMNT;
        return 0;
    case R_ST_NCMNT2:
        switch (c) {
        case '>':
            *status = R_ST_NORMAL;
            break;
        case '-':
            *status = R_ST_NCMNT2;
            break;
        default:
            if (IS_SPACE(c))
                *status = R_ST_NCMNT3;
            else
                *status = R_ST_CMNT;
            break;
        }
        break;
    case R_ST_NCMNT3:
        switch (c) {
        case '>':
            *status = R_ST_NORMAL;
            break;
        case '-':
            *status = R_ST_NCMNT1;
            break;
        default:
            if (IS_SPACE(c))
                *status = R_ST_NCMNT3;
            else
                *status = R_ST_CMNT;
            break;
        }
        return 0;
    case R_ST_IRRTAG:
        if (c == '>')
            *status = R_ST_NORMAL;
        return 0;
    }
    /* notreached */
    return 0;
}

int read_token(pStr buf, const char** instr, int* status, int pre, int append)
{
    int prev_status;

    if (!append)
        Strclear(buf);
    if (**instr == '\0')
        return 0;

    const char* p;
    for (p = *instr; *p; p++) {
        /* Drop Unicode soft hyphen */
        if (*(const unsigned char*)p == 0210
            && *(const unsigned char*)(p + 1) == 0200
            && *(const unsigned char*)(p + 2) == 0201
            && *(const unsigned char*)(p + 3) == 0255) {
            p += 3;
            continue;
        }

        prev_status = *status;
        next_status(*p, status);
        switch (*status) {
        case R_ST_NORMAL:
            if (prev_status == R_ST_AMP && *p != ';') {
                p--;
                break;
            }
            if (prev_status == R_ST_NCMNT2 || prev_status == R_ST_NCMNT3 || prev_status == R_ST_IRRTAG || prev_status == R_ST_CMNT1) {
                if (prev_status == R_ST_CMNT1 && !append && !pre)
                    Strclear(buf);
                if (pre)
                    Strcat_char(buf, *p);
                p++;
                goto proc_end;
            }
            Strcat_char(buf, (!pre && IS_SPACE(*p)) ? ' ' : *p);
            if (ST_IS_REAL_TAG(prev_status)) {
                *instr = p + 1;
                if (buf->len < 2 || buf->ptr[buf->len - 2] != '<' || buf->ptr[buf->len - 1] != '>')
                    return 1;
                Strshrink(buf, 2);
            }
            break;
        case R_ST_TAG0:
        case R_ST_TAG:
            if (prev_status == R_ST_NORMAL && p != *instr) {
                *instr = p;
                *status = prev_status;
                return 1;
            }
            if (*status == R_ST_TAG0 && !REALLY_THE_BEGINNING_OF_A_TAG(p)) {
                /* it seems that this '<' is not a beginning of a tag */
                Strcat_char(buf, '<');
                *status = R_ST_NORMAL;
            } else
                Strcat_char(buf, *p);
            break;
        case R_ST_EQL:
        case R_ST_QUOTE:
        case R_ST_DQUOTE:
        case R_ST_VALUE:
        case R_ST_AMP:
            Strcat_char(buf, *p);
            break;
        case R_ST_CMNT:
        case R_ST_IRRTAG:
            if (pre)
                Strcat_char(buf, *p);
            else if (!append)
                Strclear(buf);
            break;
        case R_ST_CMNT1:
        case R_ST_CMNT2:
        case R_ST_NCMNT1:
        case R_ST_NCMNT2:
        case R_ST_NCMNT3:
            /* do nothing */
            if (pre)
                Strcat_char(buf, *p);
            break;
        }
    }
proc_end:
    *instr = p;
    return 1;
}
