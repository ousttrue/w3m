#pragma once

extern char TrapSignal;

typedef void (*MySignalHandler)(int);
MySignalHandler mySignal(int signal_number, MySignalHandler action);

void intTrap(int _dummy);
