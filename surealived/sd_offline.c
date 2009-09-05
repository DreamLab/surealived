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

#include <common.h>
#include <sd_defs.h>
#include <glib.h>
#include <sys/file.h>
#include <sd_maincfg.h>
#include <sd_log.h>

static GHashTable *OfflineH = NULL; /* this hash holds current offline reals */

void sd_offline_dump_add(CfgReal *real) {
    gchar *line;
    CfgVirtual *virt;
    if (!real || !G_use_offline_dump)
        return;

    if (!OfflineH)
        OfflineH = g_hash_table_new(g_direct_hash, g_direct_equal);
    assert(OfflineH);

    virt = real->virt;

    line = g_strdup_printf("%s:%d:%d - %s:%d\n",
        virt->addrtxt, ntohs(virt->port), virt->ipvs_proto,
        real->addrtxt, ntohs(real->port));

    g_hash_table_insert(OfflineH, real, line);

    LOGDEBUG("offline dump add (real = %x:%s:%s) (hash size = %d)", 
	     real, virt->name, real->name, g_hash_table_size(OfflineH));
}

void sd_offline_dump_del(CfgReal *real) {
    gchar *line = NULL;

    LOGDEBUG("offline dump del (real = %s:%s)", real->virt->name, real->name);

    if (OfflineH && G_use_offline_dump) {
        line = g_hash_table_lookup(OfflineH, real);
        if (line)
            free(line);

        g_hash_table_remove(OfflineH, real);
    }
}

void sd_offline_dump_write(gpointer key, gpointer val, gpointer userdata) {
    FILE    *dump = (FILE *) userdata;

    if (!dump)
        return;

    LOGDETAIL("offline dump write [%s]", val);
    fprintf(dump, (gchar *)val, strlen((gchar *)val));
}

gint sd_offline_dump_save() {
    static FILE *dump = NULL;

    LOGDEBUG("offline dump save [%x,%d]", OfflineH, G_use_offline_dump);

    if (!OfflineH || !G_use_offline_dump)
        return 1;

    dump = fopen(G_offline_dump, "w");

    if (!dump)
        LOGWARN("Unable to open dump file - %s", G_offline_dump);
    assert(dump);

    g_hash_table_foreach(OfflineH, sd_offline_dump_write, dump);

    fclose(dump);
    return 0;
}

void sd_offline_dump_merge(GPtrArray *VCfgArr) {
    /* this function merges offline.dump file into loaded configuration */
    /* it is not optimal but this function is called only once */
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
    gint        vi, ri;
    gboolean    found = FALSE;

    if (!G_use_offline_dump) {
        LOGWARN("Not using offline_dump!");
        return;
    }

    dump = fopen(G_offline_dump, "r");

    if (!OfflineH)
        OfflineH = g_hash_table_new(g_direct_hash, g_direct_equal);
    assert(OfflineH);

    if (!dump) {
        LOGWARN("No dump file!");
        return;
    }

    LOGINFO("Processing dump file [%s]!", G_offline_dump);

    while (fscanf(dump, "%32[^:]:%d:%d - %32[^:]:%d\n",
            vip, &vporth, &ipvs_proto, rip, &rporth) == 5) {
        vport = htons(vporth);
        rport = htons(rporth);
        vaddr = inet_addr(vip);
        raddr = inet_addr(rip);
        found = FALSE;
        for (vi = 0; vi < VCfgArr->len; vi++) {
            virt = g_ptr_array_index(VCfgArr, vi);
            if (virt->addr == vaddr && virt->port == vport && virt->ipvs_proto == ipvs_proto) {
                if (!virt->realArr) /* ignore empty virtuals */
                    continue;

                for (ri = 0; ri < virt->realArr->len; ri++) {
                    real = g_ptr_array_index(virt->realArr, ri);
                    if (real->addr == raddr && real->port == rport) {
                        /* this is where we actually set node values */
                        LOGINFO("\tSet real: %s:%s to OFFLINE", real->virt->name, real->name);
                        real->online = FALSE;
                        real->last_online = FALSE;
                        real->retries_fail = real->tester->retries2fail;
                        found = TRUE;
                        sd_offline_dump_add(real);
                    }
                }
            }
        }
        if (!found)
            LOGWARN("\tUnable to find real: %s:%d:%d - %s:%d",
                vip, vporth, ipvs_proto, rip, rporth);
    }
    fclose(dump);
}
