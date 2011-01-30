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

void sd_notify_dump_save(GPtrArray *VCfgArr) {
    FILE         *dump;
    CfgVirtual   *virt;
    VNotifier    *vn;
    gint          vi;

    dump = fopen(G_notify_dump, "w");

    if (!dump)
        LOGWARN("Unable to open notify dump file - %s", G_notify_dump);
    assert(dump);

    for (vi = 0; vi < VCfgArr->len; vi++) {
        virt = g_ptr_array_index(VCfgArr, vi);
        vn = &virt->tester->vnotifier;

        if (!vn->is_defined)
            continue;

        if (!virt->realArr) /* ignore empty virtuals */
            continue;

        fprintf(dump, "vnotify %s:%d:%d:%u - state=%s",
                virt->addrtxt, ntohs(virt->port), 
                virt->ipvs_proto, virt->ipvs_fwmark,
                sd_nstate_str(vn->nstate));

        fprintf(dump, "\n");
    }

    fclose(dump);
}

void sd_notify_dump_merge(GPtrArray *VCfgArr, GHashTable *VCfgHash) {
    FILE          *f;
    gfloat         uptime;
    struct stat    st;
    struct timeval currtime;
    FILE          *dump;
    CfgVirtual    *virt;
    VNotifier     *vn;
    gchar          buf[BUFSIZ];
    gchar          vkey[64], nstatestr[16];

    /* Don't merge notify.dump if system is already booted. In such a case
       executing notifiers is required. */
    f = fopen("/proc/uptime", "r");
    assert(f);
    if (fscanf(f, "%f", &uptime) != 1) {
        LOGERROR("Can't parse /proc/uptime file. Exiting.");
        abort();
    }
    fclose(f);

    gettimeofday(&currtime, NULL);
    if (stat(G_notify_dump, &st)) {
        LOGWARN("Can't stat %s [%s]", G_notify_dump, STRERROR);
        return;
    }
    LOGDEBUG("notify.dump file mtime = %d", (int) st.st_mtime);
    LOGDEBUG("current time = %d\n", (int) currtime.tv_sec);

    /* Calculate should we run all notifiers */
    /* Example: mtime(notify_dump) = 1296422425
                current_time       = 1296423707
                uptime             = 17702
       mtime(notify_dump) - (current_time - uptime) 
       1296422425         - (1296423707 - 17702) 
       ---
       16420 ( > 0  - don't unlink notify.dump - and process it)
             ( <= 0 - unlink the notify.dump and run all notifiers)
    */
    if ((st.st_mtime - (currtime.tv_sec - uptime)) <= 0) {
        LOGWARN("Probably system restart was performed (uptime = %.2f). Executing all notifiers", uptime);
        unlink(G_notify_dump);
        return;
    }
        
    dump = fopen(G_notify_dump, "r");
    if (!dump) {
        LOGWARN("Unable to open notify dump file - %s", G_notify_dump);
        return;
    }
    assert(dump);

    LOGINFO("Processing notifiers file [%s]", G_notify_dump);

    while (fgets(buf, BUFSIZ, dump)) {
        int len = strlen(buf);
        if (len)
            buf[len-1] = '\0';

        /* Restore virtual notify state */
        if (sscanf(buf, "vnotify %s - state=%s", vkey, nstatestr) == 2) {
            LOGDEBUG("vnotify found [vkey = %s]", vkey);

            virt = sd_vcfg_hashmap_lookup_virtual(VCfgHash, vkey);
            if (!virt)
                continue;

            vn = &virt->tester->vnotifier;
            if (!strncmp(nstatestr, "UNKNOWN", 7))
                vn->nstate = NOTIFY_UNKNOWN;
            else if (!strncmp(nstatestr, "UP", 2))
                vn->nstate = NOTIFY_UP;
            else if (!strncmp(nstatestr, "DOWN", 4))
                vn->nstate = NOTIFY_DOWN;
        }
        LOGINFO("buf = [%s]", buf);
    }

    fclose(dump);
}

