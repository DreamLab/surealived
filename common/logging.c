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

#include <stdio.h>
#include <glib.h>
#include <string.h>
#include <sys/time.h>
#include <logging.h>

extern FILE *G_flog;
static gchar *loglevstr[] = { "error", "warn", "info", "debug", "dbdet", NULL };

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
    struct timeval tv;
    char buf[32];

    gettimeofday(&tv, NULL);
    va_list args;
    va_start (args, format);
    if (timeline) {
        strftime(buf, 32, "[%F %T", localtime(&tv.tv_sec));
        fprintf(G_flog, "%s", buf);
        fprintf(G_flog, ".%06d] ", (int) tv.tv_usec);
        fprintf(G_flog, "%-6s: ", loglevstr[loglev]);
    }
    vfprintf(G_flog, format, args);
    if (newline)
        fprintf(G_flog, "\n");
    va_end(args);
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
