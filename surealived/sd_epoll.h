/*
 * Copyright 2009 DreamLab Onet.pl Sp. z o.o.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
