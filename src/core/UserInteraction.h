#pragma once

typedef const char* (*InputFunc)(const char* prompt);
typedef void (*MessageFunc)(const char* msg);
struct UserInteraction {
    InputFunc inputCallback;
    // void* confirmData;
    MessageFunc messageCallback;
    // void* MessageData;
};

#define UI_TTY (struct UserInteraction) { .inputCallback = inputAnswer, .messageCallback = &error_message, }

