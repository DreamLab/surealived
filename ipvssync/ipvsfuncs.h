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

#if !defined __IPVSFUNCS_H
#define __IPVSFUNCS_H

#include <linux/types.h>
#include <glib.h>
#include <libipvs.h>
#include <net/ip_vs.h> 

int ipvsfuncs_modprobe_ipvs(void);
void ipvsfuncs_initialize(void);
void ipvsfuncs_reinitialize(void);

ipvs_service_t *ipvsfuncs_set_svc(u_int16_t protocol, char *taddr, char *tport,
                                  u_int32_t fwmark, char *sched_name, unsigned  flags,
                                  unsigned  timeout, __be32 netmask, ipvs_service_t *svc);

ipvs_dest_t *ipvsfuncs_set_dest(char *taddr, char *tport, 
                                unsigned conn_flags, int weight, 
                                u_int32_t u_threshold, u_int32_t l_threshold, 
                                ipvs_dest_t *dest);

int ipvsfuncs_add_service(ipvs_service_t *svc);
int ipvsfuncs_del_service(ipvs_service_t *svc);
int ipvsfuncs_update_service(ipvs_service_t *svc);

int ipvsfuncs_add_dest(ipvs_service_t *svc, ipvs_dest_t *dest);
int ipvsfuncs_del_dest(ipvs_service_t *svc, ipvs_dest_t *dest);
int ipvsfuncs_update_dest(ipvs_service_t *svc, ipvs_dest_t *dest);

int ipvsfuncs_fprintf_services(FILE *f);
int ipvsfuncs_del_unmanaged_services(ipvs_service_t **managed_svc, gint *is_in_ipvs);
int ipvsfuncs_del_unmanaged_dests(ipvs_service_t *svc,
                                  ipvs_dest_t **managed_dest, gint *is_in_ipvs);

int ipvsfuncs_fprintf_ipvs(FILE *f);

gboolean ipvsfuncs_set_svc_from_ht(ipvs_service_t *svc, GHashTable *ht);
gboolean ipvsfuncs_set_dest_from_ht(ipvs_dest_t *dest, GHashTable *ht, unsigned vconn_flags);
#endif
