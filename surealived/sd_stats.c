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
#define FORMAT64 "%" G_GINT64_FORMAT

//static GTree *StatsT = NULL;

void sd_stats_dump_save(GPtrArray *VCfgArr) {
    FILE         *dump;
    CfgVirtual   *virt;
    CfgReal      *real;
    VirtualStats *vs;
    RealStats    *rs;
    gint          vi, ri;

    if (!G_gather_stats)
        return;
    
    dump = fopen(G_stats_dump, "w");

    if (!dump)
        LOGWARN("Unable to open stats dump file - %s", G_stats_dump);
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

            fprintf(dump, "%d - %d:%d:%d - %d:%d:%d - " FORMAT64 ":" FORMAT64 ":" FORMAT64 " - " FORMAT64 ":" FORMAT64 ":" FORMAT64 " - %d:%d:%d\n",
                    rs->total,
                    rs->avg_conntime_ms, rs->avg_resptime_ms, rs->avg_totaltime_ms,
                    rs->avg_conntime_us, rs->avg_resptime_us, rs->avg_totaltime_us,
                    rs->total_conntime_ms, rs->total_resptime_ms, rs->total_totaltime_ms, 
                    rs->total_conntime_us, rs->total_resptime_us, rs->total_totaltime_us, 
                    rs->conn_problem, rs->arp_problem, rs->rst_problem);
        }
        fprintf(dump, "\n");
    }

    fclose(dump);
}

void sd_stats_dump_merge(GPtrArray *VCfgArr, GHashTable *VCfgHash) {
    FILE         *dump;
    CfgVirtual   *virt;
    CfgReal      *real;
//    VirtualStats *vs;
//    RealStats    *rs;
//    gint          vi, ri;
    gchar         buf[BUFSIZ];

    gchar         vkey[64], raddr[32], rkey[96];
    gint          vsamples, vtotal;
    gint          vavg_conntime_ms, vavg_resptime_ms, vavg_totaltime_ms;
    gint          vavg_conntime_us, vavg_resptime_us, vavg_totaltime_us;
    gint          vconn_problem, varp_problem, vrst_problem;

    gint          rtotal;
    gint          ravg_conntime_ms, ravg_resptime_ms, ravg_totaltime_ms;
    gint          ravg_conntime_us, ravg_resptime_us, ravg_totaltime_us;
    gint64        rtotal_conntime_ms, rtotal_resptime_ms, rtotal_totaltime_ms;
    gint64        rtotal_conntime_us, rtotal_resptime_us, rtotal_totaltime_us;
    gint          rconn_problem, rarp_problem, rrst_problem;

    if (!G_gather_stats)
        return;
    
    dump = fopen(G_stats_dump, "r");

    if (!dump) {
        LOGWARN("Unable to open stats dump file - %s", G_stats_dump);
        return;
    }
    assert(dump);

    LOGINFO("Processing stats file [%s]", G_stats_dump);

    while (fgets(buf, BUFSIZ, dump)) {
        int len = strlen(buf);
        if (len)
            buf[len-1] = '\0';

        /* Restore virtual statistics */
        if (sscanf(buf, "vstats %s - %d %d - %d:%d:%d - %d:%d:%d - %d:%d:%d",
                   vkey, 
                   &vsamples, &vtotal,
                   &vavg_conntime_ms, &vavg_resptime_ms, &vavg_totaltime_ms,
                   &vavg_conntime_us, &vavg_resptime_us, &vavg_totaltime_us,
                   &vconn_problem, &varp_problem, &vrst_problem) == 12) {
            LOGDEBUG("vstats found [vkey = %s]", vkey);

            virt = sd_vcfg_hashmap_lookup_virtual(VCfgHash, vkey);
            if (!virt)
                continue;

            VirtualStats *vs = &virt->stats;
            vs->total             = vtotal;
            vs->avg_conntime_ms   = vavg_conntime_ms;
            vs->avg_resptime_ms   = vavg_resptime_ms;
            vs->avg_totaltime_ms  = vavg_totaltime_ms;
            vs->avg_conntime_us   = vavg_conntime_us;
            vs->avg_resptime_us   = vavg_resptime_us;
            vs->avg_totaltime_us  = vavg_totaltime_us;
            vs->conn_problem      = vconn_problem;
            vs->arp_problem       = varp_problem;
            vs->rst_problem       = vrst_problem;
        }

        /* Restore real statistics */
        if (sscanf(buf, "rstats %s - %d - %d:%d:%d - %d:%d:%d - " FORMAT64 ":" FORMAT64 ":" FORMAT64 " - " FORMAT64 ":" FORMAT64 ":" FORMAT64 " - %d:%d:%d",
                   raddr,
                   &rtotal,
                   &ravg_conntime_ms, &ravg_resptime_ms, &ravg_totaltime_ms,
                   &ravg_conntime_us, &ravg_resptime_us, &ravg_totaltime_us,
                   &rtotal_conntime_ms, &rtotal_resptime_ms, &rtotal_totaltime_ms, 
                   &rtotal_conntime_us, &rtotal_resptime_us, &rtotal_totaltime_us, 
                   &rconn_problem, &rarp_problem, &rrst_problem) == 17) {

            snprintf(rkey, 96, "%s %s", vkey, raddr);
            LOGDEBUG("rstats found [rname = %s, rkey = %s]", raddr, rkey);

            real = sd_vcfg_hashmap_lookup_real(VCfgHash, rkey);
            if (!real)
                continue;

            RealStats *rs = &real->stats;
            rs->total = rtotal;
            rs->avg_conntime_ms    = ravg_conntime_ms;
            rs->avg_resptime_ms    = ravg_resptime_ms;
            rs->avg_totaltime_ms   = ravg_totaltime_ms;
            rs->avg_conntime_us    = ravg_conntime_us;
            rs->avg_resptime_us    = ravg_resptime_us;
            rs->avg_totaltime_us   = ravg_totaltime_us;
            rs->total_conntime_ms  = rtotal_conntime_ms;
            rs->total_resptime_ms  = rtotal_resptime_ms;
            rs->total_totaltime_ms = rtotal_totaltime_ms;
            rs->total_conntime_us  = rtotal_conntime_us;
            rs->total_resptime_us  = rtotal_resptime_us;
            rs->total_totaltime_us = rtotal_totaltime_us;
            rs->conn_problem       = rconn_problem;
            rs->arp_problem        = rarp_problem;
            rs->rst_problem        = rrst_problem;
        }
//        LOGINFO("buf = [%s]", buf);
    }

    fclose(dump);
}


