#include "alarm.h"
#include "alloc.h"

AlarmEvent *setAlarmEvent(AlarmEvent *event, int sec, short status, int cmd,
                          void *data) {
  if (event == nullptr)
    event = (AlarmEvent *)New(AlarmEvent);
  event->sec = sec;
  event->status = status;
  event->cmd = cmd;
  event->data = data;
  return event;
}
