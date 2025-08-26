#pragma once
#include <stdbool.h>

void fmInit();
void fmTerm();

int main_loop(const char* line_str);
void _goLine(const char* l);

struct _Buffer;
void delBuffer(struct _Buffer* buf);

bool onFrame();
void onKeyInput(char c);
