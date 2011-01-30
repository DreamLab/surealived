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

#include <sdversion.h>
#include <sd_maincfg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>

static GHashTable *Cfg = NULL;

extern gboolean G_test_config;

/* === GLOBAL VARIABLES === */
/* Maximum file descriptors available */
gint        G_maxfd              = 0;

/* Logging */
gint        G_logfd               = 1;
gchar      *G_logfname            = NULL;
gboolean    G_use_log             = FALSE;
int         G_logging             = -1;
gboolean    G_use_syslog          = FALSE;
gboolean    G_use_tm_in_syslog    = FALSE;

/* epoll, master loop and startup behavior */
gint        G_epoll_size          = 0;
guint       G_loop_interval_ms    = 0;
guint       G_epoll_interval_ms   = 0;
guint       G_startup_delay_ms    = 0;

/* Communication debug for specified testers */
gint        G_debug_comm          = FALSE;
gchar      *G_debug_comm_path     = NULL;

/* Statistics */ 
gchar      *G_stats_dir           = NULL;
gboolean    G_log_stats           = FALSE;
gboolean    G_log_stats_combined  = FALSE;

/* ipvssync */
gboolean    G_no_sync             = FALSE; /* create data for ipvssync */
gchar      *G_lock_sync_file      = NULL;
gchar      *G_full_sync_file      = NULL;
gchar      *G_full_reload_file    = NULL;
gchar      *G_diff_sync_dir       = NULL;

/* Using offline.dump */
gchar      *G_offline_dump        = NULL;
gboolean    G_use_offline_dump    = TRUE;

/* override.dump */
gchar      *G_override_dump       = NULL;

/* stats.dump */
gboolean    G_gather_stats        = TRUE;
gchar      *G_stats_dump          = NULL;
gint        G_stats_dump_savesec  = 60;

/* notify.dump */
gchar      *G_notify_dump         = NULL;

/* Cmd interface variables */
gchar      *G_listen_addr         = NULL;
u_int16_t   G_listen_port         = 1337;

/* For watchdog (tester process memlimit) */
u_int32_t   G_memlimit            = 0;  /* unlimited */

inline static void _set_default(GHashTable *ht, gchar *key, gchar *value) {
    g_hash_table_insert(ht, g_strdup(key), g_strdup(value));
}

