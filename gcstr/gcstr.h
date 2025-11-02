#pragma once
#include <stdarg.h>

#include "alloc.h"
#include "Str.h"
#include "myctype.h"
#include "hash.h"

int vscpf(const char* fmt, va_list ap);
