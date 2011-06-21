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

#include <stdio.h>
#include <glib.h>
#include <string.h>
#include <sys/time.h>
#include <logging.h>
#include <syslog.h>
#include <poll.h>

#define TIMEDIFF_WRITE_MS(t1,t2) (((t2.tv_sec-t1.tv_sec)*1000000+(int)((int)t2.tv_usec-(int)t1.tv_usec))/1000)

/* If a write will take CRITICAL_TIME_MS we have serious problem in disk operational.
   In such a case log a critical message to a syslog and abort.
 */
#define CRITICAL_TIME_MS      1000 

/* How many blocks will be written in a single msgqueue loop 
   BYTES_TO_WRITE = MIN (MAX_WRITE_AT_ONE_LOOP * 512 (GUESSED BLOCK SIZE), ...)
*/
#define MAX_WRITE_AT_ONE_LOOP 1024

static GHashTable *queueH = NULL;
static gint queued_messages = 0;

/* ---------------------------------------------------------------------- */
static MsgQueue  *l_msgqueue = NULL;
static int       *l_logfd;
static gchar     *l_logfname;
static gboolean   l_use_log;
static gboolean   l_use_syslog;
static gboolean   l_use_tm_in_syslog;
static gchar     *l_ident;

static gchar *loglevstr[] = { "error", "warn", "info", "debug", "dbdet", NULL };

/* ====================================================================== */
/* Initialize internal messaging */
/* Purpose:
   Application will write to different files in nonblocking manner.
   For example, to write a file a function should:
   - open the file in nonblocking manner write: fd = open("/tmp/myfile.log", O_WRONLY .. | O_NONBLOCK );
   - create a queue: myqueue = log_nb_queue_new(fd, TRUE, 4096);
   - ... add messages to the queue: log_nb_queue_add_msg(myqueue, mymessage, len);
   - return 
   Later, somewhere in your code a queue writing loop should be called:
   - log_nb_queue_write_loop() - this loop will write all messages it can, 
     when a queue which has close_fd_after_write flag set to TRUE a queue will be removed
     after last success writing.

   Internally queues are kept in a hash table (each queue is a pointer so it is a good key).
*/

/* Initialize queue hash table */
void log_nb_queue_init(void) {
//    fprintf(stderr, "log_nb_queue_init\n");
    if (!queueH)
        queueH = g_hash_table_new(g_direct_hash, g_direct_equal);
}

/* Create and add a queue to hash table */
MsgQueue *log_nb_queue_new(gint fd, gboolean close_fd_after_write, gint maxlen) {
//    fprintf(stderr, "log_nb_queue_new fd = %d, close_fd = %s\n", fd, GBOOLSTR(close_fd_after_write));
    MsgQueue *mq = g_new0(MsgQueue, 1);    
    assert(mq);    

    mq->queue = g_queue_new();
    mq->fd = fd;
    mq->close_fd_after_write = close_fd_after_write;
    mq->maxlen = maxlen;
    mq->dropped = 0;

    g_hash_table_insert(queueH, mq, mq);
    return mq;
}

/* Remove a queue from hash table only when it is not empty */
gboolean log_nb_queue_remove(MsgQueue *mq) {

    /* Can't remove a queue, it's not empty */
    if (!g_queue_is_empty(mq->queue))
        return FALSE;

    /* Can't remove a queue, it's a logging queue */
    if (mq == l_msgqueue)
        return FALSE;

    if (mq->close_fd_after_write)
        close(mq->fd);

    g_queue_clear(mq->queue);
    g_queue_free(mq->queue);
    g_hash_table_remove(queueH, mq);
    free(mq);

//    fprintf(stderr, "log_nb_queue_remove\n");
    return TRUE;
}

/* Record a message
   NOTE: msg will be freed after write
 */
gboolean log_nb_queue_add_msg(MsgQueue *mq, guchar *msg, gint len) {
//    static gint nra = 0;
//    fprintf(stderr, "log_nb_queue_add_msg: %d\n", nra++);

    Msg *m;

    if (!mq)
        return FALSE;

    if (g_queue_get_length(mq->queue) == mq->maxlen)
        return FALSE;

    m = g_new(Msg, 1);
    m->msg = msg;
    m->len = len;
    m->pos = 0;
    g_queue_push_tail(mq->queue, m);

    queued_messages++;
    return TRUE;
}

