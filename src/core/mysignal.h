#pragma once

typedef void (*MySignalFunc)(int);
MySignalFunc mySignal(int signal_number, MySignalFunc action);

