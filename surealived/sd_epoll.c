/*
 * Copyright 2009-2011 DreamLab Onet.pl Sp. z o.o.
 * 
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *   See the GNU General Public License for more details.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
*/

#include <sd_epoll.h>

SDepoll *sd_epoll_new(int size) {
    SDepoll *epoll = g_new0(SDepoll, 1);
    LOGDEBUG("Creating epoll size=%d", size);
    epoll->epfd = epoll_create(size);

    if (epoll->epfd < 0) {
        SYSERR("Can't create epoll descriptor\n");
        exit(1);
    }

    epoll->size   = size;
    epoll->events = g_new0(struct epoll_event, size);

    return epoll;
}

void sd_epoll_free(SDepoll *epoll) {
    close(epoll->epfd);
    free(epoll->events);
    free(epoll);
}

void sd_epoll_ctl(SDepoll *epoll, int ctl, int fd, void *ptr, uint32_t events) {
    struct epoll_event ev = {};
    ev.events   = events;
    ev.data.ptr = ptr;

    if (ctl == EPOLL_CTL_ADD) {
        if (epoll_ctl(epoll->epfd, ctl, fd, &ev) < 0) {
            SYSERR("Can't add fd to epoll fd\n");
            exit(1);
        }
    }
    else if (ctl == EPOLL_CTL_DEL) {
        if (epoll_ctl(epoll->epfd, ctl, fd, &ev) < 0) {
            SYSERR("Can't del fd to epoll fd\n");
        }
    }
    else {
        if (epoll_ctl(epoll->epfd, ctl, fd, &ev) < 0)
            SYSERR("Can't change epoll fd\n");
    }
}

int sd_epoll_wait(SDepoll *epoll, int timeout) {
    int nr_events;

    nr_events = epoll_wait(epoll->epfd, epoll->events, epoll->size, timeout);

    return nr_events;
}

void *sd_epoll_event_dataptr(SDepoll *epoll, int i) {
    return epoll->events[i].data.ptr;
}

uint32_t  sd_epoll_event_events(SDepoll *epoll, int i) {
    return epoll->events[i].events;
}

gchar *sd_epoll_event_str(uint32_t ev, gchar *buf) {

    sprintf(buf, "%s%s%s%s%s%s",
        (ev & EPOLLIN)      ? "IN "     : "",
        (ev & EPOLLOUT)     ? "OUT "    : "",
        (ev & EPOLLRDHUP)   ? "RDHUP "  : "",
        (ev & EPOLLPRI)     ? "PRI "    : "",
        (ev & EPOLLERR)     ? "ERR "    : "",
        (ev & EPOLLHUP)     ? "RDHUP"   : "");

    return g_strchomp(buf);
}
