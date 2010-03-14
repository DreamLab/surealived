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
#include <modloader.h>
#include <sd_socket.h>
#include <sd_epoll.h>
#include <sd_offline.h>
#include <sd_override.h>
#include <sd_ipvssync.h>
#include <sd_tester.h>
#include <xmlparser.h>
#include <sys/resource.h>

#define MAX_CLIENTS 16
#define COMMAND_END g_string_append_printf(s, "_SD_CMD_END_\n")

extern   gboolean G_daemonize; //surealived global flag (required to report parent and child)

int      listen_sock;
SDepoll *cmd_epoll = NULL;
int      connected_clients = 0;

typedef struct {
    int     fd;
    gchar   rbuf[BUFSIZ];
    gchar   *wbuf;
    gint    retcode;
    gint    rpos;
    gint    rend;
    gint    wpos;
    gint    wlen;
} SDClient;


static CfgVirtual *sd_cmd_find_virt(GPtrArray *VCfgArr, GHashTable *ht, gchar *errbuf) {
    CfgVirtual    *virt = NULL;
    gchar         *vaddr, *vport, *vproto, *vfwmark;
    gint           i;
    gboolean       found = FALSE;
    in_addr_t      addr;
    u_int16_t      port;
    SDIPVSProtocol proto;
    u_int32_t      fwmark;
    gchar         *defaddr = "0.0.0.0";
    gchar         *deffwmark = "0";
    
    vaddr = g_hash_table_lookup(ht, "vaddr");
    if (!vaddr)
        vaddr = defaddr;

    vport = g_hash_table_lookup(ht, "vport");
    if (!vport)
        vport = deffwmark;

    vproto = g_hash_table_lookup(ht, "vproto");
    if (!vproto) {
        if (errbuf)
            strcpy(errbuf, "cmd_find_virt: No vproto specified\n");
        return NULL;
    }

    vfwmark = g_hash_table_lookup(ht, "vfwmark");
    if (!vfwmark) {
        vfwmark = deffwmark;
    }

    addr   = inet_addr(vaddr);
    port   = htons(atoi(vport));
    proto  = sd_proto_no(vproto);
    fwmark = atoi(vfwmark);

    if (addr == -1) {
        LOGINFO("[^] logic: find_virt() invalid IP");
        return NULL;
    }

    for (i = 0; i < VCfgArr->len; i++) {
        virt = (CfgVirtual *) g_ptr_array_index(VCfgArr, i);
        LOGDETAIL("addr = %x==%x, %d==%d, %d==%d, %d==%d", virt->addr, addr, virt->port, port, 
                  virt->ipvs_proto, proto, virt->ipvs_fwmark, fwmark);
        if (fwmark > 0 && virt->ipvs_fwmark > 0 && !(virt->ipvs_fwmark - fwmark)) {
            found = TRUE;
            break;
        }
        
        if (!virt->ipvs_fwmark && 
            virt->addr == addr && virt->port == port &&
            virt->ipvs_proto == proto) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        if (errbuf)
            strcpy(errbuf, "cmd_find_virt: Virt not found\n");
        return NULL;
    }

    return virt;
}

CfgReal *sd_cmd_find_real(CfgVirtual *virt, GHashTable *ht, gchar *errbuf) {
    CfgReal     *real = NULL;
    gchar       *raddr, *rport;
    gint        i;
    gboolean    found = FALSE;
    in_addr_t   addr;
    u_int16_t   port;

    if (!virt || !virt->realArr) {
        strcpy(errbuf, "cmd_find_real: empty realArr\n");
        return NULL;
    }
    
    raddr = g_hash_table_lookup(ht, "raddr");
    if (!raddr) {
        if (errbuf)
            strcpy(errbuf, "cmd_find_real: raddr not found\n");
        return NULL;
    }

    rport = g_hash_table_lookup(ht, "rport");
    if (!rport) {
        if (errbuf)
            strcpy(errbuf, "cmd_find_real: rport not found\n");
        return NULL;
    }

    addr   = inet_addr(raddr);
    port   = htons(atoi(rport));

    if (addr == -1) {
        if (errbuf)
            strcpy(errbuf, "cmd_find_real: invalid IP\n");
        return NULL;
    }

    for (i = 0; i < virt->realArr->len; i++) {
        real = (CfgReal *)g_ptr_array_index(virt->realArr, i);
        if (real->addr != addr || real->port != port)
            continue;
        found = TRUE;
        break;
    }
    if (!found) {
        if (errbuf)
            strcpy(errbuf, "cmd_find_real: Real not found\n");
        return NULL;
    }

    return real;
}

