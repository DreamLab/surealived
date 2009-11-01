/*
 * Copyright 2009 DreamLab Onet.pl Sp. z o.o.
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

#if !defined __SD_EPOLL_H
#define __SD_EPOLL_H

#include "common.h"
#include <glib.h>

#define EPOLL_DEFAULT_EVENTS  256
#define EPOLL_DEFAULT_TIMEOUT 100

#if !defined EPOLLRDHUP
#define EPOLLRDHUP 0x2000
#endif

typedef struct {
    int epfd;
    int size;
    struct epoll_event *events;
} SDepoll;

SDepoll        *sd_epoll_new(int size);
void            sd_epoll_free(SDepoll *epoll);
void            sd_epoll_ctl(SDepoll *epoll, int ctl, int fd, void *ptr, uint32_t events);

int             sd_epoll_wait(SDepoll *epoll, int timeout);
gchar          *sd_epoll_event_str(uint32_t ev, gchar *buf);

inline void    *sd_epoll_event_dataptr(SDepoll *epoll, int i);
inline uint32_t sd_epoll_event_events(SDepoll *epoll, int i);

#endif
