#pragma once

enum AlarmStatus {
    AL_UNSET = 0,
    AL_EXPLICIT = 1,
    AL_IMPLICIT = 2,
    AL_IMPLICIT_ONCE = 3,
};

struct AlarmEvent {
    int sec;
    enum AlarmStatus status;
    int cmd;
    void* data;
};

extern struct AlarmEvent* CurrentAlarm;

void setAlarmEventDefault();

struct _Buffer;
struct AlarmEvent* setAlarmEvent(struct _Buffer* buf, int sec, enum AlarmStatus status, int cmd, void* data);
