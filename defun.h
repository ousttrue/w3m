#pragma once

struct TabBuffer;
struct Buffer;
struct DefunContext {
    struct TabBuffer* tab;
    struct Buffer* buf;
    int key;
    int lastKey;
    const char* data;
    int num;
};

typedef void (*DefunFunc)(struct DefunContext ctx);

#define DEFUN(funcname, macroname, docstring) void funcname(struct DefunContext ctx)
