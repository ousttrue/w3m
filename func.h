#pragma once
#include "defun.h"
#include <stdbool.h>
#include <stdint.h>

DefunFunc keymap_fromName(const char* name);
void keymap_init(bool force);
void keymap_parseLine(const char* p, int lineno, bool verbose);

