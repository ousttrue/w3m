/*
 * by Scarlett. public domain.
 * replacements for w3m's allocation macros which add overflow
 * detection and concentrate the macros in one file
 */
#pragma once
#include <gc.h>

size_t z_mult_no_oflow_(size_t n, size_t size);

#define New(type) \
    (GC_MALLOC(sizeof(type)))

#define NewAtom(type) \
    (GC_MALLOC_ATOMIC(sizeof(type)))

#define New_N(type, n) \
    (GC_MALLOC(z_mult_no_oflow_((n), sizeof(type))))

#define NewAtom_N(type, n) \
    (GC_MALLOC_ATOMIC(z_mult_no_oflow_((n), sizeof(type))))

#define New_Reuse(type, ptr, n) \
    (GC_REALLOC((ptr), z_mult_no_oflow_((n), sizeof(type))))
