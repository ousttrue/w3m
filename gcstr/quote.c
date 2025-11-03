#include "quote.h"
#include "Str.h"
#include "myctype.h"

char* getWord(char** str)
{
    char *p, *s;

    p = *str;
    SKIP_BLANKS(&p);
    for (s = p; *p && !IS_SPACE(*p) && *p != ';'; p++)
        ;
    *str = p;
    return Strnew_charp_n(s, p - s)->ptr;
}

char* getQWord(char** str)
{
    Str tmp = Strnew();
    char* p;
    int in_q = 0, in_dq = 0, esc = 0;

    p = *str;
    SKIP_BLANKS(&p);
    for (; *p; p++) {
        if (esc) {
            if (in_q) {
                if (*p != '\\' && *p != '\'') /* '..\\..', '..\'..' */
                    Strcat_char(tmp, '\\');
            } else if (in_dq) {
                if (*p != '\\' && *p != '"') /* "..\\..", "..\".." */
                    Strcat_char(tmp, '\\');
            } else {
                if (*p != '\\' && *p != '\'' && /* ..\\.., ..\'.. */
                    *p != '"' && !IS_SPACE(*p)) /* ..\".., ..\.. */
                    Strcat_char(tmp, '\\');
            }
            Strcat_char(tmp, *p);
            esc = 0;
        } else if (*p == '\\') {
            esc = 1;
        } else if (in_q) {
            if (*p == '\'')
                in_q = 0;
            else
                Strcat_char(tmp, *p);
        } else if (in_dq) {
            if (*p == '"')
                in_dq = 0;
            else
                Strcat_char(tmp, *p);
        } else if (*p == '\'') {
            in_q = 1;
        } else if (*p == '"') {
            in_dq = 1;
        } else if (IS_SPACE(*p) || *p == ';') {
            break;
        } else {
            Strcat_char(tmp, *p);
        }
    }
    *str = p;
    return tmp->ptr;
}
