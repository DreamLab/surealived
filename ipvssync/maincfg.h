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

#if !defined __MAINCFG_H
#define __MAINCFG_H

#include <glib.h>

/* Global variables externs */

extern FILE       *G_flog;
extern int         G_logging;

extern gboolean    G_no_sync;
extern gchar      *G_lock_sync_file;
extern gchar      *G_full_sync_file;
extern gchar      *G_full_reload_file;
extern gchar      *G_diff_sync_dir;

gboolean maincfg_new(gchar *fname);
void     maincfg_print_to_log();
gchar   *maincfg_get_value(gchar *key);

#endif
