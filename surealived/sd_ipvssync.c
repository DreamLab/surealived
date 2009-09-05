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

/* ---------------------------------------------------------------------- */
void sd_ipvssync_diffcfg_real(CfgReal *real) {
    CfgVirtual *virt    = real->virt;
    gint        currwgt = real->online ? real->ipvs_weight : 0;

    lock_ipvssync();

    if (real->tester->remove_on_fail && !real->online)
        fprintf(diff_sync_file, "cmd=delreal ");
    else if (real->tester->remove_on_fail && real->online)
        fprintf(diff_sync_file, "cmd=addreal ");
    else fprintf(diff_sync_file, "cmd=chgreal ");

    fprintf(diff_sync_file, "vname=%s vproto=%s vaddr=%s vport=%d vfwmark=%d vrt=%s vsched=%s",
            virt->name, 
            sd_proto_str(real->virt->ipvs_proto),
            virt->addrtxt, ntohs(virt->port), 
            virt->ipvs_fwmark,
            sd_rt_str(real->virt->ipvs_rt),
            virt->ipvs_sched);

    fprintf(diff_sync_file, " rname=%s raddr=%s rport=%d rweight=%d",
            real->name, 
            real->addrtxt, ntohs(real->port),
            currwgt);
    
    if (real->ipvs_lthresh > 0)
        fprintf(diff_sync_file, " rlthresh=%d", real->ipvs_lthresh);

    if (real->ipvs_uthresh > 0)
        fprintf(diff_sync_file, " ruthresh=%d", real->ipvs_uthresh);

    /* if real has rt overwrite, write rrt */
    if (virt->ipvs_rt != real->ipvs_rt)
        fprintf(diff_sync_file, " rrt=%s", sd_rt_str(real->ipvs_rt));
    
    fprintf(diff_sync_file, "\n");
    fflush(diff_sync_file);

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
        sd_ipvssync_diffcfg_real(real);
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
    fprintf(ipvsfile, buf);

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
                LOGDEBUG("real %s:%s, state %s", real->virt->name, real->name, GBOOLSTR(real->online));
                currwgt = real->online ? real->ipvs_weight : 0;

                if (real->online || !real->tester->remove_on_fail) {
                    fprintf(ipvsfile, "real rname=%s", real->name);
                    fprintf(ipvsfile, " raddr=%s rport=%d", real->addrtxt, ntohs(real->port));
                    fprintf(ipvsfile, " rweight=%d", currwgt);

                    if (real->ipvs_lthresh > 0)
                        fprintf(ipvsfile, " rlthresh=%d", real->ipvs_lthresh);

                    if (real->ipvs_uthresh > 0)
                        fprintf(ipvsfile, " ruthresh=%d", real->ipvs_uthresh);

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
