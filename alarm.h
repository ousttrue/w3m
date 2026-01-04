#pragma once

#define AL_UNSET 0
#define AL_EXPLICIT 1
#define AL_IMPLICIT 2
#define AL_IMPLICIT_ONCE 3

typedef struct _AlarmEvent {
    int sec;
    short status;
    int cmd;
    const void* data;
} AlarmEvent;

AlarmEvent* setAlarmEvent(AlarmEvent* event, int sec, short status,
    int cmd, const void* data);
