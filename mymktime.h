#pragma once
#include <time.h>

/// RFC 1123 or RFC 850 or ANSI C asctime() format string -> time_t
time_t mymktime(const char* timestr);
