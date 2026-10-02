#pragma once
#include <stdint.h>

int exec_cmd(char* cmd);

#define nextChar(s, l) \
    do {               \
        (s)++;         \
    } while ((s) < (l)->len && (l)->propBuf[s] & PC_WCHAR2)
#define prevChar(s, l) \
    do {               \
        (s)--;         \
    } while ((s) > 0 && (l)->propBuf[s] & PC_WCHAR2)
uint32_t getChar(const char* p);
int is_wordchar(uint32_t c);
