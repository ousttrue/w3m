#include "event_poller.h"
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <sys/epoll.h>
// abort
#include <stdlib.h>
// close
#include <unistd.h>
#include <assert.h>

int g_epoll = 0;
struct epoll_event g_list[32];
int g_tty = 0;

void event_init()
{
    // assert(false);
    g_epoll = epoll_create1(0);
    if (g_epoll < 0) {
        fprintf(stderr, "\nepoll_create1\n");
        exit(7);
    }
}

void event_deinit()
{
    if (g_epoll > 0) {
        close(g_epoll);
        g_epoll = 0;
    }
}

void event_listen_tty(int fd)
{
    struct epoll_event ev;
    // ev.data.fd = STDIN_FILENO;
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLEXCLUSIVE;
    int ret = epoll_ctl(g_epoll, EPOLL_CTL_ADD, fd, &ev);
    if (ret < 0) {
        const char* msg = "unknown";
        switch (errno) {
        case EBADF:
            msg = "epfd or fd is not a valid file descriptor.";
            break;

        case EEXIST:
            msg = "op was EPOLL_CTL_ADD, and the supplied file descriptor fd is already registered with this epoll instance.";
            break;

        case EINVAL:
            msg = "epfd  is  not  an  epoll file descriptor, or fd is the same as epfd, or the requested operation op is not supported by this interface."
                  "An invalid event type was specified along with EPOLLEXCLUSIVE in events."
                  "op was EPOLL_CTL_MOD and events included EPOLLEXCLUSIVE."
                  "op was EPOLL_CTL_MOD and the EPOLLEXCLUSIVE flag has previously been applied to this epfd, fd pair. EPOLLEXCLUSIVE was specified in event and fd refers to an epoll instance.";
            break;

        case ELOOP:
            msg = "fd refers to an epoll instance and this EPOLL_CTL_ADD operation would result in a circular loop of  epoll instances monitoring one another or a nesting depth of epoll instances greater than 5.";
            break;

        case ENOENT:
            msg = "op was EPOLL_CTL_MOD or EPOLL_CTL_DEL, and fd is not registered with this epoll instance.";
            break;

        case ENOMEM:
            msg = "There was insufficient memory to handle the requested op control operation.";
            break;

        case ENOSPC:
            msg = "The  limit  imposed  by  /proc/sys/fs/epoll/max_user_watches  was  encountered  while  trying to register (EPOLL_CTL_ADD) a new file descriptor on an epoll instance.  See epoll(7) for further details.";
            break;

        case EPERM:
            msg = "The target file fd does not support epoll.  This error can occur if fd refers to, for example, a  regular file or a directory.";
            break;
        }
        fprintf(stderr, "\nepoll_ctl: epoll: %d, fd: %d, error: %d => %s\n", g_epoll, fd, errno, msg);
        exit(8);
    }
    g_tty = fd;
}

struct EventValue event_wait(int timeout_ms)
{
    struct EventValue value = { 0 };
    int ret = epoll_wait(g_epoll, g_list, sizeof(g_list) / sizeof(g_list[0]), timeout_ms);
    if (ret < 0) {
        value.type = EVT_ERROR;
        value.data.error = ret;
        return value;
    }
    if (ret == 0) {
        value.type = EVT_TIMEOUT;
        return value;
    }
    assert(g_list[0].data.fd == g_tty);
    char ch;
    ret = read(g_list[0].data.fd, &ch, 1);
    if (ret < 0) {
        if (errno == EINTR || errno == EAGAIN) {
            value.type = EVT_TIMEOUT;
            return value;
        }
        value.type = EVT_ERROR;
        value.data.error = errno;
        return value;
    }
    if (ret == 0) {
        value.type = EVT_TIMEOUT;
        return value;
    }
    if (ret > 1) {
        value.type = EVT_ERROR;
        // value.data.error = errno;
        return value;
    }
    value.type = EVT_TTY_CHAR;
    value.data.ch = ch;
    return value;
}