/* Write messages to disk */
static void _log_nb_queue_write(gpointer key, gpointer val, gpointer userdata) {
    MsgQueue *mq = key;
    Msg      *m;
    gboolean  write_fail = FALSE;
    gint      ready, towrite, written, wnr = 0;
    struct timeval t1, t2;
    struct pollfd fds;
    
    fds.fd = mq->fd;
    fds.events = POLLOUT;    

    while (g_queue_is_empty(mq->queue) == FALSE && write_fail == FALSE && wnr++ < MAX_WRITE_AT_ONE_LOOP) {

        ready = poll(&fds, 1, 0);
        if (!ready || !(fds.revents & POLLOUT)) {
            write_fail = TRUE;
            break;
        }

        m = g_queue_peek_head(mq->queue);
        towrite = MIN(MAX_WRITE_AT_ONE_LOOP * 512, m->len - m->pos);
        
        gettimeofday(&t1, NULL);
        written = write(mq->fd, m->msg + m->pos, towrite);
        gettimeofday(&t2, NULL);
        if (TIMEDIFF_WRITE_MS(t2, t1) >= CRITICAL_TIME_MS) {
            openlog((const char *) l_ident, LOG_PID, LOG_USER);
            syslog(LOG_CRIT, "Writing to file takes too long [%d miliseconds]. Aborting!", (int) TIMEDIFF_WRITE_MS(t2, t1));
            abort();
        }

        if (m->pos + written != m->len) {
            m->pos += written;
            write_fail = TRUE;
            break;
        }
        else {
//            fprintf(stderr, "removing msg = %p, %d\n", m, queued_messages);
            g_queue_remove(mq->queue, m);
            free(m->msg);
            free(m);
            queued_messages--;
        }            
    }
    
    if (g_queue_is_empty(mq->queue))
        log_nb_queue_remove(mq);

}

void log_nb_queue_write_loop(void) {
    if (!queued_messages)
        return;        
    g_hash_table_foreach(queueH, _log_nb_queue_write, NULL);
}

gint log_nb_queued_messages(void) {
    return queued_messages;
}

void log_nb_queue_free(void) {
    g_hash_table_destroy(queueH);
}

void log_nb_queue_debug(void) {
    fprintf(stderr, "Hash table size: %d\n", g_hash_table_size(queueH));
}

/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */

