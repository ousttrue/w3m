#pragma once

#define AL_UNSET 0
#define AL_EXPLICIT 1
#define AL_IMPLICIT 2
#define AL_IMPLICIT_ONCE 3

struct _AlarmEvent* setAlarmEvent(struct _AlarmEvent* event, int sec, short status,
    const char* cmd, const void* data);
