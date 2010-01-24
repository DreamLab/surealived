/*
 * Copyright 2009-2010 DreamLab Onet.pl Sp. z o.o.
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

#if !defined __CONFIG_H
#define __CONFIG_H

#include <stdio.h>
#include <glib.h>
#include <net/ip_vs.h> 
#include <libipvs.h>

#define MAXSIZE 64

/*       Config                   ConfVirtual              ConfReal
  c -> * meta              
	   * virtarr: 
       | virtarr[0]   | ------> * svc 
       | virtarr[1]   |         * vname
       |    ...       |         * realarr:
       | virtarr[n-1] |         | realarr[0]   | ------> * dest
                                | realarr[1]   |         * rname
                                | realarr[...] |
                                | realarr[n-1] |
*/

typedef struct {
	struct ip_vs_dest_user dest;
	gchar      rname[MAXSIZE];
	gboolean   is_in_ipvs;
} ConfReal;

typedef struct {
	struct ip_vs_service_user svc;
	gchar      vname[MAXSIZE];
	unsigned   conn_flags;
	GArray    *realarr;
	gboolean   is_in_ipvs;
} ConfVirtual;

typedef struct {
	GHashTable *meta;
	GArray     *virtarr;
	gchar       tmp[1024];
    gchar      *diff_filename;
    FILE       *diff_file;
    struct timeval last_diffs_cleanup;
} Config;

GHashTable *config_parse_line(gchar *line);
gint config_add_real(Config *c, ConfVirtual *currcv, GHashTable *ht);
gint config_add_virtual(Config *c, GHashTable *ht);
Config *config_full_read(gchar *fname);
void config_full_free(Config *conf);
gint config_synchronize(Config *conf, gboolean flush, gboolean del_unmanaged_virtuals);
gint config_fprintf(FILE *f, Config *conf);
gint config_diff_set(Config *c, GHashTable *ht);

ConfVirtual *config_find_virtual(Config *c, ipvs_service_t *svc, gint *cvindex);
ConfReal    *config_find_real(ConfVirtual *cv, ipvs_dest_t *dest, gint *crindex);

gboolean config_remove_real_by_index(ConfVirtual *cv, gint crindex);

#endif 
