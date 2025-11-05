#pragma once
#include <stdio.h>
#include <sys/types.h>
#include "textlist.h"

extern TextList* fileToDelete;

pid_t open_pipe_rw(FILE** fr, FILE** fw);
time_t mymktime(const char* timestr);
