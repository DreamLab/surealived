/*
 * Copyright 2009 DreamLab Onet.pl Sp. z o.o.
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

#if !defined __SD_OVERRIDE_H
#define __SD_OVERRIDE_H

void sd_override_dump_add(CfgReal *real);
void sd_override_dump_del(CfgReal *real);
gint sd_override_dump_save();
void sd_override_dump_merge(GPtrArray *VCfgArr);
guint sd_override_hash_table_size(void);

#endif
