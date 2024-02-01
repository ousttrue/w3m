#pragma once
#include <stdio.h>
#include <unistd.h>

pid_t open_pipe_rw(FILE **fr, FILE **fw);
const char *file_to_url(const char *file);
int is_localhost(const char *host);
const char *lastFileName(const char *path);
const char *mybasename(const char *s);
const char *mydirname(const char *s);
const char *expandName(const char *name);
const char *expandPath(const char *name);
FILE *openSecretFile(const char *fname);
