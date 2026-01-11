#pragma once
#include "defun.h"
#include <stdbool.h>
#include <stdint.h>

DefunFunc keymap_fromName(const char *name);
void keymap_init(bool force);
void keymap_parseLine(const char* p, int lineno, bool verbose);
char* getKeyData(int key);

struct KeyRegister {
    DefunFunc func;
    const char* data;
};
void keymap_register(uint32_t key, struct KeyRegister reg);
struct KeyRegister keymap_fromKey(uint32_t key);
