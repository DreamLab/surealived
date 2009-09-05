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
#include <maincfg.h>
#include <logging.h>

static GHashTable *Cfg = NULL;

/* === GLOBAL VARIABLES === */

/* Logging */
FILE       *G_flog                = NULL;
int         G_logging             = -1;

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
    _set_default(Cfg, "ipvssync_logging",   "debug");
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
    if (!G_flog) {
        logfname = g_hash_table_lookup(Cfg, "ipvssync_log");
        if (!strcmp(logfname, "stderr")) {
            G_flog = stderr;
        } else {
            G_flog = fopen(logfname, "a+");
            if (!G_flog) {
                fprintf(stderr, "Can't open log file: %s. Exiting\n", logfname);
                exit(1);
            }
            setlinebuf(G_flog);
        }
    }

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

