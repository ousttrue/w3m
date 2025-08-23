#pragma once
#include <stdbool.h>
#include "queue.h"

struct EventThreadArgs {
    int tty_fd;
    queue_t queue;
};

enum EventType {
    EVT_TIMEOUT,
    EVT_ERROR,
    EVT_TTY_CHAR,
};

struct EventValue {
    enum EventType type;
    union Value {
        int error;
        int ch;
    } data;
};

bool event_init(struct EventThreadArgs *args);
bool event_deinit(const char** err_msg);
const char* msgrcv_error_msg();
