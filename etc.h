#pragma once
#include <time.h>
#include <stdio.h>

time_t mymktime(char *timestr);
pid_t open_pipe_rw(FILE **fr, FILE **fw);
