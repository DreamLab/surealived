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

#include <common.h>
#include <sd_defs.h>
#include <glib.h>
#include <sys/file.h>
#include <sd_maincfg.h>
#include <sd_log.h>
#include <sd_override.h>

static GHashTable *OverrideH = NULL; /* this hash holds current offline reals */

void sd_override_dump_add(CfgReal *real) {
    gchar *line;
    gchar *wgttxt;
    gint   wgt;
    CfgVirtual *virt;
    if (!real)
        return;

    if (!OverrideH)
        OverrideH = g_hash_table_new(g_direct_hash, g_direct_equal);
    assert(OverrideH);

    virt = real->virt;

    /* If someone wants to override only rstate at the beggining 
       this could save -1 to override.dump */
    if (real->override_weight == -1) {
        wgt = real->ipvs_weight;
        real->override_weight_in_percent = FALSE;
    }
    else
        wgt = real->override_weight;

    if (real->override_weight_in_percent)
        wgttxt = g_strdup_printf("weight=%u%%", wgt);
    else 
        wgttxt = g_strdup_printf("weight=%u ", wgt);

    line = g_strdup_printf("%s:%d:%d:%u - %s:%d - %s rstate=%s\n",
                           virt->addrtxt, ntohs(virt->port), 
                           virt->ipvs_proto,
                           virt->ipvs_fwmark,
                           real->addrtxt, ntohs(real->port),
                           wgttxt,
                           sd_rstate_str(real->rstate));
    free(wgttxt);

    g_hash_table_insert(OverrideH, real, line);

    LOGINFO("override dump add (real = %p:%s:%s) (hash size = %d)", 
	     real, virt->name, real->name, g_hash_table_size(OverrideH));

    sd_override_dump_save();
}

void sd_override_dump_del(CfgReal *real) {
    gchar *line = NULL;

    LOGINFO("override dump del (real = %s:%s)", real->virt->name, real->name);

    if (OverrideH) {
        line = g_hash_table_lookup(OverrideH, real);
        if (line)
            free(line);

        g_hash_table_remove(OverrideH, real);
    }

    sd_override_dump_save();
}

void sd_override_dump_write(gpointer key, gpointer val, gpointer userdata) {
    FILE    *dump = (FILE *) userdata;

    if (!dump)
        return;

    gchar *s = strchr(val, '\n');
    *s = '\0';
    LOGINFO("override dump write [%s]", val);
    *s = '\n';
    fprintf(dump, "%s", (gchar *)val);
}

gint sd_override_dump_save() {
    static FILE *dump = NULL;

    LOGDEBUG("override dump save [%p]", OverrideH);

    if (!OverrideH)
        return 1;

    dump = fopen(G_override_dump, "w");

    if (!dump)
        LOGWARN("Unable to open dump file - %s", G_override_dump);
    assert(dump);

    g_hash_table_foreach(OverrideH, sd_override_dump_write, dump);

    fclose(dump);
    return 0;
}

void sd_override_dump_merge(GPtrArray *VCfgArr) {
    FILE        *dump;
    CfgVirtual  *virt;
    CfgReal     *real;
    gchar       vip[32];
    gint        vporth;
    in_addr_t   vaddr;
    u_int16_t   vport;
    gchar       rip[32];
    gint        rporth;
    in_addr_t   raddr;
    u_int16_t   rport;
    gint        ipvs_proto;
    u_int32_t   ipvs_fwmark;
    gint        weight;
    gchar       wgttype;
    gchar       rstate[16];
    gint        vi, ri;
    gboolean    found = FALSE;

    dump = fopen(G_override_dump, "r");

    if (!OverrideH)
        OverrideH = g_hash_table_new(g_direct_hash, g_direct_equal);
    assert(OverrideH);

    if (!dump) {
        LOGWARN("No override file!");
        return;
    }

    LOGINFO("Processing override file [%s]", G_override_dump);

    while (fscanf(dump, "%32[^:]:%d:%d:%u - %32[^:]:%d - weight=%d%c rstate=%s\n",
                  vip, &vporth, &ipvs_proto, &ipvs_fwmark, rip, &rporth, &weight, &wgttype, rstate) == 9) {
        vport = htons(vporth);
        rport = htons(rporth);
        vaddr = inet_addr(vip);
        raddr = inet_addr(rip);
        found = FALSE;
        for (vi = 0; vi < VCfgArr->len; vi++) {
            virt = g_ptr_array_index(VCfgArr, vi);
            if ( (ipvs_fwmark > 0 && virt->ipvs_fwmark - ipvs_fwmark == 0) ||
                 (vaddr != 0 && virt->addr == vaddr && virt->port == vport && virt->ipvs_proto == ipvs_proto)) {
                if (!virt->realArr)
                    continue;

                for (ri = 0; ri < virt->realArr->len; ri++) {
                    real = g_ptr_array_index(virt->realArr, ri);
                    if (real->addr == raddr && real->port == rport) {
                        LOGDEBUG("\tOverride real: %s:%s (%s:%d:%d:%d - %s:%d) weight=%d%c, rstate=%s", 
                                real->virt->name, real->name, real->virt->addrtxt,
                                ntohs(real->virt->port), real->virt->ipvs_proto,
                                real->virt->ipvs_fwmark, real->addrtxt, ntohs(real->port), 
                                weight, wgttype, rstate);
                        found = TRUE;
                        real->override_weight = weight;
                        real->override_weight_in_percent = wgttype == '%' ? TRUE : FALSE;
                        real->rstate = sd_rstate_no(rstate);
                        sd_override_dump_add(real);
                    }
                }
            }
        }
        if (!found)
            LOGWARN("\tUnable to find real: %s:%d:%d - %s:%d",
                vip, vporth, ipvs_proto, rip, rporth);
    }
    fclose(dump);

    sd_override_dump_save(); 
}

guint sd_override_hash_table_size(void) {
    if (!OverrideH) 
        return -1;
    return g_hash_table_size(OverrideH);
}
