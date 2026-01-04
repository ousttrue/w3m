#include "alarm.h"
#include "alloc.h"
#include "funcname1.h"
// #include "mysignal.h"

// static AlarmEvent DefaultAlarm = {
//     0, AL_UNSET, FUNCNAME_nulcmd, 0
// };
// static AlarmEvent* CurrentAlarm = &DefaultAlarm;
// static MySignalHandler SigAlarm(SIGNAL_ARG);
// static MySignalHandler SigPipe(SIGNAL_ARG);

AlarmEvent* setAlarmEvent(AlarmEvent* event, int sec, short status, int cmd, const void* data)
{
    if (!event)
        event = New(AlarmEvent);
    event->sec = sec;
    event->status = status;
    event->cmd = cmd;
    event->data = data;
    return event;
}
