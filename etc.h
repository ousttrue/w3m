#pragma once
#include <time.h>
#include <stdio.h>

time_t mymktime(char *timestr);
pid_t open_pipe_rw(FILE **fr, FILE **fw);
char *file_to_url(char *file);
int is_localhost(const char *host);
const char *lastFileName(const char *path);
const char *mybasename(const char *s);
const char *mydirname(const char *s);
