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

#include <common.h>
#include <sd_defs.h>
#include <glib.h>
#include <sys/file.h>

//static GTree *StatsT = NULL;

void sd_stats_dump_save(GPtrArray *VCfgArr) {
    FILE         *dump;
    CfgVirtual   *virt;
    CfgReal      *real;
    VirtualStats *vs;
    RealStats    *rs;
    gint          vi, ri;
    
    dump = fopen(G_stats_dump, "w");

    if (!dump)
        LOGWARN("Unable to open dump file - %s", G_stats_dump);
    assert(dump);

    for (vi = 0; vi < VCfgArr->len; vi++) {
        virt = g_ptr_array_index(VCfgArr, vi);
        vs = &virt->stats;

        fprintf(dump, "vstats %s:%d:%d:%u - ",
                virt->addrtxt, ntohs(virt->port), 
                virt->ipvs_proto, virt->ipvs_fwmark);

        fprintf(dump, "%d %d - %d:%d:%d - %d:%d:%d - %d:%d:%d\n",
                virt->tester->stats_samples,
                vs->total,
                vs->avg_conntime_ms, vs->avg_resptime_ms, vs->avg_totaltime_ms,
                vs->avg_conntime_us, vs->avg_resptime_us, vs->avg_totaltime_us,
                vs->conn_problem, vs->arp_problem, vs->rst_problem
            );

        if (!virt->realArr) /* ignore empty virtuals */
            continue;

        for (ri = 0; ri < virt->realArr->len; ri++) {
            real = g_ptr_array_index(virt->realArr, ri);
            rs = &real->stats;

            fprintf(dump, "rstats %s:%d - ", real->addrtxt, ntohs(real->port));

            fprintf(dump, "%d - %d:%d:%d - %d:%d:%d - %d:%d:%d\n",
                    rs->total,
                    rs->avg_conntime_ms, vs->avg_resptime_ms, vs->avg_totaltime_ms,
                    rs->avg_conntime_us, vs->avg_resptime_us, vs->avg_totaltime_us,
                    rs->conn_problem, vs->arp_problem, vs->rst_problem);
        }
        fprintf(dump, "\n");
    }

    fclose(dump);
}

void sd_stats_update_real(CfgReal *real) {
    RealStats *s = &real->stats;
    gint i;

    if (s->arrlen < real->tester->stats_samples)
        s->arrlen++;

    s->total++;

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

void sd_stats_update_virtual(CfgVirtual *virt) {
    VirtualStats *s = &virt->stats;
    CfgReal *real;
    gint i, rlen;

    s->total++; //update how many tests were performed 
    s->avg_conntime_ms  = 0;
    s->avg_resptime_ms  = 0;
    s->avg_totaltime_ms = 0;
    s->avg_conntime_us  = 0;
    s->avg_resptime_us  = 0;
    s->avg_totaltime_us = 0;

    rlen = virt->realArr->len;
    for (i = 0; i < rlen; i++) {
        real = g_ptr_array_index(virt->realArr, i);
        s->avg_conntime_ms  += real->stats.avg_conntime_ms;
        s->avg_resptime_ms  += real->stats.avg_resptime_ms;
        s->avg_totaltime_ms += real->stats.avg_totaltime_ms;
        s->avg_conntime_us  += real->stats.avg_conntime_us;
        s->avg_resptime_us  += real->stats.avg_resptime_us;
        s->avg_totaltime_us += real->stats.avg_totaltime_us;

        if (real->stats.last_conntime_ms == -1)
            s->conn_problem++;
        else if (real->stats.last_conntime_ms == -2)
            s->arp_problem++;
        else if (real->stats.last_conntime_ms == -3)
            s->rst_problem++;
        
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

    LOGINFO(" * conn problem: %d, arp problem: %d, rst problem %d",
            s->conn_problem, s->arp_problem, s->rst_problem);
                
}
