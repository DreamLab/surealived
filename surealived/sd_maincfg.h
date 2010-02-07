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

extern gboolean    G_gather_stats;
extern gchar      *G_stats_dump;
extern gint        G_stats_dump_savesec;

extern gchar      *G_listen_addr;
extern u_int16_t   G_listen_port;
extern u_int32_t   G_memlimit;


gboolean sd_maincfg_new(gchar *fname);
void     sd_maincfg_print_to_log();
gchar   *sd_maincfg_get_value(gchar *key);

#endif
