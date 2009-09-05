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

#if !defined __DIFFS_H
#define __DIFFS_H

#include <glib.h>

#define DIFFS_CLEANUP_SEC 60

void lock_ipvssync();
void unlock_ipvssync();

void diffs_apply(Config *conf, gboolean sync_to_ipvs);
gint diffs_clean_old_files(Config *conf);

#endif
