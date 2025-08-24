#pragma once

int main_loop();
void _goLine(const char* l);

void _newT(void);
struct _Buffer;
void delBuffer(struct _Buffer* buf);
