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

#include <glib.h>
#include <sys/file.h>
#include <config.h>
#include <ipvsfuncs.h>
#include <maincfg.h>
#include <logging.h>
#include <diffs.h>

static int lockfd = -1;

void lock_ipvssync() {
    LOGDETAIL("lock_ipvssync");
    if (lockfd < 0)
        lockfd = open(G_lock_sync_file, 0, S_IRWXU | S_IRGRP | S_IROTH);

    if (lockfd < 0) {
        fprintf(stderr, "Can't open lockfile: %s [error=%s]\n", 
		G_lock_sync_file, STRERROR);
        exit(1);
    }
    flock(lockfd, LOCK_EX);            
}

void unlock_ipvssync() {
    LOGDETAIL("unlock_ipvssync");
    flock(lockfd, LOCK_UN);
}


static void diffs_apply_change_real(Config *conf, GHashTable *ht, gchar *line, gboolean sync_to_ipvs) {
    ConfVirtual    *cv;
    ConfReal       *cr;
    gint            cvindex;
    gint            crindex;
    ipvs_service_t  svc;
    ipvs_dest_t     dest;
    gchar          *cmd;
    gint            ret;
    gboolean        addflag = FALSE;

    LOGDEBUG("*** diffs apply change real ***");

    /* Fill svc and dst */
    ret = ipvsfuncs_set_svc_from_ht(&svc, ht);
    if (!ret) {
        LOGERROR("Can't parse svc in change real line [%s]", line);
        return;
    }

    cv = config_find_virtual(conf, &svc, &cvindex);
    if (!cv) {
        LOGWARN("Can't find virtual [%s]", line);
        return;
    }

    ret = ipvsfuncs_set_dest_from_ht(&dest, ht, cv->conn_flags);
    if (!ret) {
        LOGERROR("Can't parse dest in change real line [%s]", line);
        return;
    }

    /* need to known cmd - because after delreal it will not be in ConfVirtual */
    cmd = g_hash_table_lookup(ht, "cmd");
    if (!cmd) {
        LOGERROR("cmd lookup error");
        return;
    }
    if (!strcmp(cmd, "addreal"))
        addflag = TRUE;

    cr = config_find_real(cv, &dest, &crindex);
    if (!cr && !addflag) {
        LOGWARN("Can't find real [%s]", line);
        return;
    }

    /* Ok, here we have 'svc' and 'dest' which points to real or addflag is set */
    if (!strcmp(cmd, "chgreal")) {
        LOGINFO("* CHGREAL [%s]", line);
        if (sync_to_ipvs) //at ipvssync starts apply diffs only to configuration
            ipvsfuncs_update_dest(&svc, &dest);
        cr->dest = dest;
    }
    else if (!strcmp(cmd, "delreal")) {
        LOGINFO("* DELREAL [%s]", line);
        if (sync_to_ipvs)
            ipvsfuncs_del_dest(&svc, &dest);
        config_remove_real_by_index(cv, crindex);
    }
    else if (!strcmp(cmd, "addreal")) {
        LOGINFO("* ADDREAL [%s]", line);
        if (cr)
            LOGINFO("** real already exists, skipping adding to ipvs");
        else {
            if (sync_to_ipvs)
                ipvsfuncs_add_dest(&svc, &dest);
            config_add_real(conf, cv, ht);
        }
    }
}

/* ---------------------------------------------------------------------- */
void diffs_apply(Config *conf, gboolean sync_to_ipvs) {
    gchar       line[BUFSIZ];
    gchar       fname[BUFSIZ];
	GHashTable *ht;
    gchar       cmd[16];
    FILE       *df = conf->diff_file;
    gchar      *tbuf = NULL;
    int         fd;

    while (1) {
        if (!df)
            LOGERROR("diff file == NULL!");

        fd = fileno(df);
        if (fd < 0)
            LOGERROR("diff file fd == %d!", fd);

        tbuf = fgets(line, BUFSIZ, df);
        if (feof(df)) {
            clearerr(df);
            break;
        }

        g_strchomp(line);
        LOGDEBUG("diff line: [%s]", line);

        /* jump to next diff */
        if (sscanf(line, "DIFF file=%s", fname) == 1) {
            LOGINFO("Switch to next diff [%s]", fname);
            fclose(df);
            g_free(conf->diff_filename);
            conf->diff_filename = g_strdup(fname);

            if (!(conf->diff_file = fopen(conf->diff_filename, "r"))) {
                LOGERROR("Can't open diff file [%s]", conf->diff_file);
                exit(1);
            }
            df = conf->diff_file;
            continue;
        }

        /* change/remove real */
        if (sscanf(line, "%sreal", cmd) == 1) {
            ht = config_parse_line(line);
            diffs_apply_change_real(conf, ht, line, sync_to_ipvs);
            g_hash_table_destroy(ht);
        }
    }
}

/* ---------------------------------------------------------------------- */
gint diffs_clean_old_files(Config *conf) { 
    GDir          *dir;
    const gchar   *de;
    gchar         *diffname;
    gchar         *ffname;
    gint           matched = 0;
    struct timeval ctime;

    gettimeofday(&ctime, NULL);
    if (ctime.tv_sec - conf->last_diffs_cleanup.tv_sec < DIFFS_CLEANUP_SEC) {
        LOGDETAIL("Cleanup time not reached curr[%d], last[%d], diff[%d]",
                  ctime.tv_sec, conf->last_diffs_cleanup.tv_sec,
                  ctime.tv_sec - conf->last_diffs_cleanup.tv_sec);
        return 0;
    }
        
    dir = g_dir_open(G_diff_sync_dir, 0, NULL);
    if (!dir) {
        G_logfd = STDERR_FILENO;
        LOGERROR("Can't open directory: %s", G_diff_sync_dir);
        exit(1);
    }

    diffname = g_path_get_basename(conf->diff_filename);

    while ((de = g_dir_read_name(dir))) {
        if (strcmp(de, diffname) >= 0)
            continue;
        LOGINFO("diff file [%s] < current diff [%s], removing", de, diffname);

        ffname = g_build_filename(G_diff_sync_dir, de, NULL);
        if (unlink(ffname))
            LOGERROR("Problem removing diff [%s]", ffname);
        else 
            matched++;
        g_free(ffname);
    };

    g_free(diffname);
    g_dir_close(dir);
    conf->last_diffs_cleanup = ctime;

    LOGINFO("diffs cleanup, matched %d files", matched);

    return matched;
}
