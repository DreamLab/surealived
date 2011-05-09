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

#if !defined __SD_TESTER_H
#define __SD_TESTER_H

#include "common.h"
#include <glib.h>
#include <sd_socket.h>
#include <sd_epoll.h>
#include <sd_defs.h>
#include <modloader.h>

typedef struct {
    GPtrArray      *VCfgArr;
    guint           realcount;  
    struct timeval  not_before; /* time to loop */
} SDTester;

#define PRINTDIFFTIME(info, tvs, tve)   fprintf(stderr, "%s time: %ld : %ld, diff=%ld\n", \
        info,                                                           \
        (int) (tve).tv_sec-(tvs).tv_sec,                                \
        (int) (tve).tv_usec-(tvs).tv_usec,                              \
        (long int) (((tve).tv_sec-(tvs).tv_sec)*1000000 + ((tve).tv_usec-(tvs).tv_usec)))

extern gboolean stop; 

extern guint G_stats_test_success;
extern guint G_stats_test_failed;
extern guint G_stats_online_set;
extern guint G_stats_offline_set;
extern guint G_stats_conn_problem;
extern guint G_stats_arp_problem;
extern guint G_stats_rst_problem;
extern guint G_stats_bytes_rcvd;
extern guint G_stats_bytes_sent;

SDTester   *sd_tester_create(GPtrArray *VCfgArr);
gint        sd_tester_master_loop(SDTester *sdtest);
void        sd_tester_debug(SDTester *sdtest);

#endif
