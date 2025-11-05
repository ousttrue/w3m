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
Str tmpfname(int type, char* ext);
