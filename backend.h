#pragma once

int backend(void);

extern int w3m_backend;
extern struct TextLineList* backend_halfdump_buf;
extern struct TextList* backend_batch_commands;
