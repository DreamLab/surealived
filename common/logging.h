/*
 * Copyright 2009-2010 DreamLab Onet.pl Sp. z o.o.
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

typedef struct {
    GQueue  *queue;
    gint     fd;
    gboolean close_fd_after_write;
    gint     maxlen;
    gint     dropped;
} MsgQueue;

typedef struct {
    guchar  *msg;
    gint     len;
    gint     pos;
} Msg;

void log_nb_queue_init(void);
MsgQueue *log_nb_queue_new(gint fd, gboolean close_fd_after_write, gint maxlen);
gboolean log_nb_queue_remove(MsgQueue *mq);
gboolean log_nb_queue_add_msg(MsgQueue *mq, guchar *msg, gint len);
void log_nb_queue_write_loop(void);
gint log_nb_queued_messages(void);
void log_nb_queue_free(void);
void log_nb_queue_debug(void);

/* ======== */
void log_init(int *logfd, gchar *logfname, gboolean use_log, gboolean use_syslog, gboolean use_tm_in_syslog, gchar *ident);
void log_rotate(int *logfd);
int log_level_no(char *log);
void log_message(int loglev, int timeline, int newline, char *format, ...);
gchar *log_level_str(gint loglev);

#define LOG_NB_FLUSH_QUEUES() G_STMT_START { while (log_nb_queued_messages()) log_nb_queue_write_loop(); } G_STMT_END

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
