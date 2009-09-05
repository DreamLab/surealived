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

SDTester   *sd_tester_create(GPtrArray *VCfgArr);
gint        sd_tester_master_loop(SDTester *sdtest);
void        sd_tester_debug(SDTester *sdtest);

#endif
