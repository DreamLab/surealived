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

#define __USE_LARGEFILE64
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <time.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <sys/file.h>

#include <sd_stats.h>
#include <sd_notify.h>
#include <sd_defs.h>
#include <sd_maincfg.h>
#include <logging.h>

void time_inc(struct timeval *, guint, guint);

#ifndef MAXPATHLEN
#define MAXPATHLEN 512
#endif

gboolean sd_unlink_commlog(CfgReal *real);
gboolean sd_append_to_file(gchar *fname, gchar *buf, gint bufsiz, gchar *usertxt);
gboolean sd_append_to_commlog(CfgReal *real, gchar *buf, gint bufsiz, gchar *usertxt);
gchar *sd_proto_str(SDIPVSProtocol proto);
SDIPVSProtocol sd_proto_no(gchar *str);
gchar *sd_rt_str(SDIPVSRt rt);
gchar *sd_rstate_str(RState st);
RState sd_rstate_no(gchar *str);
gchar *sd_nstate_str(NotifyState st);
NotifyState sd_nstate_no(gchar *str);

GHashTable *sd_parse_line(gchar *line);

/* create hash table which contains mapping string -> virtual/real */
GHashTable *sd_vcfg_hashmap_new(GPtrArray *VCfgArr);
CfgVirtual *sd_vcfg_hashmap_lookup_virtual(GHashTable *VCfgHash, gchar *vkey);
CfgReal *sd_vcfg_hashmap_lookup_real(GHashTable *VCfgHash, gchar *rkey);
gint sd_vcfg_reals_weight_sum(CfgVirtual *virt, gboolean apply_online_state);
gint sd_vcfg_reals_online_sum(CfgVirtual *virt);