void sd_stats_update_real(CfgReal *real) {
    RealStats *s = &real->stats;
    gint i;

    if (!G_gather_stats)
        return;

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

    /* total us/ms for all real tests */
    s->total_conntime_ms  += s->conntime_ms_arr[s->arridx];
    s->total_resptime_ms  += s->resptime_ms_arr[s->arridx];
    s->total_totaltime_ms += s->totaltime_ms_arr[s->arridx];
    s->total_conntime_us  += s->conntime_us_arr[s->arridx];
    s->total_resptime_us  += s->resptime_us_arr[s->arridx];
    s->total_totaltime_us += s->totaltime_us_arr[s->arridx];

    s->total_avg_conntime_ms  = s->total_conntime_ms / s->total;
    s->total_avg_resptime_ms  = s->total_resptime_ms / s->total;
    s->total_avg_totaltime_ms = s->total_totaltime_ms / s->total;
    s->total_avg_conntime_us  = s->total_conntime_us / s->total;
    s->total_avg_resptime_us  = s->total_resptime_us / s->total;
    s->total_avg_totaltime_us = s->total_totaltime_us / s->total;
    /* --- */
    
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

/*
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
*/
    s->arridx = (s->arridx + 1) % real->tester->stats_samples;

//    LOGINFO("Arridx = %d, stats_samples = %d", s->arridx, real->tester->stats_samples);
}

void sd_stats_update_virtual(CfgVirtual *virt) {
    VirtualStats *s = &virt->stats;
    CfgReal *real;
    gint i, rlen;

    if (!G_gather_stats)
        return;

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
        /*
        LOGINFO("VReal: %s, avg ms: %d,%d,%d, avg us: %d,%d,%d",
                real->name, 
                real->stats.avg_conntime_ms, real->stats.avg_resptime_ms, real->stats.avg_totaltime_ms,
                real->stats.avg_conntime_us, real->stats.avg_resptime_us, real->stats.avg_totaltime_us);
        */
    }

    if (rlen) {
        s->avg_conntime_ms  /= rlen;
        s->avg_resptime_ms  /= rlen;
        s->avg_totaltime_ms /= rlen;
        s->avg_conntime_us  /= rlen;
        s->avg_resptime_us  /= rlen;
        s->avg_totaltime_us /= rlen;
    }

/*
    LOGINFO("Virtual: %s, rlen = %d, avg ms: %d,%d,%d, avg us: %d,%d,%d",
            virt->name, 
            rlen,
            s->avg_conntime_ms, s->avg_resptime_ms, s->avg_totaltime_ms,
            s->avg_conntime_us, s->avg_resptime_us, s->avg_totaltime_us);

    LOGINFO(" * conn problem: %d, arp problem: %d, rst problem %d",
            s->conn_problem, s->arp_problem, s->rst_problem);
*/              
}
