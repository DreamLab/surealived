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

#if !defined __SD_IPVSSYNC_H
#define __SD_IPVSSYNC_H

gint sd_ipvssync_calculate_real_weight(CfgReal *real, gboolean apply_online_state);
void sd_ipvssync_diffcfg_real(CfgReal *real, gboolean override_change);
gint sd_ipvssync_diffcfg_virt(CfgVirtual *virt);
gint sd_ipvssync_save_fullcfg(GPtrArray *VCfgArr, gboolean force);

#endif
