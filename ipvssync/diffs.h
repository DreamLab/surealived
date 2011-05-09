/*
 * Copyright 2009-2011 DreamLab Onet.pl Sp. z o.o.
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

#if !defined __DIFFS_H
#define __DIFFS_H

#include <glib.h>

#define DIFFS_CLEANUP_SEC 60

void lock_ipvssync();
void unlock_ipvssync();

void diffs_apply(Config *conf, gboolean sync_to_ipvs);
gint diffs_clean_old_files(Config *conf);

#endif
