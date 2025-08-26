#include "event_poller.h"
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <assert.h>
#include <gc.h>

pthread_t g_thread;
struct EventThreadArgs* g_args = 0;

void* g_queue_buffer[100];
queue_t g_queue = QUEUE_INITIALIZER(g_queue_buffer);
bool g_use_input = false;

static const char* err_msg(int no);

int create_signalfd(void)
{
    sigset_t mask;
    sigemptyset(&mask);
    // sigaddset(&mask, SIGINT);
    // sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGUSR1);
    // sigaddset(&mask, SIGUSR2);

    sigprocmask(SIG_BLOCK, &mask, NULL);
    return signalfd(-1, &mask, SFD_CLOEXEC);
}

void* thread_func(void* param)
{
    const char* err_msg = 0;
    int epoll = epoll_create1(0);
    if (epoll < 0) {
        fprintf(stderr, "\nepoll_create1\n");
        goto join;
    }

    // epoll tty
    {
        struct epoll_event ev;
        ev.data.fd = g_args->tty_fd;
        ev.events = EPOLLIN;
        int ret = epoll_ctl(epoll, EPOLL_CTL_ADD, ev.data.fd, &ev);
        if (ret < 0) {
            err_msg = "epoll_ctl: tty fd";
            goto join;
        }
    }

    // epoll signalfd
    int signal_fd = create_signalfd();
    {
        struct epoll_event ev;
        if (signal_fd == -1) {
            err_msg = "create_signalfd";
            goto join;
        }
        ev.events = EPOLLIN;
        ev.data.fd = signal_fd;
        int ret = epoll_ctl(epoll, EPOLL_CTL_ADD, ev.data.fd, &ev);
        if (ret < 0) {
            err_msg = "epoll_ctl: signalfd";
            goto join;
        }
    }

    while (true) {
        struct epoll_event list[32];
        int ret = epoll_wait(epoll, list, sizeof(list) / sizeof(list[0]), -1);
        if (ret < 0) {
            err_msg = "epoll_wait";
            break;
        }

        for (int i = 0; i < ret; ++i) {
            struct epoll_event* event = &list[i];
            if (event->data.fd == g_args->tty_fd) {
                int fd = event->data.fd;
                int ready_read_size;
                ioctl(fd, FIONREAD, &ready_read_size);
                for (int j = 0; j < ready_read_size; ++j) {
                    char c;
                    int ret = read(event->data.fd, &c, 1);
                    if (ret < 0) {
                        if (ret == EAGAIN) {
                            break;
                        }
                        err_msg = "read tty";
                        goto join;
                    } else if (ret == 0) {
                        // err_msg = "read tty: zero";
                        // goto join;
                        break;
                    } else {
                        assert(ret == 1);
                        struct EventValue* msg = (struct EventValue*)GC_MALLOC(sizeof(struct EventValue));
                        msg->type = EVT_TTY_CHAR;
                        msg->data.ch = c;
                        if (g_use_input) {
                            queue_enqueue(&g_queue, msg);
                        } else {
                            queue_enqueue(&g_args->queue, msg);
                        }
                    }
                }
            } else if (event->data.fd == signal_fd) {
                err_msg = "signal_fd";
                goto join;
            }
        }
    }

join:
    close(epoll);
    return (void*)err_msg;
}

bool event_init(struct EventThreadArgs* args)
{
    g_args = args;
    return pthread_create(&g_thread, NULL, thread_func, NULL) == 0;
}

bool event_deinit(const char** err_msg)
{
    raise(SIGUSR1);
    int ret = pthread_join(g_thread, (void**)err_msg);
    if (ret != 0) {
        *err_msg = "pthread_join";
        return false;
    }
    return *err_msg == 0;
}

static const char* err_msg(int no)
{
    switch (errno) {
    case EBADF:
        return "epfd or fd is not a valid file descriptor.";
    case EEXIST:
        return "op was EPOLL_CTL_ADD, and the supplied file descriptor fd is already registered with this epoll instance.";
    case EINVAL:
        return "epfd  is  not  an  epoll file descriptor, or fd is the same as epfd, or the requested operation op is not supported by this interface."
               "An invalid event type was specified along with EPOLLEXCLUSIVE in events."
               "op was EPOLL_CTL_MOD and events included EPOLLEXCLUSIVE."
               "op was EPOLL_CTL_MOD and the EPOLLEXCLUSIVE flag has previously been applied to this epfd, fd pair. EPOLLEXCLUSIVE was specified in event and fd refers to an epoll instance.";
    case ELOOP:
        return "fd refers to an epoll instance and this EPOLL_CTL_ADD operation would result in a circular loop of  epoll instances monitoring one another or a nesting depth of epoll instances greater than 5.";
    case ENOENT:
        return "op was EPOLL_CTL_MOD or EPOLL_CTL_DEL, and fd is not registered with this epoll instance.";
    case ENOMEM:
        return "There was insufficient memory to handle the requested op control operation.";
    case ENOSPC:
        return "The  limit  imposed  by  /proc/sys/fs/epoll/max_user_watches  was  encountered  while  trying to register (EPOLL_CTL_ADD) a new file descriptor on an epoll instance.  See epoll(7) for further details.";
    case EPERM:
        return "The target file fd does not support epoll.  This error can occur if fd refers to, for example, a  regular file or a directory.";
    }
    return "unknown";
}

const char* msgrcv_error_msg()
{
    switch (errno) {
    case E2BIG:
        return "The value of mtext is greater than msgsz and (msgflg & MSG_NOERROR) is 0.";
    case EACCES:
        return "Operation permission is denied to the calling process; see Section 2.7, XSI Interprocess Communication.";
    case EIDRM:
        return "The message queue identifier msqid is removed from the system.";
    case EINTR:
        return "The msgrcv() function was interrupted by a signal.";
    case EINVAL:
        return "msqid is not a valid message queue identifier.";
    case ENOMSG:
        return "The queue does not contain a message of the desired type and (msgflg & IPC_NOWAIT) is non-zero.";
    }
    return "unknown";
}

int getch(void)
{
    struct EventValue* event = queue_dequeue(&g_queue);
    assert(event->type == EVT_TTY_CHAR);
    return event->data.ch;
}

GetChFunc event_begin_input(int timeout_ms)
{
    g_use_input = true;
    return getch;
}

void event_end_input(GetChFunc)
{
    g_use_input = false;
}
