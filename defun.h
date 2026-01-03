#pragma once

struct TabBuffer;
struct Buffer;
struct DefunContext {
    struct TabBuffer* tab;
    struct Buffer* buf;
};

typedef void (*DefunFunc)(struct DefunContext ctx);

#define DEFUN(funcname, macroname, docstring) void funcname(struct DefunContext ctx)
