#pragma once

extern bool fmInitialized;
extern bool IsForkChild;
extern char *w3m_version;
extern bool w3m_backend;
struct TextLineList;
extern TextLineList *backend_halfdump_buf;
struct TextList;
extern TextList *backend_batch_commands;

extern char *HostName;
extern char *CurrentDir;
extern int CurrentPid;
struct TextList;
extern TextList *fileToDelete;

extern char *document_root;

int backend(void);
void w3m_exit(int i);
