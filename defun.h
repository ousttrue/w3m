#pragma once
#include "config.h"

MySignalHandler resize_hook(SIGNAL_ARG);
void _newT(void);
void delBuffer(struct Buffer *buf);
void mainloop(char *line_str);