gboolean sd_maincfg_new(gchar *fname) {
    FILE *in;
//    gchar *logfname;
    gchar line[BUFSIZ];
    gchar key[32], value[1024];
    gint currnfd, nfd;
    struct rlimit lim;

    if (Cfg)
        g_hash_table_destroy(Cfg);

    Cfg = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    assert(Cfg);

    /* setting defaults */
    _set_default(Cfg, "maxfd",              "1024");
    _set_default(Cfg, "log",                "/var/log/surealived/surealived.log");
    _set_default(Cfg, "use_log",            "false");
    _set_default(Cfg, "logging",            "info");
    _set_default(Cfg, "use_syslog",         "false");
    _set_default(Cfg, "use_tm_in_syslog",   "false");
    _set_default(Cfg, "modules_path",       "/usr/lib/surealived/modules/");
    _set_default(Cfg, "modules",            "all");
    _set_default(Cfg, "listen_addr",        "127.0.0.1");
    _set_default(Cfg, "listen_port",        "1337");
    _set_default(Cfg, "memlimit",           "0"); /* unlimited */

    _set_default(Cfg, "loop_interval_ms",   "100");
    _set_default(Cfg, "epoll_interval_ms",  "10");
    _set_default(Cfg, "startup_delay_ms",   "1000");
    _set_default(Cfg, "debug_comm",         "0");
    _set_default(Cfg, "debug_comm_path",    "/var/log/surealived/comm");

    _set_default(Cfg, "stats_dir",          "/var/log/surealived/stats");
    _set_default(Cfg, "log_stats",          "false");
    _set_default(Cfg, "log_stats_combined", "false");

    _set_default(Cfg, "no_sync",            "false");
    _set_default(Cfg, "lock_sync_file",     "/var/lib/surealived/ipvsfull.lock");
    _set_default(Cfg, "full_sync_file",     "/var/lib/surealived/ipvsfull.cfg");
    _set_default(Cfg, "full_reload_file",   "/var/lib/surealived/ipvsfull.reload");
    _set_default(Cfg, "diff_sync_dir",      "/var/lib/surealived/diffs");

    _set_default(Cfg, "offline_dump",       "/var/lib/surealived/offline.dump");
    _set_default(Cfg, "use_offline_dump",   "true");
    _set_default(Cfg, "override_dump",      "/var/lib/surealived/override.dump");

    _set_default(Cfg, "gather_stats",       "true");
    _set_default(Cfg, "stats_dump",         "/var/lib/surealived/stats.dump");
    _set_default(Cfg, "stats_dump_savesec", "60");

    _set_default(Cfg, "notify_dump",        "/var/lib/surealived/notify.dump");

    _set_default(Cfg, "epoll_size",         "256"); 

    in = fopen(fname, "r");
    if (in) {
        while (fgets(line, BUFSIZ, in)) {
            g_strchomp(line);

            if (line[0] == '#')
                continue;

            if (sscanf(line, "%32s %1024s", key, value) == 2)
                g_hash_table_replace(Cfg, g_strdup(key), g_strdup(value));
        }

        fclose(in);
    }

    /* Do we use logging to file */
    if (toupper(((gchar *)g_hash_table_lookup(Cfg, "use_log"))[0]) == 'T')
        G_use_log = TRUE;

    /* Do we use syslog? */
    if (toupper(((gchar *)g_hash_table_lookup(Cfg, "use_syslog"))[0]) == 'T')
        G_use_syslog = TRUE;

    /* Add timestamp to syslog messages? */
    if (toupper(((gchar *)g_hash_table_lookup(Cfg, "use_tm_in_syslog"))[0]) == 'T')
        G_use_tm_in_syslog = TRUE;

    if (G_test_config) {
        G_use_syslog = FALSE;
        G_logfd = STDERR_FILENO;
    }

    G_logfname = g_hash_table_lookup(Cfg, "log");
    if (!strcmp(G_logfname, "stderr"))
        G_logfd = STDERR_FILENO;

    log_init(&G_logfd, G_logfname, G_use_log, G_use_syslog, G_use_tm_in_syslog, "surealived");

    log_message(2, TRUE, TRUE, "Starting surealived %s", VERSION);
    log_message(2, TRUE, TRUE, "logging level: %s", g_hash_table_lookup(Cfg, "logging"));
    log_message(2, TRUE, TRUE, "log file: %s", G_logfname);

    /* Set global variable: logging */
    if (G_logging < 0)
        G_logging = log_level_no(g_hash_table_lookup(Cfg, "logging"));

    /* Verify and try to setrlimit for maxfd */
    if (getrlimit(RLIMIT_NOFILE, &lim))
        LOGWARN("%s", "getrlimit error");
    else
        LOGINFO("current maxfd: (soft=%d, hard=%d)",
            (int) lim.rlim_cur, (int) lim.rlim_max);

    currnfd = (int) lim.rlim_cur;
    nfd = atoi((char *) g_hash_table_lookup(Cfg, "maxfd"));
    if (lim.rlim_max < nfd) {
        lim.rlim_cur = nfd;
        lim.rlim_max = nfd;
    }
    else
        lim.rlim_cur = nfd;

    if (setrlimit(RLIMIT_NOFILE, &lim)) {
        gchar t[16];
        sprintf(t, "%d", currnfd);
        g_hash_table_replace(Cfg, g_strdup("maxfd"), g_strdup(t));
        LOGWARN("setrlimit error: [sys=%s]", STRERROR);
    } 
    else 
        LOGINFO("changed maxfd to: (soft=%d, hard=%d)",
                (int) lim.rlim_cur, (int) lim.rlim_max);

    sscanf(g_hash_table_lookup(Cfg, "listen_port"), "%hu", &G_listen_port);
    G_listen_addr = g_hash_table_lookup(Cfg, "listen_addr");
    sscanf(g_hash_table_lookup(Cfg, "memlimit"), "%u", &G_memlimit);

    sscanf(g_hash_table_lookup(Cfg, "loop_interval_ms"), "%u", &G_loop_interval_ms);
    sscanf(g_hash_table_lookup(Cfg, "epoll_interval_ms"), "%u", &G_epoll_interval_ms);
    sscanf(g_hash_table_lookup(Cfg, "startup_delay_ms"), "%u", &G_startup_delay_ms);
    sscanf(g_hash_table_lookup(Cfg, "debug_comm"), "%u", &G_debug_comm);
    G_debug_comm_path = g_hash_table_lookup(Cfg, "debug_comm_path");

    G_stats_dir = g_hash_table_lookup(Cfg, "stats_dir");
    if (toupper(((gchar *)g_hash_table_lookup(Cfg, "log_stats"))[0]) == 'T')
        G_log_stats = TRUE;
    if (toupper(((gchar *)g_hash_table_lookup(Cfg, "log_stats_combined"))[0]) == 'T')
        G_log_stats_combined = TRUE;
    if (G_log_stats || G_log_stats_combined)
        mkdir(G_stats_dir, 0700);

    if (toupper(((gchar *)g_hash_table_lookup(Cfg, "no_sync"))[0]) == 'T')
        G_no_sync = TRUE;
    G_lock_sync_file = g_hash_table_lookup(Cfg, "lock_sync_file");
    G_full_sync_file = g_hash_table_lookup(Cfg, "full_sync_file");
    G_full_reload_file = g_hash_table_lookup(Cfg, "full_reload_file");
    G_diff_sync_dir = g_hash_table_lookup(Cfg, "diff_sync_dir");

    if (toupper(((gchar *)g_hash_table_lookup(Cfg, "use_offline_dump"))[0]) == 'T')
        G_use_offline_dump = TRUE;
    else
        G_use_offline_dump = FALSE;

    G_offline_dump = g_hash_table_lookup(Cfg, "offline_dump");
    G_override_dump = g_hash_table_lookup(Cfg, "override_dump");

    if (toupper(((gchar *)g_hash_table_lookup(Cfg, "gather_stats"))[0]) == 'T')
        G_gather_stats = TRUE;
    else
        G_gather_stats = FALSE;
    G_stats_dump = g_hash_table_lookup(Cfg, "stats_dump");
    sscanf(g_hash_table_lookup(Cfg, "stats_dump_savesec"), "%u", &G_stats_dump_savesec);

    G_notify_dump = g_hash_table_lookup(Cfg, "notify_dump");

    if (G_loop_interval_ms > G_startup_delay_ms) {
        LOGWARN("loop_interval_ms MUST BE <= startup_delay_ms, "
            "setting loop_interval_ms = startup_delay_ms/2");
        G_loop_interval_ms = G_startup_delay_ms/2;
    }

    G_epoll_size = atoi((gchar *)g_hash_table_lookup(Cfg, "epoll_size"));;
    
    return in ? TRUE : FALSE;
}

static void _print(gpointer key, gpointer value, gpointer userdata) {
    LOGDEBUG(" * key: %s, value: %s", (char *) key, (char *) value);
}

void sd_maincfg_print_to_log(void) {
    LOGDEBUG("MainCFG:");
    g_hash_table_foreach(Cfg, _print, NULL);
}

gchar *sd_maincfg_get_value(gchar *key) {
    return g_hash_table_lookup(Cfg, key);
}

