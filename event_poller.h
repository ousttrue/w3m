#pragma once

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

void event_init();
void event_deinit();
void event_listen_tty(int fd);
struct EventValue event_wait(int timeout_ms);
