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

#if !defined __SD_CMD_H
#define __SD_CMD_H

extern SDepoll *logic_epoll;
gint sd_cmd_listen_socket_create(u_int16_t lport);
void sd_cmd_loop(GPtrArray *VCfgArr);

#endif
