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
