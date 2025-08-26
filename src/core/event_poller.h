#pragma once

typedef int (*GetChFunc)(void);
GetChFunc event_begin_input(int timeout_ms);
void event_end_input(GetChFunc);