/* Returns allocated string */
static gchar *sd_cmd_vlist(GPtrArray *VCfgArr) {
    GString    *s = g_string_new_len(NULL, BUFSIZ);
    CfgVirtual *virt;
    gint        i;

    for (i = 0; i < VCfgArr->len; i++) {
        virt = (CfgVirtual *) g_ptr_array_index(VCfgArr, i);
        g_string_append_printf(s, "v:%d vname=%s vproto=%s vaddr=%s vport=%d vfwmark=%d vrt=%s vsched=%s\n",
                               i,
                               virt->name, 
                               sd_proto_str(virt->ipvs_proto),
                               virt->addrtxt, ntohs(virt->port), 
                               virt->ipvs_fwmark,
                               sd_rt_str(virt->ipvs_rt),
                               virt->ipvs_sched);
    }

    COMMAND_END;

    return g_string_free(s, FALSE);
}

/* Returns allocated string */
static gchar *sd_cmd_rlist(GPtrArray *VCfgArr, GHashTable *ht) {
    GString    *s = NULL;
    CfgVirtual *virt;
    CfgReal    *real;
    gint        i;
    gchar       errbuf[BUFSIZ];
    gint        currwgt;
        
    virt = sd_cmd_find_virt(VCfgArr, ht, errbuf);
    if (!virt)
        return g_strdup(errbuf);

    s = g_string_new_len(NULL, BUFSIZ);

    g_string_append_printf(s, "vname=%s vproto=%s vaddr=%s vport=%d vfwmark=%d vrt=%s vsched=%s\n",
                           virt->name, sd_proto_str(virt->ipvs_proto), virt->addrtxt, ntohs(virt->port), 
                           virt->ipvs_fwmark, sd_rt_str(virt->ipvs_rt), virt->ipvs_sched);
    
    if (virt->realArr) {
        for (i = 0; i < virt->realArr->len; i++) {
            real = (CfgReal *) g_ptr_array_index(virt->realArr, i);
            currwgt = sd_ipvssync_calculate_real_weight(real, TRUE);
            g_string_append_printf(s, "r:%d rname=%s raddr=%s rport=%d currwgt=%d confwgt=%d ronline=%s rstate=%s\n",
                                   i,
                                   real->name, 
                                   real->addrtxt, ntohs(real->port),
                                   currwgt, real->ipvs_weight, GBOOLSTR(real->online), sd_rstate_str(real->rstate));
        }
    } 
    else 
        g_string_append_printf(s, "EMPTY LIST - no reals defined in virtual\n");

    COMMAND_END;

    return g_string_free(s, FALSE);
}

