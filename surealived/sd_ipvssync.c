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
#include <sd_maincfg.h>
#include <sd_ipvssync.h>

#define FULL_SYNC_DELAY 60

struct timeval      full_sync_time = { 0, 0 };
gchar               *diff_sync_filename = NULL;
FILE                *diff_sync_file = NULL;
int                 lockfd = -1;

/* Lock when ipvssync_locks == 1, subsequent calls increment this variable
   but no flock will be performed (lock is already set)
   This allows effective "single lock" in function sd_ipvssync_diffcfg_virt,
   (which calls sd_ipvssync_diffcfg_real, which try to lock too).
   Unlock when ipvssync_locks == 0.
*/

static int          ipvssync_locks = 0;

/* ---------------------------------------------------------------------- */
static void lock_ipvssync() {
    LOGDEBUG("lock_ipvssync [%d]", ipvssync_locks);

    if (ipvssync_locks++ > 0)
        return;

    if (lockfd < 0)
        lockfd = open(G_lock_sync_file, O_CREAT, S_IRWXU | S_IRGRP | S_IROTH);

    if (lockfd < 0) {
        fprintf(stderr, "Can't open lockfile: %s [error=%s]\n", 
		G_lock_sync_file, STRERROR);
        exit(1);
    }
    flock(lockfd, LOCK_EX);            
}

static void unlock_ipvssync() {
    LOGDEBUG("unlock_ipvssync [%d]", ipvssync_locks);

    if (--ipvssync_locks > 0)
        return;

    flock(lockfd, LOCK_UN);
}

static void touch_fullreload() {
    int fd;

    LOGINFO("Full reload file touched [%s]", G_full_reload_file);

    fd = open(G_full_reload_file, O_CREAT, S_IRWXU | S_IRGRP | S_IROTH);

    if (fd < 0) {
        fprintf(stderr, "Can't touch full reload file: %s [error=%s]\n", 
		G_full_reload_file, STRERROR);
        exit(1);
    }
    close(fd);
}

