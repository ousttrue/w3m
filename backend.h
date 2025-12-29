#pragma once
#include "textlist.h"

extern struct TextList* backend_batch_commands;
extern TextLineList* backend_halfdump_buf;
int backend(void);