/* Returns allocated string */
static gchar *sd_cmd_stats(GPtrArray *VCfgArr) {
    GString    *s = NULL;
    CfgVirtual *virt;
    CfgReal    *real;
    gint        i, j;
    gboolean    do_scan; 
    struct timeval ctime;
    static struct timeval lsttime = { 0, 0 };
    static struct rusage usage;
    static gint total_v = 0;
    static gint total_r = 0;
    static gint total_ronline = 0;
    static gint total_roffline = 0; 
    static gint total_rdown = 0;
    static gint total_success = 0;
    static gint total_failed = 0;
    FILE   *in = NULL;
    gchar  *fname = NULL;
    gchar   buf[BUFSIZ];
    GDir   *dir = NULL;
    gint    fdno = 0;
    gchar   vmname[32], vmunit[8];
    gulong  vmvalue;
        
    gettimeofday(&ctime, NULL);
    if (ctime.tv_sec >= lsttime.tv_sec + 2)
        do_scan = TRUE;

    s = g_string_new_len(NULL, BUFSIZ);
    if (do_scan) {
        getrusage(RUSAGE_SELF, &usage);
        total_v = VCfgArr->len;
        total_r = total_ronline = total_roffline = total_rdown = total_success = total_failed = 0;

        for (i = 0; i < VCfgArr->len; i++) {
            virt = (CfgVirtual *) g_ptr_array_index(VCfgArr, i);
            
            if (!virt->realArr) //no reals in virtual
                continue;

            for (j = 0; j < virt->realArr->len; j++) {
                real = (CfgReal *) g_ptr_array_index(virt->realArr, j);
                total_r++;

                if (real->rstate == REAL_ONLINE) 
                    total_ronline++;
                else if (real->rstate == REAL_OFFLINE)
                    total_roffline++;
                else 
                    total_rdown++;
                
                if (real->online)
                    total_success++;
                else 
                    total_failed++;
            }
        }
    }

    g_string_append_printf(s, 
                           "=== global stats ===\n"
                           "watchdog_pid   : %d\n"
                           "tester_pid     : %d\n"
                           "virtuals       : %d\n"
                           "reals          : %d\n\n"
                           "=== real settings ===\n"
                           "ronline        : %d\n"
                           "roffline       : %d\n"
                           "rdown          : %d\n"
                           "offline_size   : %u\n"
                           "override_size  : %u\n\n"
                           "=== real stats ===\n"
                           "success        : %d\n"
                           "failed         : %d\n\n",
                           G_daemonize ? getppid() : -1,
                           getpid(),
                           total_v, total_r,
                           total_ronline, total_roffline, total_rdown,
                           sd_offline_hash_table_size(),
                           sd_override_hash_table_size(),
                           total_success, total_failed);

    g_string_append_printf(s, 
                           "=== tests stats ===\n"
                           "t_success      : %u\n"
                           "t_failed       : %u\n"
                           "t_online_set   : %u\n"
                           "t_offline_set  : %u\n"
                           "t_conn_problem : %u\n"
                           "t_arp_problem  : %u\n"
                           "t_rst_problem  : %u\n"
                           "t_bytes_rcvd   : %u\n"
                           "t_bytes_sent   : %u\n\n",
                           G_stats_test_success,
                           G_stats_test_failed,
                           G_stats_online_set,
                           G_stats_offline_set,
                           G_stats_conn_problem,
                           G_stats_arp_problem,
                           G_stats_rst_problem,
                           G_stats_bytes_rcvd,
                           G_stats_bytes_sent);

    g_string_append_printf(s, 
                           "=== resource usage ===\n"
                           "user_time      : %d.%06d sec\n"
                           "system_time    : %d.%06d sec\n"
                           "block_in_ops   : %ld\n"
                           "block_out_ops  : %ld\n"
                           "messages_sent  : %ld\n"
                           "messages_rcvd  : %ld\n"
                           "signals_rcvd   : %ld\n"
                           "voluntary_ctx  : %ld\n"
                           "involunt_ctx   : %ld\n",
                           (int) usage.ru_utime.tv_sec, (int) usage.ru_utime.tv_usec,
                           (int) usage.ru_stime.tv_sec, (int) usage.ru_stime.tv_usec,
                           usage.ru_inblock, usage.ru_oublock,
                           usage.ru_msgsnd, usage.ru_msgrcv,
                           usage.ru_nsignals,
                           usage.ru_nvcsw, usage.ru_nivcsw);

    /* Count fd usage */
    fname = g_strdup_printf("/proc/%d/fd", getpid());
    dir = g_dir_open(fname, 0, NULL);
    assert(dir != NULL);
    free(fname);
    fdno = 0;
    while (g_dir_read_name(dir))
        fdno++;
    g_dir_close(dir);

    g_string_append_printf(s, "fd_used        : %d\n\n", fdno);

    /* Print Vm* from /proc/PID/status */
    fname = g_strdup_printf("/proc/%d/status", getpid());
    in = fopen(fname, "r");
    assert(in != NULL);
    free(fname);
    g_string_append_printf(s, "=== memory usage ===\n");
    while (fgets(buf, BUFSIZ, in))
        if (sscanf(buf, "Vm%31s %ld %7s\n", vmname, &vmvalue, vmunit) == 3) {
            vmname[strlen(vmname)-1] = '\0';
            g_string_append_printf(s, "Vm%-12s : %-6ld %s\n", vmname, vmvalue, vmunit);
        }
    fclose(in);

    COMMAND_END;

    return g_string_free(s, FALSE);
}

