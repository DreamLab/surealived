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
#include <maincfg.h>
#include <logging.h>

static GHashTable *Cfg = NULL;

/* === GLOBAL VARIABLES === */

/* Logging */
gint        G_logfd               = 1;
gchar      *G_logfname            = NULL;
gboolean    G_use_log             = TRUE;
int         G_logging             = -1;
gboolean    G_use_syslog          = FALSE;
gboolean    G_use_tm_in_syslog    = FALSE;

/* ipvssync */
gboolean    G_no_sync             = FALSE;
gchar      *G_lock_sync_file      = NULL;
gchar      *G_full_sync_file      = NULL;
gchar      *G_full_reload_file    = NULL;
gchar      *G_diff_sync_dir       = NULL;

inline static void _set_default(GHashTable *ht, gchar *key, gchar *value) {
    g_hash_table_insert(ht, g_strdup(key), g_strdup(value));
}

gboolean maincfg_new(gchar *fname) {
    FILE *in;
    gchar *logfname;
    gchar line[BUFSIZ];
    gchar key[32], value[1024];

    if (Cfg)
        g_hash_table_destroy(Cfg);

    Cfg = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    assert(Cfg);

    /* setting defaults */
    _set_default(Cfg, "ipvssync_log",       "/var/log/surealived/ipvssync.log");
    _set_default(Cfg, "use_log",            "false");
    _set_default(Cfg, "ipvssync_logging",   "debug");
    _set_default(Cfg, "use_syslog",         "false");
    _set_default(Cfg, "use_tm_in_syslog",   "false");
    _set_default(Cfg, "no_sync",            "false");
    _set_default(Cfg, "lock_sync_file",     "/var/lib/surealived/ipvsfull.lock");
    _set_default(Cfg, "full_sync_file",     "/var/lib/surealived/ipvsfull.cfg");
    _set_default(Cfg, "full_reload_file",   "/var/lib/surealived/ipvsfull.reload");
    _set_default(Cfg, "diff_sync_dir",      "/var/lib/surealived/diffs");

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

    /* Open log file (global flog) - if not already set to stderr */
    G_logfname = g_hash_table_lookup(Cfg, "ipvssync_log");
    if (!strcmp(G_logfname, "stderr"))
        G_logfd = STDERR_FILENO;

    if (toupper(((gchar *)g_hash_table_lookup(Cfg, "use_log"))[0]) == 'T')
        G_use_log = TRUE;

    if (toupper(((gchar *)g_hash_table_lookup(Cfg, "use_syslog"))[0]) == 'T')
        G_use_syslog = TRUE;

    if (toupper(((gchar *)g_hash_table_lookup(Cfg, "use_tm_in_syslog"))[0]) == 'T')
        G_use_tm_in_syslog = TRUE;


    log_init(&G_logfd, G_logfname, G_use_log, G_use_syslog, G_use_tm_in_syslog, "ipvssync");

    /* Set global variable: logging */
    if (G_logging < 0)
        G_logging = log_level_no(g_hash_table_lookup(Cfg, "ipvssync_logging"));

    log_message(2, TRUE, TRUE, "Starting ipvssync");
    log_message(2, TRUE, TRUE, "logging level: %d", G_logging);
    log_message(2, TRUE, TRUE, "logging level: %s", log_level_str(G_logging));
    if (logfname)
        log_message(2, TRUE, TRUE, "log file: %s", logfname);

    if (toupper(((gchar *)g_hash_table_lookup(Cfg, "no_sync"))[0]) == 'T')
        G_no_sync = TRUE;
    G_lock_sync_file = g_hash_table_lookup(Cfg, "lock_sync_file");
    G_full_sync_file = g_hash_table_lookup(Cfg, "full_sync_file");
    G_full_reload_file = g_hash_table_lookup(Cfg, "full_reload_file");
    G_diff_sync_dir = g_hash_table_lookup(Cfg, "diff_sync_dir");
    
    return in ? TRUE : FALSE;
}

static void _print(gpointer key, gpointer value, gpointer userdata) {
    LOGDEBUG(" * key: %s, value: %s", (char *) key, (char *) value);
}

void maincfg_print_to_log(void) {
    LOGDEBUG("MainCFG:");
    g_hash_table_foreach(Cfg, _print, NULL);
}

gchar *maincfg_get_value(gchar *key) {
    return g_hash_table_lookup(Cfg, key);
}

