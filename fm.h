/*
 * w3m: WWW wo Miru utility
 *
 * by A.ITO  Feb. 1995
 *
 * You can use,copy,modify and distribute this program without any permission.
 */
#pragma once
#include "config.h" /* At top for defines below */

#define DEFUN(funcname, macroname, docstring) void funcname(void)

#define MAX_IMAGE 1000
#define MAX_IMAGE_SIZE 2048

#define PIPEBUFFERNAME "*stream*"

#define RELATIVE_WIDTH(w) (((w) >= 0) ? (int)((w) / pixel_per_char) : (w))

#if defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE)
global char* MyProgramName init("w3m");
#endif /* defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE) */
