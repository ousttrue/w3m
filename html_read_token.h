#pragma once
#include <gcstr.h>
#include "ReadTokenStatus.h"

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
