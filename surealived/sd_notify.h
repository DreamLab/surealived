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

#if !defined __SD_NOTIFY_H
#define __SD_NOTIFY_H

#include <glib.h>
#include <sd_defs.h>

void sd_notify_dump_save(GPtrArray *VCfgArr);
void sd_notify_dump_merge(GPtrArray *VCfgArr, GHashTable *VCfgHash);

void sd_notify_execute_if_required(GPtrArray *VCfgArr, CfgVirtual *virt);

void sd_notify_real(CfgReal *real, RState state);

#endif
