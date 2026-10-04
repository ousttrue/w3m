#pragma once

enum AlarmEventStatus {
    AL_UNSET,
    AL_EXPLICIT,
    AL_IMPLICIT,
    AL_IMPLICIT_ONCE,
};

struct AlarmEvent {
    int sec;
    short status;
    int cmd;
    const void* data;
};

extern struct AlarmEvent* CurrentAlarm;

struct AlarmEvent* getDefaultAlarm();

void setAlarmEventOrDefaultAlarm(struct AlarmEvent* event);

struct AlarmEvent* setAlarmEvent(struct AlarmEvent* event,
    int sec, enum AlarmEventStatus status, int cmd, const void* data);
