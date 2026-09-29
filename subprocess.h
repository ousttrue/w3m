#pragma once
#include <signal.h>
#include <stdio.h>

typedef void (*SigActionFunc)(int);
extern SigActionFunc mySignal(int signal_number, SigActionFunc action);
void setup_child(bool is_child, int i, int f);
pid_t open_pipe_rw(FILE ** fr, FILE ** fw);
void myExec(const char *command);
int mySystem(const char *command, bool background);