/* ---------------------------------------------------------------------- */
static void _sd_execute_notify_script(CfgVirtual *virt, NotifyState nstate) {
    GString *s = NULL;
    GError *err = NULL;
    gchar *script = NULL;
    gchar **argv = NULL;
    gboolean isok;

    if (nstate == NOTIFY_UP) 
        script = virt->tester->vnotifier.notify_up;
    else if (nstate == NOTIFY_DOWN)
        script = virt->tester->vnotifier.notify_down;
    else 
        return;

    if (!script)
        return;
    
    LOGINFO(" * trying to execute notify script [nstate = %s, script = %s]",
            sd_nstate_str(nstate), script);

    s = g_string_new(NULL);
    assert(s);

    g_string_append_printf(s, "'%s' ", script);
    g_string_append_printf(s, "'notify=%s' 'vname=%s' 'vaddr=%s' 'vport=%d' 'vproto=%s' 'vfwmark=%d'", 
                           sd_nstate_str(nstate), 
                           virt->name, virt->addrtxt, ntohs(virt->port), 
                           sd_proto_str(virt->ipvs_proto), virt->ipvs_fwmark);
    LOGDEBUG("cmd = [%s]", s->str);
    isok = g_shell_parse_argv(s->str, NULL, &argv, &err);
    if (!isok) {
        LOGWARN("Can't parse notify cmd line [%s, err = %s]", s->str, err->message);
        g_string_free(s, TRUE);
        return;
    }

    isok = g_spawn_async(NULL, argv, NULL, 
                         G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, 
                         NULL, NULL, NULL, 
                         &err);
    if (!isok)
        LOGWARN("Can't exec notify line [%s, err = %s]", s->str, err->message);

    g_string_free(s, TRUE);
    g_strfreev(argv);
}

/* ---------------------------------------------------------------------- */
static void _sd_notify_info(CfgVirtual *virt, gchar *info,
                            gboolean is_enough_reals, gboolean is_enough_weight,
                            gint curr_reals, gint exp_reals,
                            gint curr_weight, gint exp_weight) {

    VNotifier *vn = &virt->tester->vnotifier;
    gchar c = ' ';

    LOGINFO("%s: [vname=%s vaddr=%s vport=%d vproto=%s vfwmark=%d]", 
            info, virt->name, virt->addrtxt, ntohs(virt->port), sd_proto_str(virt->ipvs_proto), virt->ipvs_fwmark);

    if (vn->min_reals_in_percent)
        c = '%';
    if (vn->min_reals) 
        LOGINFO(" * is enough reals:  %s, curr_reals = %d%c / exp_reals = %d%c",
                GBOOLSTR(is_enough_reals), curr_reals, c, exp_reals, c);

    if (vn->min_weight_in_percent)
        c = '%';
    if (vn->min_weight)
        LOGINFO(" * is enough weight: %s, curr_weight = %d%c / exp_weight = %d%c",
                GBOOLSTR(is_enough_weight), curr_weight, c, exp_weight, c);
}

#define SD_NOTIFY(_info) _sd_notify_info(virt, (_info), is_enough_reals, is_enough_weight, curr_reals, exp_reals, curr_weight, exp_weight)

