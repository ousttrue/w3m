#pragma once
#include <stdio.h>
#include <stdint.h>

// pid_t
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

pid_t open_pipe_rw(FILE **fr, FILE **fw);

char *expandName(char *name);

void sleepSeconds(uint32_t seconds);
