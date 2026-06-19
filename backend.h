#ifndef W3M_BACKEND_H
#define W3M_BACKEND_H
#include "textlist.h"

int backend(void);

extern int w3m_backend;
extern TextLineList *backend_halfdump_buf;
extern TextList *backend_batch_commands;
#endif
