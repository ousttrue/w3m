#pragma once
#include <stdio.h>
#include <sys/types.h>
#include <wc.h>
#include "textlist.h"

extern TextList* fileToDelete;

pid_t open_pipe_rw(FILE** fr, FILE** fw);
time_t mymktime(const char* timestr);
int read_token(Str buf, char** instr, int* status, int pre, int append);
Str correct_irrtag(int status);
Str romanNumeral(int n);
Str romanAlphabet(int n);
Str myExtCommand(char* cmd, char* arg, int redirect);
Str myEditor(char* cmd, char* file, int line);

#define TMPF_DFL 0
#define TMPF_SRC 1
#define TMPF_FRAME 2
#define TMPF_CACHE 3
#define TMPF_COOKIE 4
#define TMPF_HIST 5
#define MAX_TMPF_TYPE 6


Str tmpfname(int type, const char* ext);

int is_localhost(const char* host);
char* file_to_url(char* file);
char* lastFileName(char* path);
struct parsed_tagarg;
void change_charset(struct parsed_tagarg* arg);
