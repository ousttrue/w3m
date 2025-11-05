#pragma once
#include "textlist.h"

extern int w3m_backend;
extern TextLineList* backend_halfdump_buf;
extern TextList* backend_batch_commands;

int backend(void);
