#pragma once
#include <w3m.h>

extern struct _textlist* backend_batch_commands;
extern struct _textlinelist* backend_halfdump_buf;

int backend(struct CmdArgs *args);
