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

#if !defined __SD_MAINCFG_H
#define __SD_MAINCFG_H

#include "common.h"
#include <glib.h>

/* Global variables externs */

extern FILE       *G_flog;
extern int         G_logging;

/* epoll, master loop and startup behavior */
extern gint        G_epoll_size;
extern guint       G_loop_interval_ms;
extern guint       G_epoll_interval_ms;
extern guint       G_startup_delay_ms;

/* Communication debug for specified testers */
extern gint        G_debug_comm;
extern gchar      *G_debug_comm_path;

extern gchar      *G_stats_dir;
extern gboolean    G_log_stats;
extern gboolean    G_log_stats_combined;

extern gboolean    G_no_sync;
extern gchar      *G_lock_sync_file;
extern gchar      *G_full_sync_file;
extern gchar      *G_full_reload_file;
extern gchar      *G_diff_sync_dir;

extern gchar      *G_offline_dump;
extern gboolean    G_use_offline_dump;

extern gchar      *G_override_dump;

extern u_int16_t   G_listen_port;
extern u_int32_t   G_memlimit;


gboolean sd_maincfg_new(gchar *fname);
void     sd_maincfg_print_to_log();
gchar   *sd_maincfg_get_value(gchar *key);

#endif