gint sd_ipvssync_calculate_real_weight(CfgReal *real, gboolean apply_online_state) {
    gint        currwgt  = 0;
    gboolean    isonline;
    
    if (real->rstate == REAL_DOWN)
        isonline = FALSE;
    else if (real->rstate == REAL_OFFLINE) 
        isonline = FALSE;
    else if (real->rstate == REAL_ONLINE)
        isonline = real->online;

    /* if real is not online weight is 0 always 
       (apply_online_state == FALSE returns calculated weight as host would be online,
       useful for total reals weight sum)
    */
    if (apply_online_state && !isonline)
        currwgt = 0;
    else {
        /* use overrided weight if real->override_weight > 0 */
        if (real->override_weight > 0 && real->override_weight_in_percent)
            currwgt = MAX(1, real->ipvs_weight * real->override_weight / 100);
        else if (real->override_weight > 0 && !real->override_weight_in_percent)
            currwgt = real->override_weight;
        else 
            currwgt = real->ipvs_weight;
    }
    return currwgt;
}
/* ---------------------------------------------------------------------- */
void sd_ipvssync_diffcfg_real(CfgReal *real, gboolean override_change) {
    GString    *s;
    CfgVirtual *virt     = real->virt;
    gint        currwgt  = 0;
    gboolean    rof      = real->tester->remove_on_fail;
    gboolean    do_del = FALSE;
    gboolean    do_add = FALSE;
    gboolean    do_chg = FALSE;

    lock_ipvssync();

    LOGDEBUG("rstate = %s, last_rstate = %s, online = %s, last_online = %s",
             sd_rstate_str(real->rstate),
             sd_rstate_str(real->last_rstate),
             GBOOLSTR(real->online),
             GBOOLSTR(real->last_online));

    if (real->rstate == REAL_DOWN)
        do_del = TRUE;
    else if (real->rstate != REAL_DOWN && rof && !real->online) 
        do_del = TRUE;
    else if (real->rstate != REAL_DOWN) {
        do_add = TRUE;
        do_chg = TRUE;
    }

    currwgt = sd_ipvssync_calculate_real_weight(real, TRUE);

    s = g_string_new_len(NULL, BUFSIZ);
    
    g_string_append_printf(s, "vname=%s vproto=%s vaddr=%s vport=%d vfwmark=%d vrt=%s vsched=%s",
                           virt->name, 
                           sd_proto_str(real->virt->ipvs_proto),
                           virt->addrtxt, ntohs(virt->port), 
                           virt->ipvs_fwmark,
                           sd_rt_str(real->virt->ipvs_rt),
                           virt->ipvs_sched);

    g_string_append_printf(s, " rname=%s raddr=%s rport=%d rweight=%d",
                           real->name, 
                           real->addrtxt, ntohs(real->port),
                           currwgt);
    
    if (real->ipvs_lthresh > 0)
        g_string_append_printf(s, " rl_thresh=%d", real->ipvs_lthresh);

    if (real->ipvs_uthresh > 0)
        g_string_append_printf(s, " ru_thresh=%d", real->ipvs_uthresh);

    /* if real has rt overwrite, write rrt */
    if (virt->ipvs_rt != real->ipvs_rt)
        g_string_append_printf(s, " rrt=%s", sd_rt_str(real->ipvs_rt));
    
    g_string_append_printf(s, "\n");

    if (do_del) {
        LOGDEBUG("diffcfg: do_del [%s]", s->str);
        fprintf(diff_sync_file, "cmd=delreal ");
        fprintf(diff_sync_file, "%s", s->str);
    }

    if (do_add) {
        LOGDEBUG("diffcfg: do_add [%s]", s->str);
        fprintf(diff_sync_file, "cmd=addreal ");
        fprintf(diff_sync_file, "%s", s->str);
    }

    if (do_chg) {
        LOGDEBUG("diffcfg: do_chg [%s]", s->str);
        fprintf(diff_sync_file, "cmd=chgreal ");
        fprintf(diff_sync_file, "%s", s->str);
    }

    fflush(diff_sync_file);
    g_string_free(s, TRUE);

    real->diff = FALSE;

    unlock_ipvssync();
}

/* ---------------------------------------------------------------------- */
gint sd_ipvssync_diffcfg_virt(CfgVirtual *virt) {
    CfgReal *real;
    gint    i, updated = 0;

    lock_ipvssync();

    for (i = 0; i < virt->realArr->len; i++){
        real = (CfgReal *) g_ptr_array_index(virt->realArr, i);
        if (!real->diff)        /* nothing changed */
            continue;
        sd_ipvssync_diffcfg_real(real, FALSE);
        updated++;
    }

    unlock_ipvssync();
            
    return updated;
}

