#include "alarm.h"
#include "alloc.h"
#include "funcname1.h"

static struct AlarmEvent DefaultAlarm = {
    .sec = 0,
    .status = AL_UNSET,
    .cmd = FUNCNAME_nulcmd,
    .data = 0
};
struct AlarmEvent* getDefaultAlarm() { return &DefaultAlarm; }

struct AlarmEvent* CurrentAlarm = &DefaultAlarm;

void setAlarmEventOrDefaultAlarm(struct AlarmEvent* event)
{
    if (event) {
        CurrentAlarm = event;
    } else {
        CurrentAlarm = &DefaultAlarm;
    }
}

struct AlarmEvent*
setAlarmEvent(struct AlarmEvent* event, int sec, enum AlarmEventStatus status,
    int cmd, const void* data)
{
    if (event == 0)
        event = New(struct AlarmEvent);
    event->sec = sec;
    event->status = status;
    event->cmd = cmd;
    event->data = data;
    return event;
}
