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

#include <common.h>
#include <sd_defs.h>
#include <glib.h>
#include <sys/file.h>
#include <sd_maincfg.h>
#include <sd_stats.h>
#include <sd_log.h>

void human_time(gchar *out, gint len, struct timeval t) {
    char buf[32];
    strftime(buf, 32, "%F %T", localtime(&t.tv_sec));
    snprintf(out, len, "%s.%03u", buf, (guint)t.tv_usec/1000);
}

static void timediff(struct timeval *out, struct timeval t1, struct timeval t2) {
    /* this function sets out as a difference between t1 and t2: out=t2-t1 */
    gint dusec = (gint) t2.tv_usec - (gint) t1.tv_usec;

    out->tv_sec = out->tv_usec = 0;

    if ((!t1.tv_sec && !t1.tv_usec) || (!t2.tv_sec && !t2.tv_usec))
        return;

    out->tv_sec = t2.tv_sec - t1.tv_sec;

    if (dusec >= 0)
        out->tv_usec = dusec;
    else{
        out->tv_usec = 1000000 + dusec;
        out->tv_sec--;
    }
}

/*! \brief Function stores statistics for specified virtual. Logging depends on global
  variables \a log_stats and \a log_stats_combined defined in \a surealived.cfg
  \param virt Virtual which statistics should be written to disk
*/
gint sd_log_stats(CfgVirtual *virt) {
    /* here we write statistics to combined (full) file and per-virtual file */
    FILE    *file_combined  = NULL;
    FILE    *file_local     = NULL;
    CfgReal *real;
    gchar   filename[MAXPATHLEN];
    gchar   htime[32];
    gint    i, j;
    FILE    *farr[2];
    struct timeval  conn_time;

    if (G_log_stats) {
        snprintf(filename, MAXPATHLEN, "%s/sd_virtstats_%s:%d.%u", 
                 G_stats_dir, virt->name, ntohs(virt->port), (unsigned) virt->end_time.tv_sec);
        if ((file_local = fopen(filename, "w")) == NULL)
            LOGERROR("Unable to open file '%s' for writing!", filename);
    }

    if (G_log_stats_combined) {
        snprintf(filename, MAXPATHLEN, "%s/sd_fullstats.log", G_stats_dir);
        if ((file_combined = fopen(filename, "a")) == NULL)
            LOGERROR("Unable to open file '%s' for append!", filename);
    }

    if (!file_combined && !file_local)
        return 0;

    farr[0] = file_combined;
    farr[1] = file_local;

    human_time(htime, sizeof(htime), virt->start_time);
    for (j = 0; j < 2; j++)
        if (farr[j])
            fprintf(farr[j], "START Virtual: %s start time=%s\n", virt->name, htime);

    for (i = 0; i < virt->realArr->len; i++) {
        real = g_ptr_array_index(virt->realArr, i);
        human_time(htime, sizeof(htime), real->start_time);
        timediff(&conn_time, real->start_time, real->conn_time);

        sd_stats_update_real(real);

        for (j = 0; j < 2; j++) /* -funroll-loops should help in this function! */
            if (farr[j]) {
                if (virt->tester->logmicro) 
                    fprintf(farr[j],
                            "%s virt=%s:%d vproto=%s vaddr=%s:%d real=%s:%d raddr=%s:%d "
                            "conntime=%ldus resptime=%ldus totaltime=%ldus currtest=%s online=%s\n",
                            htime,
                            virt->name, ntohs(virt->port), 
                            sd_proto_str(virt->ipvs_proto),
                            virt->addrtxt, ntohs(virt->port),
                            real->name, ntohs(real->port), 
                            real->addrtxt, ntohs(real->port),    
                            TIMEDIFF_US(real->start_time, real->conn_time),
                            TIMEDIFF_US(real->conn_time,  real->end_time),  
                            TIMEDIFF_US(real->start_time, real->end_time),
                            GBOOLSTR(real->test_state),
                            GBOOLSTR(real->online));
                else 
                    fprintf(farr[j],
                            "%s virt=%s:%d vproto=%s vaddr=%s:%d real=%s:%d raddr=%s:%d "
                            "conntime=%ldms resptime=%ldms totaltime=%ldms currtest=%s online=%s\n",
                            htime,
                            virt->name, ntohs(virt->port), 
                            sd_proto_str(virt->ipvs_proto),
                            virt->addrtxt, ntohs(virt->port),
                            real->name, ntohs(real->port), 
                            real->addrtxt, ntohs(real->port),    
                            TIMEDIFF_MS(real->start_time, real->conn_time), 
                            TIMEDIFF_MS(real->conn_time,  real->end_time), 
                            TIMEDIFF_MS(real->start_time, real->end_time),
                            GBOOLSTR(real->test_state),
                            GBOOLSTR(real->online));
            }
    }

    sd_stats_update_virtual(virt);

    human_time(htime, sizeof(htime), virt->end_time);
    for (j = 0; j < 2; j++)
        if (farr[j]){
            fprintf(farr[j], "END   Virtual: %s end time=%s\n--------------\n", virt->name, htime);
            fclose(farr[j]);
        }
    return 0;
}