/* Returns allocated string */
static gchar *sd_cmd_rset(GPtrArray *VCfgArr, GHashTable *ht) {
    GString    *s = NULL;
    CfgVirtual *virt;
    CfgReal    *real;
    gchar       errbuf[BUFSIZ];
    gchar      *rweight, *rstate, *c;
    gint        rst;
        
    virt = sd_cmd_find_virt(VCfgArr, ht, errbuf);
    if (!virt)
        return g_strdup(errbuf);

    real = sd_cmd_find_real(virt, ht, errbuf);
    if (!real)
        return g_strdup(errbuf);

    rstate = g_hash_table_lookup(ht, "rstate");
    rweight = g_hash_table_lookup(ht, "rweight");
    if (!rstate && !rweight) 
        return g_strdup_printf("cmd_rset: you need to set at least 'rstate' or 'rweight'\n");

    if (!rstate) {
        rstate = sd_rstate_str(real->rstate); //leave rstate untouched
        rst = sd_rstate_no(rstate);
    }
    else {
        rst = sd_rstate_no(rstate);
        if (rst < 0)
            return g_strdup_printf("cmd_rset: rstate unknown [%s]\n", rstate);
    }

    if (rweight) {
        c = strchr(rweight, '%');
        if (c) {
            c[0] = 0;
            real->override_weight_in_percent = TRUE;
        }
        else 
            real->override_weight_in_percent = FALSE;
        real->override_weight = atoi(rweight);
    }

    /* set default weight */
    if (real->override_weight == -1)
        real->override_weight = real->ipvs_weight;
    
    if (rst == REAL_ONLINE &&
        ((real->ipvs_weight == real->override_weight && !real->override_weight_in_percent) ||
         (real->override_weight == 100 && real->override_weight_in_percent))) {
        LOGINFO("Deleting entry from overrides: rstate=ONLINE, weight=ORIGINAL");
        real->last_rstate = real->rstate;
        real->rstate = rst;
        sd_override_dump_del(real);
        sd_ipvssync_diffcfg_real(real, TRUE);
    }
    else {
        LOGINFO("Replacing entry in overrides: rstate=%s, weight=%d, inpercent=%s",
                sd_rstate_str(rst), real->override_weight, 
                GBOOLSTR(real->override_weight_in_percent));
        real->last_rstate = real->rstate;
        real->rstate = rst;
        sd_override_dump_add(real);
        sd_ipvssync_diffcfg_real(real, TRUE);
    }
          
    s = g_string_new_len(NULL, BUFSIZ);

    g_string_append_printf(s, "Set: rstate=%s, weight=%d, inpercent=%s\n",
                           sd_rstate_str(rst), real->override_weight, 
                           GBOOLSTR(real->override_weight_in_percent));

    COMMAND_END;

    return g_string_free(s, FALSE);
}


/* ---------------------------------------------------------------------- */
inline static void _connstats_string_append_virtual(GString *s, CfgVirtual *virt) {
    g_string_append_printf(s, "vname=%s vproto=%s vaddr=%s vport=%d vfwmark=%d ",
                           virt->name, sd_proto_str(virt->ipvs_proto), virt->addrtxt, ntohs(virt->port), virt->ipvs_fwmark);
}

inline static void _connstats_string_append_real(GString *s, CfgReal *real) {
    g_string_append_printf(s, "rname=%s raddr=%s rport=%d total=%d ", real->name, real->addrtxt, ntohs(real->port), real->stats.total);
}