void log_init(int *logfd, gchar *logfname, gboolean use_log, gboolean use_syslog, gboolean use_tm_in_syslog, gchar *ident) {
    log_nb_queue_init();

    l_logfd            = logfd;
    l_logfname         = g_strdup(logfname);
    l_use_log          = use_log;
    l_use_syslog       = use_syslog;
    l_use_tm_in_syslog = use_tm_in_syslog;

    if (l_use_syslog) {
        l_ident = ident ? g_strdup(ident) : "unknown";
        openlog((const char *) l_ident, LOG_PID, LOG_USER);
    }

    if (l_use_log) {
        if (*logfd != STDERR_FILENO) {
            *l_logfd = open(l_logfname, 
                            O_WRONLY | O_CREAT | O_APPEND | O_LARGEFILE | O_NONBLOCK,
                            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            if (*l_logfd < 0) {
                fprintf(stderr, "Can't open log file: %s. Exiting [err = %s]\n", l_logfname, STRERROR);
                exit(1);
            }
            if (!l_msgqueue)
                l_msgqueue = log_nb_queue_new(*l_logfd, FALSE, 4096);
        }
        else 
            *l_logfd = *logfd;
    }
}

void log_rotate(int *logfd) {
    if (l_use_syslog) {
        closelog();
        openlog((const char *) l_ident, LOG_PID, LOG_USER);
    }

    if (l_use_log) {
        close(*logfd);
        *logfd = open(l_logfname, 
                      O_WRONLY | O_CREAT | O_APPEND | O_LARGEFILE | O_NONBLOCK,
                      S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (*logfd < 0) {
            fprintf(stderr, "Can't open log file: %s. Exiting [err = %s]\n", l_logfname, STRERROR);
            exit(1);
        }
    }
}

/*! \brief Return an index in \a loglevstr gchar array 
  \param log A string which index we want to find out, if not found 0 is returned (error index)
  \return String index in a \a loglevstr array or 0 if not found (error index)
 */
int log_level_no(char *log) {
    gint i = 0;
    while (loglevstr[i]) {
        if (!strcmp(loglevstr[i], log))
            break;
        i++;
    }
    if (!loglevstr[i])
        return 0;

    return i;
}

/*! \brief Main and preferred way to log program messages. Avoid direct usage, use \a LOG... and \a FLOG macros instead.
  \param loglev A logging level: \a LOGLEV_(\a ERROR, \a WARN, \a INFO, \a DEBUG, \a DETAIL)
  \param timeline Append a string which contains "yyyy-mm-dd hh:mm:ss.microseconds" (if not equal 0, otherwise don't)
  \param newline Append a newline (if not equal 0, otherwise don't)
  \param format Printf like format string
  \param ... arguments for format
*/
void log_message(int loglev, int timeline, int newline, char *format, ...) {
    int priority;
    struct timeval tv;
    char buf[32];
    GString *ss = g_string_new_len(NULL, BUFSIZ);
    GString *sf = g_string_new_len(NULL, BUFSIZ);
    assert(ss);
    assert(sf);
    
    gettimeofday(&tv, NULL);
    if (timeline) {
        strftime(buf, 32, "[%F %T", localtime(&tv.tv_sec));        
        g_string_append_printf(sf, "%s.%06d] %-6s: ", buf, (int) tv.tv_usec, loglevstr[loglev]);
        if (l_use_tm_in_syslog)
            g_string_append_printf(ss, "%s.%06d] %-6s: ", buf, (int) tv.tv_usec, loglevstr[loglev]);
    }
    va_list args;
    va_start (args, format);
    g_string_append_vprintf(sf, format, args);
    va_end(args);

    va_start (args, format);
    g_string_append_vprintf(ss, format, args);    
    va_end(args);
    if (newline) {        
        if (l_use_log)
            g_string_append(sf, "\n");
        if (l_use_syslog)
            g_string_append(ss, "\n");      
    }

    if (l_use_syslog) {
        switch (loglev) {
        case LOGLEV_ERROR: 
            priority = LOG_ERR;
            break;
        case LOGLEV_WARN:
            priority = LOG_WARNING;
            break;
        case LOGLEV_INFO:
            priority = LOG_NOTICE;
            break;
        case LOGLEV_DEBUG:
            priority = LOG_INFO;
            break;
        case LOGLEV_DETAIL:
            priority = LOG_DEBUG;
            break;
        }
        syslog(priority, "%s", ss->str);
    } 

    if (l_use_log) {
        gboolean isok = TRUE;

        if (*l_logfd == STDERR_FILENO) {
            write(*l_logfd, sf->str, strlen(sf->str));
            g_free(sf->str);
        }
        else
            isok = log_nb_queue_add_msg(l_msgqueue, (guchar *) sf->str, strlen(sf->str));
        if (!isok) {
            openlog((const char *) l_ident, LOG_PID, LOG_USER);
            syslog(LOG_CRIT, "Log message queue full [maxlen = %d] - disk problems?", l_msgqueue->maxlen);
            closelog();
        }
    }  
    g_string_free(ss, TRUE);
    if (l_use_log)
        g_string_free(sf, FALSE); //message will be freed after save in log_nb_queue_write_loop()
    else 
        g_string_free(sf, TRUE); //free it now

}

/*! \brief Return ptr to log level corresponding string 
  \param loglev A logging level: \a LOGLEV_(\a ERROR, \a WARN, \a INFO, \a DEBUG, \a DETAIL)
  \return Pointer to corresponding string or "" if invalid loglev.
*/
static gchar *_empty = "";
gchar *log_level_str(gint loglev) {
    if (loglev < 0)        
        return _empty;
    else if (loglev > LOGLEV_DETAIL)
        return loglevstr[LOGLEV_DETAIL];
    return loglevstr[loglev];
}

