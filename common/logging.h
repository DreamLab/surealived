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

#if !defined __LOGGING_H
#define __LOGGING_H

#define __USE_LARGEFILE64
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <time.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <netdb.h>

int log_level_no(char *log);
void log_message(int loglev, int timeline, int newline, char *format, ...);
gchar *log_level_str(gint loglev);

#define LOGLEV_ERROR  0
#define LOGLEV_WARN   1
#define LOGLEV_INFO   2
#define LOGLEV_DEBUG  3
#define LOGLEV_DETAIL 4

#ifndef MAXPATHLEN
#define MAXPATHLEN 512
#endif

/* F(ull)LOG* macros */
#define FLOGERROR(timeline, newline, fmt...) G_STMT_START { if (G_logging >= LOGLEV_ERROR) \
            log_message(LOGLEV_ERROR, timeline, newline, fmt); } G_STMT_END
#define FLOGWARN(timeline, newline, fmt...)  G_STMT_START { if (G_logging >= LOGLEV_WARN) \
            log_message(LOGLEV_WARN, timeline, newline, fmt); } G_STMT_END
#define FLOGINFO(timeline, newline, fmt...)  G_STMT_START { if (G_logging >= LOGLEV_INFO) \
            log_message(LOGLEV_INFO, timeline, newline, fmt); } G_STMT_END
#define FLOGDEBUG(timeline, newline, fmt...) G_STMT_START { if (G_logging >= LOGLEV_DEBUG) \
            log_message(LOGLEV_DEBUG, timeline, newline, fmt); } G_STMT_END
#define FLOGDETAIL(timeline, newline, fmt...) G_STMT_START { if (G_logging >= LOGLEV_DETAIL) \
            log_message(LOGLEV_DETAIL, timeline, newline, fmt); } G_STMT_END

/* single line wrappers */
#define LOGERROR(fmt...) FLOGERROR(TRUE, TRUE, fmt)
#define LOGWARN(fmt...)  FLOGWARN(TRUE, TRUE, fmt)
#define LOGINFO(fmt...)  FLOGINFO(TRUE, TRUE, fmt)
#define LOGDEBUG(fmt...) FLOGDEBUG(TRUE, TRUE, fmt)
#define LOGDETAIL(fmt...) FLOGDETAIL(TRUE, TRUE, fmt)

#define STRERROR strerror(errno)
#define SYSERR(s)   fprintf(stderr, "%s: [%s]\n", s, strerror(errno))

#define GBOOLSTR(v) ((v) == TRUE ? "TRUE" : "FALSE")

#endif