/* Returns allocated string */
static gchar *sd_cmd_connstats(GPtrArray *VCfgArr) {
    GString      *s = g_string_new_len(NULL, BUFSIZ);
    CfgVirtual   *virt;
    VirtualStats *vs;
    CfgReal      *real;
    gint          i, j;

    if (!G_gather_stats) {
        g_string_append_printf(s, "Gathering statistics is OFF\n");
        return g_string_free(s, FALSE);
    }

    /* description:
       s - samples
       t - total 

       virtual:
       savgconn  - samples avg conntime
       savgresp  - samples avg resptime
       savgtotal - samples avg totaltime

       real:
       savgconn  - samples avg conntime
       savgresp  - samples avg resptime
       savgtotal - samples avg totaltime
       tconn     - total conntime (sum - 64bit integer)
       tresp     - total resptime 
       ttotal    - total totaltime
       tavgconn  - total avg conntime (total conntime / total samples)
       tavgresp  - total avg resptime (total resptime / total samples)
       tavgtotal - total avg totaltime (total totaltime / total samples)
    */
       

    for (i = 0; i < VCfgArr->len; i++) {
        virt = (CfgVirtual *) g_ptr_array_index(VCfgArr, i);
        vs = &virt->stats;
        g_string_append_printf(s, "Virtual:\n");
        g_string_append_printf(s, "v:%d vname=%s vproto=%s vaddr=%s vport=%d vfwmark=%d vrt=%s vsched=%s ",
                               i,
                               virt->name, 
                               sd_proto_str(virt->ipvs_proto),
                               virt->addrtxt, ntohs(virt->port), 
                               virt->ipvs_fwmark,
                               sd_rt_str(virt->ipvs_rt),
                               virt->ipvs_sched);
        if (virt->tester->logmicro)
            g_string_append_printf(s, "samples=%d total=%d savgconn=%dus savgresp=%dus savgtotal=%dus ",
                                   virt->tester->stats_samples,
                                   vs->total,
                                   vs->avg_conntime_us,
                                   vs->avg_resptime_us, 
                                   vs->avg_totaltime_us);
        else
            g_string_append_printf(s, "samples=%d total=%d savgconn=%dms savgresp=%dms savgtotal=%dms ",
                                   virt->tester->stats_samples,
                                   vs->total,
                                   vs->avg_conntime_ms,
                                   vs->avg_resptime_ms, 
                                   vs->avg_totaltime_ms);

        g_string_append_printf(s, "connprob=%d arpprob=%d rstprob=%d\n\n",
                               vs->conn_problem, vs->arp_problem, vs->rst_problem);


        g_string_append_printf(s, "Reals:\n");
        if (virt->realArr) {
            for (j = 0; j < virt->realArr->len; j++) {
                real = (CfgReal *) g_ptr_array_index(virt->realArr, j);
//                currwgt = sd_ipvssync_calculate_real_weight(real);

                /* Statistics for connection problems */
                g_string_append_printf(s, "r:%d.%d real-stats ", i, j);
                _connstats_string_append_virtual(s, virt);
                _connstats_string_append_real(s, real);

                g_string_append_printf(s, "connprob=%d arpprob=%d rstprob=%d\n",
                                       real->stats.conn_problem, real->stats.arp_problem, real->stats.rst_problem);

                /* Statistics for samples */
                g_string_append_printf(s, "r:%d.%d lstats_ms  ", i, j);
                _connstats_string_append_virtual(s, virt);
                _connstats_string_append_real(s, real);

                g_string_append_printf(s, "lastconn=%dms lastresp=%dms lasttotal=%dms\n",
                                       real->stats.last_conntime_ms, real->stats.last_resptime_ms, real->stats.last_totaltime_ms);

                g_string_append_printf(s, "r:%d.%d lstats_us  ", i, j);
                _connstats_string_append_virtual(s, virt);
                _connstats_string_append_real(s, real);

                g_string_append_printf(s, "lastconn=%dus lastresp=%dus lasttotal=%dus\n",
                                       real->stats.last_conntime_us, real->stats.last_resptime_us, real->stats.last_totaltime_us);

                g_string_append_printf(s, "r:%d.%d sstats_ms  ", i, j);
                _connstats_string_append_virtual(s, virt);
                _connstats_string_append_real(s, real);

                g_string_append_printf(s, "savgconn=%dms savgresp=%dms savgtotal=%dms\n",
                                       real->stats.avg_conntime_ms, real->stats.avg_resptime_ms, real->stats.avg_totaltime_ms);

                g_string_append_printf(s, "r:%d.%d sstats_us  ", i, j);
                _connstats_string_append_virtual(s, virt);
                _connstats_string_append_real(s, real);

                g_string_append_printf(s, "savgconn=%dus savgresp=%dus savgtotal=%dus\n",
                                       real->stats.avg_conntime_us, real->stats.avg_resptime_us, real->stats.avg_totaltime_us);

                /* Statistics for total ms/us */
                g_string_append_printf(s, "r:%d.%d tstats_ms  ", i, j);
                _connstats_string_append_virtual(s, virt);
                _connstats_string_append_real(s, real);

                g_string_append_printf(s, "tavgconn=%dms tavgresp=%dms tavgtotal=%dms\n",
                                       real->stats.total_avg_conntime_ms, 
                                       real->stats.total_avg_resptime_ms, 
                                       real->stats.total_avg_totaltime_ms);

                g_string_append_printf(s, "r:%d.%d tstats_us  ", i, j);
                _connstats_string_append_virtual(s, virt);
                _connstats_string_append_real(s, real);

                g_string_append_printf(s, "tavgconn=%dus tavgresp=%dus tavgtotal=%dus\n",
                                       real->stats.total_avg_conntime_us, 
                                       real->stats.total_avg_resptime_us, 
                                       real->stats.total_avg_totaltime_us);

                g_string_append_printf(s, "\n");
            }
            g_string_append_printf(s, "-----\n");
        } 
        g_string_append_printf(s, "\n");
    }

    COMMAND_END;

    return g_string_free(s, FALSE);
}

