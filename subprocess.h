#pragma once

typedef void (*SigActionFunc)(int);
extern SigActionFunc mySignal(int signal_number, SigActionFunc action);
