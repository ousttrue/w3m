#pragma once
#include <stdbool.h>

typedef void (*MySignalFunc)(int);
MySignalFunc mySignal(int signal_number, MySignalFunc action);

void reset_signals(void);