/* ---------------------------------------------------------------------- */
gint sd_cmd_listen_socket_create(gchar *addr, u_int16_t lport) {
    struct sockaddr_in  sa;
    SDClient            *server = NULL;
    gchar               *localhost = "127.0.0.1";
    int                 reuseaddr = 1;

    LOGDETAIL("%s()", __PRETTY_FUNCTION__);

    listen_sock = sd_socket_nb(SOCK_STREAM);

    if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, 
                   (const void *) &reuseaddr, sizeof(int)) == -1) {
        LOGERROR("Unable to reuse listen_sock [%s]", STRERROR);
    }

    if (!strlen(addr))
        addr = localhost;

    sa.sin_family = PF_INET;
    sa.sin_port = htons(lport);
    sa.sin_addr.s_addr = inet_addr(addr);

    LOGINFO("Binding cmd interface to [%s:%d]", addr, lport);
    if (bind(listen_sock, (struct sockaddr *)&sa, sizeof(sa))) {
        LOGERROR("Unable to bind to socket: [%s:%d] [syserror = %s]", addr, lport, STRERROR);
        exit(1);
    }

    if (listen(listen_sock, MAX_CLIENTS)) {
        LOGERROR("Unable to listen()");
        return -1;
    }
    server = (SDClient *) malloc(sizeof(SDClient));
    server->fd = listen_sock;

    if (!cmd_epoll)
        cmd_epoll = sd_epoll_new(MAX_CLIENTS+1);

    sd_epoll_ctl(cmd_epoll, EPOLL_CTL_ADD, listen_sock, server, EPOLLIN);

    return listen_sock;
}

static gint sd_cmd_accept_client() {
    int                 client_sock;
    struct sockaddr_in  sa;
    socklen_t           len = sizeof(struct sockaddr_in);
    SDClient            *client;

    if (connected_clients == MAX_CLIENTS) /* limit reached */
        return 1;

    client_sock = accept(listen_sock, (struct sockaddr *)&sa, &len);
    if (client_sock == -1 && errno != EAGAIN) {
        LOGDEBUG("[@] Client disconnected!");
        return -1;
    }
    LOGDEBUG("[@] New client connected!\n");

    client = (SDClient *)malloc(sizeof(SDClient));
    memset(client, 0, sizeof(SDClient));
    client->fd = client_sock;
    sd_epoll_ctl(cmd_epoll, EPOLL_CTL_ADD, client_sock, client, EPOLLIN); /* waiting for read */

    connected_clients++;        /* increase counter */

    return 0;
}

static void sd_cmd_execute(SDClient *client, GPtrArray *VCfgArr) {
    GHashTable *ht;
    gchar      *cmd;

    if (!client)
        return;

    cmd = client->rbuf;
    ht = sd_parse_line(cmd);

    if (client->wbuf) {
        free(client->wbuf);
        client->wbuf = NULL;
    }

    if (!strncmp(cmd, "vlist", 5)) {
        client->wbuf = sd_cmd_vlist(VCfgArr);
        goto cleanup;
    }
    else if (!strncmp(cmd, "rlist", 5)) {
        client->wbuf = sd_cmd_rlist(VCfgArr, ht);
        goto cleanup;
    }
    else if (!strncmp(cmd, "stats", 5)) {
        client->wbuf = sd_cmd_stats(VCfgArr);
        goto cleanup;
    }
    else if (!strncmp(cmd, "rset", 4)) {
        client->wbuf = sd_cmd_rset(VCfgArr, ht);        
    }
    else if (!strncmp(cmd, "connstats", 9)) {
        client->wbuf = sd_cmd_connstats(VCfgArr);
        goto cleanup;
    }
    else
        client->wbuf = g_strdup_printf("Invalid request! [%s]\n", cmd);

cleanup:
    client->wlen = strlen(client->wbuf);
    client->wpos = 0;
    g_hash_table_destroy(ht);
}

