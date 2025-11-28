#pragma once
#include <stdbool.h>

struct KeyValueList {
    const char* arg;
    const char* value;
    struct KeyValueList* next;
};

const char* tag_get_value(struct KeyValueList* t, const char* arg);
bool tag_exists(struct KeyValueList* t, const char* arg);
struct KeyValueList* cgistr2tagarg(const char* cgistr);