/* ---------------------------------------------------------------------- */
gint sd_ipvssync_save_fullcfg(GPtrArray *VCfgArr, gboolean force) {
    guint       i, j, currwgt;
    CfgVirtual  *virt;
    CfgReal     *real;
    FILE        *ipvsfile;
    gchar        buf[80];
    struct timeval  ctime;
    gboolean     save = TRUE;
    gboolean     complete = FALSE; /* all virtuals tested? */

    if (force)
        LOGINFO("Startup - full ipvs config forced");

    gettimeofday(&ctime, NULL);
    if (G_no_sync)
        return 1;

    if (!force && full_sync_time.tv_sec != 0 && !TIME_HAS_COME(full_sync_time, ctime))
        return 1;
    else
        full_sync_time = ctime;

    if (!force && !complete) { /* test if all virtuals were tested at least once */
        for (i = 0; i < VCfgArr->len ; i++)
            save &= ((CfgVirtual *) g_ptr_array_index(VCfgArr, i))->tested;
        if (!save)
            return 1;
        complete = TRUE;
    }
    if (!force && !save)
        return 1;

    time_inc(&full_sync_time, FULL_SYNC_DELAY, 0);

    lock_ipvssync();
    if (!(ipvsfile = fopen(G_full_sync_file, "w"))){
        LOGERROR(
            "\n\n@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
            "@@@ UNABLE TO OPEN FULL SYNC FILE:\n@@@ %s"
            "\n@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n", G_full_sync_file);
        exit(1);
    }

    strftime(buf, 80, "meta version=1.0 mdate=%F mtime=%T\n\n", localtime(&ctime.tv_sec));
    fprintf(ipvsfile, "%s", buf);

    if (diff_sync_filename)
        free(diff_sync_filename);
    diff_sync_filename = g_strdup_printf("%s/diff_sync.%u", 
                                         G_diff_sync_dir, 
                                         (unsigned) ctime.tv_sec);
    fprintf(ipvsfile, "DIFF file=%s\n\n", diff_sync_filename); /* save latest diff filename */

    if (diff_sync_file) {
        fprintf(diff_sync_file, "DIFF file=%s\n", diff_sync_filename); /* switch to diff */
        fclose(diff_sync_file); 
    }

    if (!(diff_sync_file = fopen(diff_sync_filename, "w"))) {
        LOGERROR(
            "\n\n@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
            "@@@ UNABLE TO OPEN DIFF SYNC FILE:\n@@@ %s"
            "\n@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n", diff_sync_filename);
        fclose(ipvsfile);
        exit(1);
    }

    for (i = 0; i < VCfgArr->len; i++) {
        virt = g_ptr_array_index(VCfgArr, i);

        fprintf(ipvsfile, "virtual vname=%s vproto=%s vaddr=%s vport=%d vrt=%s vsched=%s",
                virt->name, 
                sd_proto_str(virt->ipvs_proto),
                virt->addrtxt, ntohs(virt->port), 
                sd_rt_str(virt->ipvs_rt),
                virt->ipvs_sched);

        if (virt->ipvs_persistent)
            fprintf(ipvsfile," vpers=%u", virt->ipvs_persistent);

        if (virt->ipvs_fwmark)
            fprintf(ipvsfile," vfwmark=%d", virt->ipvs_fwmark);

        fprintf(ipvsfile, "\n");

        if (virt->realArr) {
            for (j = 0; j < virt->realArr->len; j++) {
                real = g_ptr_array_index(virt->realArr, j);
                LOGDEBUG("real %s:%s, state %s, rstate %s", 
                         real->virt->name, real->name, GBOOLSTR(real->online), sd_rstate_str(real->rstate));
                currwgt = sd_ipvssync_calculate_real_weight(real, TRUE);

                if (real->rstate == REAL_DOWN) {
                    LOGDEBUG("Real down - skipping from output");
                    continue;
                }

                if (real->online || !real->tester->remove_on_fail) {
                    fprintf(ipvsfile, "real rname=%s", real->name);
                    fprintf(ipvsfile, " raddr=%s rport=%d", real->addrtxt, ntohs(real->port));
                    fprintf(ipvsfile, " rweight=%d", currwgt);

                    if (real->ipvs_lthresh > 0)
                        fprintf(ipvsfile, " rl_thresh=%d", real->ipvs_lthresh);

                    if (real->ipvs_uthresh > 0)
                        fprintf(ipvsfile, " ru_thresh=%d", real->ipvs_uthresh);

                    /* if real has rt overwrite, write rrt */
                    if (virt->ipvs_rt != real->ipvs_rt)
                        fprintf(ipvsfile, " rrt=%s", sd_rt_str(real->ipvs_rt));

                    fprintf(ipvsfile, "\n");
                }
            }
            fprintf(ipvsfile, "\n");
        }
    }
    fclose(ipvsfile);

    if (force)
        LOGINFO("Full ipvssync config saved [%s]", G_full_sync_file);
    else 
        LOGDEBUG("Full ipvssync config saved [%s]", G_full_sync_file);

    if (force)
        touch_fullreload();

    unlock_ipvssync();

    return 0;
}
