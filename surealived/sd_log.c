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

#include <common.h>
#include <sd_defs.h>
#include <glib.h>
#include <sys/file.h>
#include <sd_maincfg.h>
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

static void sd_log_update_real_stats(CfgReal *real) {
    RealStats *s = &real->stats;
    gint i;

    if (s->arrlen < real->tester->stats_samples)
        s->arrlen++;

    s->last_conntime_ms  = TIMEDIFF_MS(real->start_time, real->conn_time);
    s->last_resptime_ms  = TIMEDIFF_MS(real->conn_time,  real->end_time);
    s->last_totaltime_ms = TIMEDIFF_MS(real->start_time, real->end_time);
    s->last_conntime_us  = TIMEDIFF_US(real->start_time, real->conn_time);
    s->last_resptime_us  = TIMEDIFF_US(real->conn_time,  real->end_time);
    s->last_totaltime_us = TIMEDIFF_US(real->start_time, real->end_time);

    s->conntime_ms_arr[s->arridx]  = 
        (s->last_conntime_ms < 0) ? s->last_totaltime_ms : s->last_conntime_ms;
    s->resptime_ms_arr[s->arridx]  = s->last_resptime_ms;
    s->totaltime_ms_arr[s->arridx] = s->last_totaltime_ms;
    s->conntime_us_arr[s->arridx]  =
        (s->last_conntime_us < 0) ? s->last_totaltime_us : s->last_conntime_us;
    s->resptime_us_arr[s->arridx]  = s->last_resptime_us;
    s->totaltime_us_arr[s->arridx] = s->last_totaltime_us;

    s->avg_conntime_ms = s->avg_resptime_ms = s->avg_totaltime_ms = 0;
    s->avg_conntime_us = s->avg_resptime_us = s->avg_totaltime_us = 0;
    for (i = 0; i < s->arrlen; i++) {
        s->avg_conntime_ms  += s->conntime_ms_arr[i];
        s->avg_resptime_ms  += s->resptime_ms_arr[i];
        s->avg_totaltime_ms += s->totaltime_ms_arr[i];
        s->avg_conntime_us  += s->conntime_us_arr[i];
        s->avg_resptime_us  += s->resptime_us_arr[i];
        s->avg_totaltime_us += s->totaltime_us_arr[i];
    }
    s->avg_conntime_ms  /= s->arrlen;
    s->avg_resptime_ms  /= s->arrlen;
    s->avg_totaltime_ms /= s->arrlen;
    s->avg_conntime_us  /= s->arrlen;
    s->avg_resptime_us  /= s->arrlen;
    s->avg_totaltime_us /= s->arrlen;

    for (i = 0; i < s->arrlen; i++)
        LOGINFO("i = %d, %d", i, s->conntime_ms_arr[i]);

    LOGINFO("Real: %s, avg ms: %d,%d,%d, avg us: %d,%d,%d",
            real->name, 
            s->avg_conntime_ms, s->avg_resptime_ms, s->avg_totaltime_ms,
            s->avg_conntime_us, s->avg_resptime_us, s->avg_totaltime_us);


    LOGINFO("Real: %s, last ms: %d,%d,%d, last us: %d,%d,%d",
            real->name, 
            s->last_conntime_ms, s->last_resptime_ms, s->last_totaltime_ms,
            s->last_conntime_us, s->last_resptime_us, s->last_totaltime_us);

    s->arridx = (s->arridx + 1) % real->tester->stats_samples;
    LOGINFO("Arridx = %d, stats_samples = %d", s->arridx, real->tester->stats_samples);
}

static void sd_log_update_virtual_stats(CfgVirtual *virt) {
    VirtualStats *s = &virt->stats;
    CfgReal *real;
    gint i, rlen;

    memset(&virt->stats, 0, sizeof(VirtualStats));
    rlen = virt->realArr->len;
    for (i = 0; i < rlen; i++) {
        real = g_ptr_array_index(virt->realArr, i);
        s->avg_conntime_ms  += real->stats.avg_conntime_ms;
        s->avg_resptime_ms  += real->stats.avg_resptime_ms;
        s->avg_totaltime_ms += real->stats.avg_totaltime_ms;
        s->avg_conntime_us  += real->stats.avg_conntime_us;
        s->avg_resptime_us  += real->stats.avg_resptime_us;
        s->avg_totaltime_us += real->stats.avg_totaltime_us;
        
        LOGINFO("VReal: %s, avg ms: %d,%d,%d, avg us: %d,%d,%d",
                real->name, 
                real->stats.avg_conntime_ms, real->stats.avg_resptime_ms, real->stats.avg_totaltime_ms,
                real->stats.avg_conntime_us, real->stats.avg_resptime_us, real->stats.avg_totaltime_us);

    }

    if (rlen) {
        s->avg_conntime_ms  /= rlen;
        s->avg_resptime_ms  /= rlen;
        s->avg_totaltime_ms /= rlen;
        s->avg_conntime_us  /= rlen;
        s->avg_resptime_us  /= rlen;
        s->avg_totaltime_us /= rlen;
    }

    LOGINFO("Virtual: %s, rlen = %d, avg ms: %d,%d,%d, avg us: %d,%d,%d",
            virt->name, 
            rlen,
            s->avg_conntime_ms, s->avg_resptime_ms, s->avg_totaltime_ms,
            s->avg_conntime_us, s->avg_resptime_us, s->avg_totaltime_us);
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

        sd_log_update_real_stats(real);

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

    sd_log_update_virtual_stats(virt);

    human_time(htime, sizeof(htime), virt->end_time);
    for (j = 0; j < 2; j++)
        if (farr[j]){
            fprintf(farr[j], "END   Virtual: %s end time=%s\n--------------\n", virt->name, htime);
            fclose(farr[j]);
        }
    return 0;
}
