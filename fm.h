#pragma once

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* strcasestr() */
#endif

#define DEFUN(funcname, macroname, docstring) void funcname(void)

#include "func.h"
extern unsigned char GlobalKeymap[];
extern unsigned char EscKeymap[];
extern unsigned char EscBKeymap[];
extern unsigned char EscDKeymap[];
extern struct FuncList w3mFuncList[];

