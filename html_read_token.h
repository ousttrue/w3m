#pragma once
#include <gcstr.h>

/* state of token scanning finite state machine */
enum ReadTokenStatus {
    R_ST_NORMAL = 0, /* normal */
    R_ST_TAG0 = 1, /* within tag, just after < */
    R_ST_TAG = 2, /* within tag */
    R_ST_QUOTE = 3, /* within single quote */
    R_ST_DQUOTE = 4, /* within double quote */
    R_ST_EQL = 5, /* = */
    R_ST_AMP = 6, /* within ampersand quote */
    R_ST_EOL = 7, /* end of file */
    R_ST_CMNT1 = 8, /* <!  */
    R_ST_CMNT2 = 9, /* <!- */
    R_ST_CMNT = 10, /* within comment */
    R_ST_NCMNT1 = 11, /* comment - */
    R_ST_NCMNT2 = 12, /* comment -- */
    R_ST_NCMNT3 = 13, /* comment -- space */
    R_ST_IRRTAG = 14, /* within irregular tag */
    R_ST_VALUE = 15, /* within tag attribule value */
};

inline static bool ST_IS_REAL_TAG(enum ReadTokenStatus s)
{
    return ((s) == R_ST_TAG || (s) == R_ST_TAG0 || (s) == R_ST_EQL || (s) == R_ST_VALUE);
}

/* is this '<' really means the beginning of a tag? */
inline static bool REALLY_THE_BEGINNING_OF_A_TAG(const char* p)
{
    return (IS_ALPHA(p[1]) || p[1] == '/' || p[1] == '!' || p[1] == '?' || p[1] == '\0' || p[1] == '_');
}

int next_status(char c, enum ReadTokenStatus* status);
bool read_token(Str buf, const char** instr, enum ReadTokenStatus* status, bool pre, bool append);
Str correct_irrtag(enum ReadTokenStatus status);