static void _sd_cmd_client_free(SDClient *client) {
    LOGDEBUG("Client free [%x], fd = %d", client, client->fd);
    sd_epoll_ctl(cmd_epoll, EPOLL_CTL_DEL, client->fd, client, EPOLLIN);

    close(client->fd);
    if (client->wbuf)
        free(client->wbuf);
    free(client);
    connected_clients--;
}

static gboolean sd_cmd_process_client_read(SDClient *client, GPtrArray *VCfgArr) {
    gint    bytes;
    gchar   *npos;

    LOGDEBUG("Processing client READ!");
    bytes = read(client->fd, client->rbuf + client->rpos, sizeof(client->rbuf) - client->rpos-1);
    
    /* EOF */
    if (!bytes) {
        _sd_cmd_client_free(client);
        return TRUE;
    }
    /* error */
    else if (bytes < 0) {
        LOGDEBUG(" * error READ!");
        return FALSE;
    }
    
    npos = strchr(client->rbuf + client->rpos, '\n');
    client->rpos += bytes;

    if (!npos)
        return FALSE;

    *npos = '\0';
    if (*(npos-1) == '\r')
        *(npos-1) = '\0';
    client->rpos = 0;

    LOGDEBUG("rbuf = [%s], bufaddr = %x, npos = %x", client->rbuf, client->rbuf, npos);

    sd_cmd_execute(client, VCfgArr);

    sd_epoll_ctl(cmd_epoll, EPOLL_CTL_MOD, client->fd, client, EPOLLOUT);

    return FALSE;
}

void static sd_cmd_process_client_write(SDClient *client) {
    gint    bytes;

    LOGDEBUG("Processing client WRITE [%s]!", client->wbuf + client->wpos);
    bytes = write(client->fd, client->wbuf + client->wpos, client->wlen - client->wpos);
    
    /* EOF */
    if (bytes <= 0)
        return;

    client->wpos += bytes;
    LOGDEBUG(" * written %d bytes [%d / %d]", bytes, client->wpos, client->wlen);

    if (client->wpos == client->wlen) //message sent
        sd_epoll_ctl(cmd_epoll, EPOLL_CTL_MOD, client->fd, client, EPOLLIN);
}

void sd_cmd_loop(GPtrArray *VCfgArr) {
    gint        nr_events = 0, i;
    SDClient    *client;
    gchar        s[64];

    nr_events = sd_epoll_wait(cmd_epoll, 0);

    for (i = 0; i < nr_events; i++) {
        client = (SDClient *)sd_epoll_event_dataptr(cmd_epoll, i);
        uint32_t ev = sd_epoll_event_events(cmd_epoll, i);
        sd_epoll_event_str(ev, s);

        LOGDEBUG("events: nr_events = %d, i = %d, fd = %d, ev = [%s]",
                                nr_events, i, client->fd, s);

        /* manage errors */
        if (ev & EPOLLERR) {
            _sd_cmd_client_free(client);
            continue;
        }

        /* new client */
        if (client->fd == listen_sock) {
            LOGDEBUG("New client connected");
            sd_cmd_accept_client();
            continue;
        }

        /* process client - read command from client */
        if (ev & EPOLLIN) {
            gboolean client_closed;
            LOGDEBUG("READ EVENT [%d] on client [%d]", ev, i);
            client_closed = sd_cmd_process_client_read(client, VCfgArr);
            if (client_closed)
                return;
            continue;
        }

        /* process client - write command output to client */
        if (ev & EPOLLOUT) {
            LOGDEBUG("WRITE EVENT [%d] on client [%d]", ev, i);
            sd_cmd_process_client_write(client);
            continue;
        }            
    }
}
