#pragma once

extern char *w3m_version;

extern char *HostName;
extern char *CurrentDir;
extern int CurrentPid;
struct TextList;
extern TextList *fileToDelete;

extern char *document_root;

void w3m_exit(int i);
