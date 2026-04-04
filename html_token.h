#pragma once
#include "Str.h"

// state of token scanning finite state machine
enum TokenStatus {
    // normal
    R_ST_NORMAL = 0,
    // within tag, just after <
    R_ST_TAG0 = 1,
    // within tag
    R_ST_TAG = 2,
    // within single quote
    R_ST_QUOTE = 3,
    // within double quote
    R_ST_DQUOTE = 4,
    // =
    R_ST_EQL = 5,
    // within ampersand quote
    R_ST_AMP = 6,
    // end of file
    R_ST_EOL = 7,
    // <!
    R_ST_CMNT1 = 8,
    // <!-
    R_ST_CMNT2 = 9,
    // within comment
    R_ST_CMNT = 10,
    // comment -
    R_ST_NCMNT1 = 11,
    // comment --
    R_ST_NCMNT2 = 12,
    // comment -- space
    R_ST_NCMNT3 = 13,
    // within irregular tag
    R_ST_IRRTAG = 14,
    // within tag attribule value
    R_ST_VALUE = 15,
};

#define ST_IS_REAL_TAG(s) ((s) == R_ST_TAG || (s) == R_ST_TAG0 || (s) == R_ST_EQL || (s) == R_ST_VALUE)

int read_token(Str buf, const char** instr, enum TokenStatus* status, int pre, int append);
int next_status(char c, enum TokenStatus* status);
Str correct_irrtag(enum TokenStatus status);