void sd_notify_execute_if_required(GPtrArray *VCfgArr, CfgVirtual *virt) {
    gboolean       is_enough_reals = FALSE, is_enough_weight = FALSE;
    gint           curr_reals = 0, exp_reals = 0;
    gint           curr_weight = 0, exp_weight = 0;
    VNotifier     *vn;

    if (!virt || !virt->realArr)
        return;

    vn = &virt->tester->vnotifier;

    virt->reals_weight_sum  = sd_vcfg_reals_weight_sum(virt, FALSE); 
    virt->online_weight_sum = sd_vcfg_reals_weight_sum(virt, TRUE);
    virt->reals_online_sum  = sd_vcfg_reals_online_sum(virt);

    LOGDEBUG("notify_execute()");

    /* If min_reals == 0 there's enough reals, otherwise calculate it */
    if (vn->min_reals == 0)
        is_enough_reals = TRUE;
    else {
        if (vn->min_reals_in_percent) {
            curr_reals = virt->reals_online_sum * 100 / virt->realArr->len;
            exp_reals = vn->min_reals;
            LOGDEBUG(" * MIN_REALS (PERC): curr_reals = %d%%, exp_reals = %d%%", curr_reals, exp_reals);
        }
        else {
            curr_reals = virt->reals_online_sum;
            exp_reals = vn->min_reals;
            LOGDEBUG(" * MIN_REALS (NUMB): curr_reals = %d, exp_reals = %d", curr_reals, exp_reals);
        }

        if (curr_reals < exp_reals) {
            LOGDEBUG(" * MIN_REALS (NOTIFY_DOWN): curr_reals = %d < exp_reals = %d", curr_reals, exp_reals);
            is_enough_reals = FALSE;
        }
        else 
            is_enough_reals = TRUE;
    }
    LOGDEBUG(" * IS_ENOUGH_REALS = %s, NSTATE = %s", GBOOLSTR(is_enough_reals), sd_nstate_str(vn->nstate));

    /* if min_weight == 0 there's enough weight, otherwise calculate it */
    if (vn->min_weight == 0)
        is_enough_weight = TRUE;
    else {
        if (vn->min_weight_in_percent) {
            curr_weight = virt->online_weight_sum * 100 / virt->reals_weight_sum;
            exp_weight = vn->min_weight;
            LOGDEBUG(" * MIN_WEIGHT (PERC): curr_weight = %d%%, exp_weight = %d%%", curr_weight, exp_weight);
        }
        else {
            curr_weight = virt->online_weight_sum;
            exp_reals = vn->min_weight;
            LOGDEBUG(" * MIN_WEIGHT (NUMB): curr_weight = %d, exp_weight = %d", curr_weight, exp_weight);
        }

        if (curr_weight < exp_weight) {
            LOGDEBUG(" * MIN_WEIGHT (NOTIFY_DOWN): curr_weight = %d < exp_weight = %d", curr_weight, exp_weight);
            is_enough_weight = FALSE;
        }
        else 
            is_enough_weight = TRUE;
    }
    LOGDEBUG(" * IS_ENOUGH_WEIGHT = %s, NSTATE = %s", GBOOLSTR(is_enough_weight), sd_nstate_str(vn->nstate));

    /* Call notify script */
    if (vn->nstate == NOTIFY_UNKNOWN) {
        if (is_enough_reals && is_enough_weight) {
            vn->nstate = NOTIFY_UP;
            SD_NOTIFY("NOTIFY UP");
            _sd_execute_notify_script(virt, vn->nstate);
            sd_notify_dump_save(VCfgArr);
        }
        else {
            vn->nstate = NOTIFY_DOWN;
            SD_NOTIFY("NOTIFY DOWN");
            _sd_execute_notify_script(virt, vn->nstate);
            sd_notify_dump_save(VCfgArr);
        }
    }
    else if (vn->nstate == NOTIFY_DOWN && is_enough_reals && is_enough_weight) {
            vn->nstate = NOTIFY_UP;
            SD_NOTIFY("NOTIFY UP");
            _sd_execute_notify_script(virt, vn->nstate);
            sd_notify_dump_save(VCfgArr);
    }
    else if (vn->nstate == NOTIFY_UP && (!is_enough_reals || !is_enough_weight)) {
            vn->nstate = NOTIFY_DOWN;
            SD_NOTIFY("NOTIFY DOWN");
            _sd_execute_notify_script(virt, vn->nstate);
            sd_notify_dump_save(VCfgArr);
    }

}

/* ---------------------------------------------------------------------- */
static void _sd_execute_notify_real_script(CfgReal *real, NotifyState nstate) {
    CfgVirtual *virt = real->virt;
    GString *s = NULL;
    GError *err = NULL;
    gchar *script = NULL;
    gchar **argv = NULL;
    gboolean isok;

    if (nstate == NOTIFY_UP) 
        script = virt->tester->rnotifier.notify_up;
    else if (nstate == NOTIFY_DOWN)
        script = virt->tester->rnotifier.notify_down;
    else 
        return;

    if (!script)
        return;
    
    LOGINFO(" * trying to execute real notify script [nstate = %s, script = %s]",
            sd_nstate_str(nstate), script);

    s = g_string_new(NULL);
    assert(s);

    g_string_append_printf(s, "'%s' ", script);
    g_string_append_printf(s, "'notify=%s' 'vname=%s' 'vaddr=%s' 'vport=%d' 'vproto=%s' 'vfwmark=%d' "
                           "'rname=%s' 'raddr=%s' 'rport=%d'", 
                           sd_nstate_str(nstate), 
                           virt->name, virt->addrtxt, ntohs(virt->port), 
                           sd_proto_str(virt->ipvs_proto), virt->ipvs_fwmark,
                           real->name, real->addrtxt, ntohs(real->port));
    LOGDEBUG("cmd = [%s]", s->str);
    isok = g_shell_parse_argv(s->str, NULL, &argv, &err);
    if (!isok) {
        LOGWARN("Can't parse notify real cmd line [%s, err = %s]", s->str, err->message);
        g_string_free(s, TRUE);
        return;
    }

    isok = g_spawn_async(NULL, argv, NULL, 
                         G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, 
                         NULL, NULL, NULL, 
                         &err);
    if (!isok)
        LOGWARN("Can't exec notify line [%s, err = %s]", s->str, err->message);

    g_string_free(s, TRUE);
    g_strfreev(argv);
}

void sd_notify_real(CfgReal *real, RState state) {
    RNotifier *rn = &real->tester->rnotifier;
    if (!rn->is_defined)
        return;

    if (rn->is_defined && state == REAL_ONLINE && rn->notify_up)
        _sd_execute_notify_real_script(real, NOTIFY_UP);
    else if (rn->is_defined && state == REAL_OFFLINE && rn->notify_down)
        _sd_execute_notify_real_script(real, NOTIFY_DOWN);
}
