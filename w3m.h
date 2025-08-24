#pragma once

void fmInit();
void fmTerm();

int main_loop();
void _goLine(const char* l);

struct _Buffer;
void delBuffer(struct _Buffer* buf);
