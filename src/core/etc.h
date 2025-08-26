#pragma once
#include "Str.h"

Str base64_encode(const char* src, size_t len);
char* mybasename(char* s);

typedef void (*MySignalFunc)(int);
MySignalFunc mySignal(int signal_number, MySignalFunc action);
