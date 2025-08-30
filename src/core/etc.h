#pragma once
#include <Str.h>
#include "line.h"

struct _Buffer;

Str base64_encode(const char* src, size_t len);
char* mybasename(char* s);

typedef void (*MySignalFunc)(int);
MySignalFunc mySignal(int signal_number, MySignalFunc action);

int columnSkip(struct _Buffer* buf, int offset);
struct _Line* lineSkip(struct _Buffer* buf, struct _Line* line, int offset, int last);
struct _Line* currentLineSkip(struct _Buffer* buf, struct _Line* line, int offset, int last);
int gethtmlcmd(char** s);
Str checkType(Str s, Lineprop** oprop, Linecolor** ocolor);
char* lastFileName(char* path);
char* mydirname(char* s);
int next_status(char c, int* status);
int read_token(Str buf, char** instr, int* status, int pre, int append);
Str correct_irrtag(int status);
