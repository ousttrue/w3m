#include "AlarmEvent.h"
#include "funcname1.h"
#include "buffer.h"
#include "fm.h"

static struct AlarmEvent DefaultAlarm = (struct AlarmEvent) {
    .sec = 0,
    .status = AL_UNSET,
    .cmd = FUNCNAME_nulcmd,
    .data = 0,
};

struct AlarmEvent* CurrentAlarm = &DefaultAlarm;

void setAlarmEventDefault()
{
    CurrentAlarm = &DefaultAlarm;
}

struct AlarmEvent*
setAlarmEvent(struct _Buffer* buf, int sec, enum AlarmStatus status, int cmd, void* data)
{
    struct AlarmEvent* event = (buf)
        ? (buf->event ? buf->event : New(struct AlarmEvent))
        : &DefaultAlarm;
    event->sec = sec;
    event->status = status;
    event->cmd = cmd;
    event->data = data;
    return event;
}
