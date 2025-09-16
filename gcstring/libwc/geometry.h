#pragma once
#include <stdbool.h>

// cursor position, term size... etc
struct Int2 {
    int x;
    int y;
};

struct Rect {
    struct Int2 offset;
    struct Int2 size;
};

struct VirtualTerm;
struct Buffer;

enum MessageSeverity {
    MSG_INFO,
    MSG_ERR,
};

//
// blockable UI functions
//
struct mco_coro;
typedef const int (*GetChFunc)(struct mco_coro*);
typedef const char* (*InputFunc)(struct mco_coro*, const char* prompt);
typedef void (*MessageFunc)(struct mco_coro*, enum MessageSeverity error, const char* msg);
struct CoVTable {
    GetChFunc getCh;
    InputFunc input;
    MessageFunc message;
};

// #define UI_TTY \
//     (struct UserInteraction) { .inputCallback = inputAnswer, .messageCallback = &error_message, }
struct UI;
typedef void (*CommandFunc)(struct UI *ui);
struct UI {
    struct Buffer* first_buffer;
    struct Buffer* current_buffer;
    // struct Content* content;
    // struct Document* document;
    struct VirtualTerm* vt;
    bool use_graphic;
    struct Rect viewport;
    // viewport local position
    struct Int2 viewport_cursor;
    // term global position
    struct Int2 term_cursor;
    int searchkey_num;

    CommandFunc cmd;
    struct mco_coro* co;
    struct CoVTable vtable;
};

struct BufferPoint {
    int line;
    int pos;
    int invalid;
};

struct BufferPos {
    int top_linenumber;
    int cur_linenumber;
    int currentColumn;
    int pos;
    struct BufferPos* next;
    struct BufferPos* prev;
};
