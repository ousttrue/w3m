static void
resize_screen(void)
{
    need_resize_screen = false;
    setlinescols(get_tty_fd());
    vt_setupscreen(getScreen(), getLines(), getCols());
    vt_clear(getScreen());
}

typedef struct _Event {
    int cmd;
    void* data;
    struct _Event* next;
} Event;
static Event* CurrentEvent = NULL;
static Event* LastEvent = NULL;

// static AlarmEvent DefaultAlarm = {
//     0, AL_UNSET, FUNCNAME_nulcmd, NULL
// };
// static AlarmEvent* CurrentAlarm = &DefaultAlarm;
// static void SigAlarm(int _dummy);

void pushEvent(int cmd, void* data)
{
    Event* event;

    event = New(Event);
    event->cmd = cmd;
    event->data = data;
    event->next = NULL;
    if (CurrentEvent)
        LastEvent->next = event;
    else
        CurrentEvent = event;
    LastEvent = event;
}

static void SigAlarm(int _dummy)
{
    struct UI ui = getUI();
    if (CurrentAlarm->sec > 0) {
        CurrentKey = -1;
        CurrentKeyData = NULL;
        char* data;
        CurrentCmdData = data = (char*)CurrentAlarm->data;
        w3mFuncList[CurrentAlarm->cmd].func(getUI());
        CurrentCmdData = NULL;
        if (CurrentAlarm->status == AL_IMPLICIT_ONCE) {
            CurrentAlarm->sec = 0;
            CurrentAlarm->status = AL_UNSET;
        }
        if (ui.current_buffer->event) {
            if (ui.current_buffer->event->status != AL_UNSET)
                CurrentAlarm = ui.current_buffer->event;
            else
                ui.current_buffer->event = NULL;
        }
        if (!ui.current_buffer->event)
            CurrentAlarm = &DefaultAlarm;
        // if (CurrentAlarm->sec > 0) {
        //     mySignal(SIGALRM, SigAlarm);
        //     alarm(CurrentAlarm->sec);
        // }
    }
}

AlarmEvent*
setAlarmEvent(AlarmEvent* event, int sec, short status, int cmd, void* data)
{
    if (event == NULL)
        event = New(AlarmEvent);
    event->sec = sec;
    event->status = status;
    event->cmd = cmd;
    event->data = data;
    return event;
}

/* get keypress event */
// if (ui.current_buffer->event) {
//     if (ui.current_buffer->event->status != AL_UNSET) {
//         CurrentAlarm = ui.current_buffer->event;
//         if (CurrentAlarm->sec == 0) { /* refresh (0sec) */
//             ui.current_buffer->event = NULL;
//             CurrentKey = -1;
//             CurrentKeyData = NULL;
//             CurrentCmdData = (char*)CurrentAlarm->data;
//             w3mFuncList[CurrentAlarm->cmd].func(ui);
//
//             if (updateCursor(getUI().current_buffer)) {
//             }
//             termClear(ttyWriter());
//             bufToScreen(ui);
//             renderFrame(ui);
//
//             CurrentCmdData = NULL;
//             return false;
//         }
//     } else
//         ui.current_buffer->event = NULL;
// }
// if (!ui.current_buffer->event)
//     CurrentAlarm = &DefaultAlarm;
// if (CurrentAlarm->sec > 0) {
//     mySignal(SIGALRM, SigAlarm);
//     alarm(CurrentAlarm->sec);
// }

// if (CurrentAlarm->sec > 0) {
//     alarm(0);
// }

bool onFrame()
{
    struct UI ui = getUI();

    struct TermEntry* t = getTermEntry();
    bool use_graphic = graph_ok(t);

    updateDownload();
    if (ui.current_buffer->submit) {
        struct Anchor* a = ui.current_buffer->submit;
        ui.current_buffer->submit = NULL;
        gotoLine(&ui.current_buffer->document, a->start.line);
        ui.current_buffer->document.pos = a->start.pos;
        _followForm(ui, true, false);
        return false;
    }

    /* event processing */
    if (CurrentEvent) {
        CurrentKey = -1;
        CurrentKeyData = NULL;
        CurrentCmdData = (char*)CurrentEvent->data;
        w3mFuncList[CurrentEvent->cmd].func(ui);

        if (updateCursor(getUI().current_buffer)) {
        }
        termClear(ttyWriter());
        bufToScreen(ui);
        renderFrame(ui);

        CurrentCmdData = NULL;
        CurrentEvent = CurrentEvent->next;
        return false;
    }

    // mySignal(SIGWINCH, resize_hook);
    if (activeImage && displayImage && ui.document->img && !ui.document->image_loaded) {
        loadImage(ui, ui.document, IMG_FLAG_NEXT, false);
        bufToScreen(ui);
        renderFrame(ui);
        // continue;
    }
    if (need_resize_screen) {
        resize_screen();
        bufToScreen(ui);
        renderFrame(ui);
    }

    return true;
}

// static void
// sig_chld(int signo)
// {
//     int p_stat;
//     pid_t pid;
//
//     while ((pid = waitpid(-1, &p_stat, WNOHANG)) > 0) {
//         DownloadList* d;
//
//         if (WIFEXITED(p_stat)) {
//             for (d = FirstDL; d != NULL; d = d->next) {
//                 if (d->pid == pid) {
//                     d->err = WEXITSTATUS(p_stat);
//                     break;
//                 }
//             }
//         }
//     }
//     mySignal(SIGCHLD, sig_chld);
// }

// static void
// SigPipe(int _dummy)
// {
//     mySignal(SIGPIPE, SigPipe);
// }

// void set_int(void)
// {
//     mySignal(SIGHUP, reset_exit);
//     mySignal(SIGINT, reset_exit);
//     mySignal(SIGQUIT, reset_exit);
//     mySignal(SIGTERM, reset_exit);
//     mySignal(SIGILL, error_dump);
//     mySignal(SIGIOT, error_dump);
//     mySignal(SIGFPE, error_dump);
// #ifdef SIGBUS
//     mySignal(SIGBUS, error_dump);
// #endif /* SIGBUS */
//     /* mySignal(SIGSEGV, error_dump); */
// }

