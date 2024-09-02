#pragma once
#include "config.h"

MySignalHandler resize_hook(SIGNAL_ARG);
void _newT(void);
void delBuffer(Buffer *buf);
int do_add_download_list();
void mainloop(char *line_str);
