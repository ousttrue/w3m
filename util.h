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
struct wc_option;
uint32_t getChar(struct wc_option *WcOption, const char* p);
int is_wordchar(uint32_t c);
