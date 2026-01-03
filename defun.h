#pragma once

#define DEFUN(funcname, macroname, docstring) void funcname(void)

typedef void (*DefunFunc)();
