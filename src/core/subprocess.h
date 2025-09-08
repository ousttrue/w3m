#pragma once
#include <stdio.h>
#include <unistd.h>

void setup_child(int child, int i, int f);
pid_t open_pipe_rw(FILE** fr, FILE** fw);
void myExec(const char* command);
void mySystem(const char* command, bool background);
