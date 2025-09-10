#pragma once
#include <stdbool.h>

struct KeyValue {
    const char* arg;
    const char* value;
    struct KeyValue* next;
};

const char* tag_get_value(struct KeyValue* t, const char* arg);
bool tag_exists(struct KeyValue* t, const char* arg);
struct KeyValue* cgistr2tagarg(const char* cgistr);
