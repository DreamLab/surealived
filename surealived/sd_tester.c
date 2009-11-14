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

#include <common.h>
#include <sd_tester.h>
#include <sd_maincfg.h>
#include <sd_log.h>
#include <sd_offline.h>
#include <sd_ipvssync.h>
#include <sd_cmd.h>

#define STARVATION_COUNTER 3    /* defines how many operations should happen until break loop */
SDepoll *epoll;

/* returns FALSE if t1 is less than t2 (TRUE otherwise) */
#define TIME_HAS_COME(t1,t2) ( (t2.tv_sec - t1.tv_sec > 0) ? TRUE :     \
        ((t2.tv_sec - t1.tv_sec == 0) && (t2.tv_usec - t1.tv_usec > 0)) ? TRUE : FALSE )

/* the name is misleading it returns random value from 0 to startup_delay_ms */
#define RANDSEC (int)((double)G_startup_delay_ms*rand()/(RAND_MAX+1.0))

guint        tests_running = 0;  /* for gentle restart */
gboolean     stop = FALSE;

/* Statistics counters */
guint        G_stats_test_success = 0; //counter for single test
guint        G_stats_test_failed  = 0; 
guint        G_stats_online_set   = 0; //counter (switch to online)
guint        G_stats_offline_set  = 0; //counter (switch to offline)
guint        G_stats_conn_problem = 0; //counter (if arp reply is ok (no EPOLLERR occured in connect function) that means there's no SYN,ACK segment)
guint        G_stats_arp_problem  = 0; //counter (if EPOLLERR occured before connect - suppose, there's no arp reply for that host)
guint        G_stats_bytes_rcvd   = 0;
guint        G_stats_bytes_sent   = 0;

/* ---------------------------------------------------------------------- */
/* === Private methods === */
static gint sd_read(CfgReal *real) {
    gint    ret;
    gint    len = REQ_LEN(real->req);
    gchar   brk = STARVATION_COUNTER;

    real->error = 0;
    while (brk && real->pos < len) {
        if (!real->ssl && (ret = read(real->fd, real->buf + real->pos, len - real->pos)) > 0) {
            if (G_debug_comm == TRUE && real->tester->debugcomm && ret > 0)
                sd_append_to_commlog(real, real->buf + real->pos, ret);
            real->pos += ret;
            G_stats_bytes_rcvd += ret;
            brk--;
        }
        else if (real->ssl && (ret = SSL_read(real->ssl, real->buf + real->pos, len - real->pos)) > 0) {
            if (G_debug_comm == TRUE && real->tester->debugcomm && ret > 0)
                sd_append_to_commlog(real, real->buf + real->pos, ret);
            real->pos += ret;
            G_stats_bytes_rcvd += ret;
            brk--;
        }
        else
            break;
    }
    real->bytes_read = real->pos;

    LOGDETAIL("READ: %d/%d bytes read (real = [%s:%s])\n", real->pos, len, real->virt->name, real->name);

    if (!brk)
        return 1;

    if (ret < 0) {
        if (!real->ssl && errno == EAGAIN)    /* read all buffer */
            return 1;
        else if (real->ssl && SSL_get_error(real->ssl, ret) == SSL_ERROR_WANT_READ)
            return 1;
        else {                   /* some error occured */
            real->error = errno;
            return -1;
        }
    }
    if (!ret)
        real->error = EOF;

    return 0;                   /* request completed! */
}

static gint sd_read_av(CfgReal *real) { /* read and return after one read */
    gint ret;
    gint len = REQ_LEN(real->req);

    real->error = real->bytes_read = 0;
    if (!real->ssl)
        ret = read(real->fd, real->buf, len); /* set buf end */
    else
        ret = SSL_read(real->ssl, real->buf, len);

    LOGDETAIL("READ_AV: %d bytes read (wanted = %d) (real = [%s:%s]\n", ret, len, 
              real->virt->name, real->name);

    if (ret > 0) {
        real->bytes_read = ret;
        G_stats_bytes_rcvd += ret;
        return 0;               /* some data read */
    }

    if (!ret) {
        real->error = EOF;
        return 0;               /* EOF reached! */
    }

    if (ret < 0) {
        if (!real->ssl && errno == EAGAIN)
            return 1;           /* no data */
        if (real->ssl && SSL_get_error(real->ssl, ret) == SSL_ERROR_WANT_READ)
            return 1;
    }
    real->error = errno;
    return -1;
}

static gint sd_write(CfgReal *real) {
    gint    ret;
    gint    len = REQ_LEN(real->req);
    gchar   brk = STARVATION_COUNTER;

    real->error = 0;

    while (brk && real->pos < len) {
        if (!real->ssl && (ret = write(real->fd, real->buf + real->pos, len - real->pos)) > 0) {
            if (G_debug_comm == TRUE && real->tester->debugcomm && ret > 0)
                sd_append_to_commlog(real, real->buf + real->pos, ret);
            real->pos += ret;
            G_stats_bytes_sent += ret;
            brk--;
        }
        else if (real->ssl && (ret = SSL_write(real->ssl, real->buf + real->pos, len-real->pos)) > 0) {
            if (G_debug_comm == TRUE && real->tester->debugcomm && ret > 0)
                sd_append_to_commlog(real, real->buf + real->pos, ret);
            real->pos += ret;
            G_stats_bytes_sent += ret;
            brk--;
        }
        else
            break;
    }
    LOGDETAIL("WRITE: %d/%d bytes written (real = [%s:%s]\n", real->pos, len, real->virt->name, real->name);

    if (!brk)                   /* do not starve other tests! */
        return 1;

    if (!ret)
        real->error = errno;

    if (ret < 0) {
        if ((!real->ssl && errno == EAGAIN) ||    /* request not completed! some data still to write */
            (real->ssl && SSL_get_error(real->ssl, ret) == SSL_ERROR_WANT_WRITE))
            return 1;
        else {
            real->error = errno;
            return -1;          /* error! */
        }
    }
    return 0;                   /* request completed! */
}

static gint sd_eof(CfgReal *real) {
    gchar   buf[BUFSIZ];
    gint    ret = 0;

    real->error = 0;
    ret = read(real->fd, buf, BUFSIZ);

    LOGDETAIL("READ(EOF): %d bytes read, (wanted = %d) (real = [%s,%s])\n", ret, BUFSIZ, 
              real->virt->name, real->name);

    if (G_debug_comm == TRUE && real->tester->debugcomm && ret > 0)
        sd_append_to_commlog(real, buf, ret);

    real->bytes_read += ret;
    G_stats_bytes_rcvd += ret;

    if (ret < 0 && errno != EAGAIN)
        real->error = errno;

    return ret;
}

/* ---------------------------------------------------------------------- */
static void sd_start_real(CfgReal *real) {
    if (!real)
        return;

    if (real->tester->mops->m_test_protocol == SD_PROTO_TCP) {
        real->fd = sd_socket_nb(SOCK_STREAM);
        LOGDETAIL("TCP socket fd = [%d]", real->fd);
    }
    else if (real->tester->mops->m_test_protocol == SD_PROTO_UDP) {
        real->fd = sd_socket_nb(SOCK_DGRAM);
        LOGDETAIL("UDP socket fd = [%d]", real->fd);
    }

    if (real->tester->mops->m_test_protocol == SD_PROTO_TCP)
        sd_socket_solinger(real->fd);
    else {
        if (real->tester->mops->m_test_protocol != SD_PROTO_UDP)
            real->fd = -1;
    }

    LOGDEBUG("Starting real [%s:%s] (fd = %d)!", real->virt->name, real->name, real->fd);
    if (real->tester->mops->m_test_protocol == SD_PROTO_NO_TEST) {
        real->test_state = TRUE;
        gettimeofday(&real->start_time, NULL);
    }
    else
        real->test_state = FALSE; /* we assume a node is offline */

    real->end_time.tv_sec  = real->end_time.tv_usec = 0;  /* reset end_time */
    real->conn_time.tv_sec = real->conn_time.tv_usec = 0; /* reset connection time */

    real->req = 0;              /* reset request */
    if (real->tester->mops->m_prepare)
        real->tester->mops->m_prepare(real);
        

    if (real->tester->mops->m_test_protocol == SD_PROTO_TCP ||
        real->tester->mops->m_test_protocol == SD_PROTO_UDP) {
        sd_epoll_ctl(epoll, EPOLL_CTL_ADD, real->fd, (void *)real, EPOLLOUT); /* add to epoll */
        gettimeofday(&real->start_time, NULL);  /* set real's start time */
    }

    if (real->tester->mops->m_test_protocol == SD_PROTO_TCP ||
        real->tester->mops->m_test_protocol == SD_PROTO_UDP) {
        sd_socket_connect(real->fd, real->addr, real->testport);

        if (real->ssl && sd_bind_ssl(real))
            LOGWARN("Unable to bind ssl to socket (real = [%s:%s]!", real->virt->name, real->name);
    }
    else if (real->tester->mops->m_test_protocol == SD_PROTO_EXEC)
        real->tester->mops->m_start(real);

    if (G_debug_comm == TRUE)
        sd_unlink_commlog(real);
}

static void sd_tester_start_real(CfgVirtual *virt) {
    guint n = virt->last_real + virt->nodes_to_run;

    for (; virt->last_real < n && virt->last_real < virt->realArr->len; ++virt->last_real) {
        sd_start_real(g_ptr_array_index(virt->realArr, virt->last_real));
    }

    if (virt->nodes_rest && virt->last_real < virt->realArr->len) {
        sd_start_real(g_ptr_array_index(virt->realArr, virt->last_real++));
        virt->nodes_rest--;
    }

    if (virt->realArr && virt->last_real == virt->realArr->len) { /* the last one */
        gettimeofday(&virt->end_time, NULL);
        time_inc(&virt->end_time, virt->tester->timeout, 0);
    }
}

static void sd_tester_set_virtual_time(CfgVirtual *virt) {
    struct timeval ctime;

    if (!virt->realArr)
        return;

    virt->last_real = virt->nodes_to_run = virt->nodes_rest = 0;

    /* Use nice connection startup schedule algoritm:
       ex. virtual has 23 reals, loop_interval_ms == 100ms, startup_delay_ms = 1000ms
       - virtual startup is divided to 10 periods (startup_delay_ms == 1000 / loop_interval_ms == 100)
       - nodes_to_run = (100 * 23 / 1000) == 2 (we connect to 2 reals at every period, but this gives
         us only 20 reals tested)
       - nodes_rest = (23 % (1000/100)) == 3 (at period 0,1,2 we'll connect to additional one real)
    */
    if (G_startup_delay_ms && G_loop_interval_ms) {
        virt->nodes_to_run = G_loop_interval_ms * virt->realArr->len / G_startup_delay_ms;
        virt->nodes_rest = virt->realArr->len % (G_startup_delay_ms / G_loop_interval_ms);
    }
    else
        virt->nodes_to_run = virt->realArr->len;

    gettimeofday(&ctime, NULL);
    virt->end_time.tv_sec = 0;

    if (!virt->start_time.tv_sec) { /* first run */
        virt->start_time = ctime;                           /* current time */
        time_inc(&virt->start_time, 0, RANDSEC);            /* let's randomize virtual start time by 1 sec */
        return;
    }
    virt->start_time = ctime;
    time_inc(&virt->start_time, virt->tester->loopdelay, 0); /* increase start_time by loopdelay */
    virt->changed = FALSE; 
}

static gboolean sd_tester_eval_state(CfgReal *real) {
    gboolean real_state_changed = FALSE;

    /* 1) Increment real->retries_ok if SUCCESS
       2) Increment real->retries_fail if FAIL */
    if (real->test_state) {
        if (real->retries_ok != real->tester->retries2ok)
            real->retries_ok++;
        real->retries_fail = 0;
        G_stats_test_success++;
    }
    else {
        if (real->retries_fail != real->tester->retries2fail)
            real->retries_fail++;
        real->retries_ok = 0;
        G_stats_test_failed++;
    }

    LOGDEBUG("eval state, real = %s:%s, last_online=%s, online=%s, rok=%d r2ok=%d rfail=%d r2fail=%d", 
              real->virt->name, real->name, GBOOLSTR(real->last_online), GBOOLSTR(real->online),
              real->retries_ok, real->tester->retries2ok, real->retries_fail, real->tester->retries2fail);

    if (real->retries_ok == real->tester->retries2ok) {
        real->last_online = real->online;
        real->online = TRUE;
        if (real->last_online != real->online) {
            real->diff = TRUE;
            G_stats_online_set++;
            LOGINFO("Real: [%s:%s - %s:%d:%s %s:%d] changed its test state to ONLINE (rstate = %s)", 
                    real->virt->name, real->name,
                    real->virt->addrtxt, ntohs(real->virt->port),
                    sd_proto_str(real->virt->ipvs_proto),
                    real->addrtxt, ntohs(real->port),
                    sd_rstate_str(real->rstate));
            sd_offline_dump_del(real);
            real_state_changed = TRUE;
        }
    }
    else if (real->retries_fail == real->tester->retries2fail) {
        real->last_online = real->online;
        real->online = FALSE;
        if (real->last_online != real->online) { 
            real->diff = TRUE;
            G_stats_offline_set++;
            LOGINFO("Real: [%s:%s - %s:%d:%s %s:%d] changed its test state to OFFLINE (rstate = %s)", 
                    real->virt->name, real->name,
                    real->virt->addrtxt, ntohs(real->virt->port),
                    sd_proto_str(real->virt->ipvs_proto),
                    real->addrtxt, ntohs(real->port),
                    sd_rstate_str(real->rstate));
            sd_offline_dump_add(real);
            real_state_changed = TRUE;
        }
    } 

    if (real_state_changed)
        real->virt->changed = TRUE;

    return real_state_changed;
}

static void sd_tester_virtual_expired(SDTester *sdtest, CfgVirtual *virt) {
    gint        i;
    CfgReal     *real;

    if (!virt || !virt->realArr)
        return;

    i = virt->realArr->len-1;

    for (; i>=0; i--) {
        real = g_ptr_array_index(virt->realArr, i);

        if (virt->tester->mops->m_test_protocol == SD_PROTO_EXEC)
            virt->tester->mops->m_check(real);
        else if (virt->tester->mops->m_test_protocol == SD_PROTO_NO_TEST)
            real->conn_time = real->end_time = real->start_time;

        LOGDEBUG("Real [%s:%s] test status: %s", real->virt->name, real->name, GBOOLSTR(real->test_state));

        LOGDEBUG("Eval: virtual expired [%s, virt changed = %s]", virt->name, GBOOLSTR(virt->changed));
        sd_tester_eval_state(real);

        if (!real->end_time.tv_sec)
            real->end_time = virt->end_time;

        /* I. When we have default kernel settings for arp solicitation
           # net.ipv4.neigh.IFACE.retrans_time_ms == 1000 
           # net.ipv4.neigh.IFACE.mcast_solicit == 3
           
           if (conn_time.tv_sec == 0) we have 2 possible situations:
           - tester timeout <= 3 sec:
             a) no ARP response (EPOLLERR occured on descriptor)
             b) we have ARP, but SYN frame was dropped in the network
           - tester timeout > 3 sec:
             we have ARP but SYN frame was dropped elsewhere (that's why
             conn_time.sec == 0)

           II. When we have customized kernel settings 
           You need to set valid timeout attribute in xml configuration.
           For example if you have retrans_time_ms == 500 and mcast_solicit == 2
           descriptor will receive EPOLLERR after 1 sec. In that case setting
           timeout=2 is good choice (of course if no SYN frame will be dropped
           to your realserver).

           Resume:
           Suggested timeout should be greater than (retrans_time_ms * mcast_solicit).           
        */
        if (!real->conn_time.tv_sec) {
            real->conn_time = real->start_time; //perhaps connection established fail
            real->conn_time.tv_usec -= 1000;    //if timeout > 3sec - this means that no SYN,ACK
            G_stats_conn_problem++;
        }

        /* last_online - useful when online status change - only this real need to be
           stored in diff file */
        LOGDEBUG("Stopping real [%s:%s] [fd = %d, test_state = %s, online = %s, last_online = %s, "
            "retries_ok = (%d of %d), retries_fail = (%d of %d)]",
            real->virt->name,
            real->name,
            real->fd,
            GBOOLSTR(real->test_state),
            GBOOLSTR(real->online),
            GBOOLSTR(real->last_online),
            real->retries_ok,
            real->tester->retries2ok,
            real->retries_fail,
            real->tester->retries2fail);

        /* Close a descriptor only for timeouted connections */
        if (real->fd != -1)
            close(real->fd);
        if (real->ssl)
            SSL_clear(real->ssl);
        if (real->tester->mops->m_cleanup)
            real->tester->mops->m_cleanup(real);
        real->fd = -1;
    }

    virt->tested = TRUE;

    sd_log_stats(virt);

    if (virt->changed) //save offline dump if any real changed its state
        sd_offline_dump_save();

    if (!G_no_sync) {             /* check if we only need stats */
        if (sd_ipvssync_save_fullcfg(sdtest->VCfgArr, FALSE))
            sd_ipvssync_diffcfg_virt(virt); 
    }

    virt->last_real = 0;
    virt->end_time.tv_sec = 0;
}

static void sd_tester_process_virt(gpointer virtptr, gpointer dataptr) {
    CfgVirtual     *virt = (CfgVirtual *) virtptr;
    SDTester       *sdtest = (SDTester *) dataptr;
    struct timeval  ctime;

    switch(virt->state) {
    case NOT_READY:             /* first run */
        sd_tester_set_virtual_time(virt);
        virt->state = READY_TO_GO;
        break;
    case READY_TO_GO:
        gettimeofday(&ctime, NULL);
        if (!stop && TIME_HAS_COME(virt->start_time, ctime)) { /* it is time to start the nodes */
            sd_tester_start_real(virt);
            virt->state = RUNNING;
            if (virt->realArr) //count tests only if there are reals in such a virtual
                tests_running++;
        }
        break;
    case RUNNING:
        gettimeofday(&ctime, NULL);
        if (virt->end_time.tv_sec && TIME_HAS_COME(virt->end_time, ctime)) { /* this virtual tester has expired */
            sd_tester_virtual_expired(sdtest, virt);
            if (stop)
                LOGINFO(" * reloading (SIGHUP) - tests running: %d", tests_running);
            tests_running--;
            sd_tester_set_virtual_time(virt);
            virt->state = READY_TO_GO;
        }
        else if (!virt->end_time.tv_sec) /* do we have some nodes left? */
            sd_tester_start_real(virt);
        break;
    }
}

inline gboolean sd_tester_loop(SDTester *sdtest) {
    struct timeval ctime;

    if (sdtest && sdtest->VCfgArr) {
        gettimeofday(&ctime, NULL);
        if (TIME_HAS_COME(sdtest->not_before, ctime)) {
            g_ptr_array_foreach(sdtest->VCfgArr, sd_tester_process_virt, sdtest);
            sdtest->not_before = ctime;
            time_inc(&sdtest->not_before, 0, G_loop_interval_ms);
        }
    }
    return 0;
}

static SDTester *sd_tester_new(GPtrArray *VCfgArr) {
    struct timeval ctime;
    SDTester *sdtest = g_new0(SDTester, 1);

    sdtest->VCfgArr = VCfgArr;
    sdtest->realcount = 0;
    gettimeofday(&ctime, NULL);
    sdtest->not_before = ctime;
    
    return sdtest;
}

static void sd_tester_prepare_virt(gpointer virtptr, gpointer userdata) {
    CfgVirtual *virt   = (CfgVirtual *) virtptr;
    SDTester   *sdtest = (SDTester *) userdata;

    if (!virt->tester) //virtual without tester tags defined
        return;

    if (virt->tester->mops->m_test_protocol == SD_PROTO_TCP ||
        virt->tester->mops->m_test_protocol == SD_PROTO_UDP) { /* all EXEC and NO_TEST reals should NOT be included in epoll */
        if (virt->realArr)
            sdtest->realcount += virt->realArr->len;
    }
}

/*! \brief Create \a SDTester structure based on VCfgArr.
  \param VCfgArr Configuration array (VCfgArr = sd_xmlParseFile(...))
  \return SDTester structure
*/
SDTester *sd_tester_create(GPtrArray *VCfgArr) {
    gint realsum = 0;

    SDTester *sdtest = sd_tester_new(VCfgArr);
    assert(sdtest);

    g_ptr_array_foreach(VCfgArr, sd_tester_prepare_virt, sdtest); //update necessary struct fields in virtuals

    G_epoll_size = MAX(G_epoll_size, EPOLL_DEFAULT_EVENTS);
    realsum = sdtest ? sdtest->realcount : 0;
    LOGDEBUG("Creating epoll for tester %d/%d", realsum, G_epoll_size);

    epoll = sd_epoll_new(G_epoll_size);

    return sdtest;
}


static void sd_tester_process_events(gint nr_events) {
    CfgReal *real = NULL;
    gchar   s[32];
    gint    err;
    int     i;

    for (i = 0; i < nr_events; i++) {
        uint32_t ev = sd_epoll_event_events(epoll, i);
        real = (CfgReal *) sd_epoll_event_dataptr(epoll, i);
        sd_epoll_event_str(ev, s);
        LOGDETAIL("Process events on real = %s:%s, fd = %d [%s]", real->virt->name, real->name, real->fd, s);

        if (ev & EPOLLERR) {
            LOGDETAIL("EPOLLERR: closing descriptor for real = %s, fd = %d\n", real->virt->name, real->name, real->fd);
            if (real->fd != -1)
                close(real->fd);
            real->fd = -1; //to avoid closing descriptor which belongs to another connection
            if (real->ssl)
                SSL_clear(real->ssl);

            /* If EPOLLERR occured and conn_time wasn't already set - this perhaps
               means, we have arp problem to the real server */
            if (!real->conn_time.tv_sec) {
                real->conn_time = real->start_time;
                real->conn_time.tv_usec -= 2000; //arp problem?
                G_stats_arp_problem++;
            }

            LOGDEBUG("Eval: process events (EPOLLERR)");
            if (sd_tester_eval_state(real)) {
                LOGDEBUG("ipvssync diffcfg real (EPOLLERR)");
                sd_ipvssync_diffcfg_real(real, FALSE); 
            }
            continue;
        }

        if (!real->ssl && !real->conn_time.tv_sec)
            gettimeofday(&real->conn_time, NULL);
        else if (real->ssl && !real->conn_time.tv_sec) {
            if ((err = SSL_connect(real->ssl)) == -1) { /* connection not completed */
                if (SSL_get_error(real->ssl, err) == SSL_ERROR_WANT_WRITE)
                    sd_epoll_ctl(epoll, EPOLL_CTL_MOD, real->fd, real, EPOLLOUT);
                else if (SSL_get_error(real->ssl, err) == SSL_ERROR_WANT_READ)
                    sd_epoll_ctl(epoll, EPOLL_CTL_MOD, real->fd, real, EPOLLIN);
                else {            /* error! */
                    LOGDEBUG("Error connect, real = %s:%s, fd = %d", real->virt->name, real->name, real->fd);
                    if (real->fd != -1)
                        close(real->fd);
                    real->fd = -1;
                    SSL_clear(real->ssl);
                }
                continue;
            }
            else if (err == 1)
                gettimeofday(&real->conn_time, NULL);
        }

        if (REQ_READ(real->req)) { /* we wanted to read */
            LOGDETAIL("REQ READ: (real %s:%s)", real->virt->name, real->name);
            if ((err = sd_read(real)) > 0)
                continue;       /* not completed */
            else if (err < 0)   /* error */
                LOGDEBUG("Error on (real %s:%s) sd_read: %s\n", real->virt->name, real->name, strerror(errno));
        }
        else if (REQ_READ_AV(real->req)) { /* module wanted to read ANY data */
            LOGDETAIL("REQ READ AV: (real %s:%s)", real->virt->name, real->name);
            if ((err = sd_read_av(real)) > 0)
                continue;
            else if (err < 0)           /* error */
                LOGDEBUG("Error on sd_read_av: (real %s:%s) %s\n", real->virt->name, real->name, strerror(errno));
        }
        else if (REQ_WRITE(real->req)) { /* we wanted to write */
            LOGDETAIL("REQ WRITE: (real %s:%s)", real->virt->name, real->name);
            if ((err = sd_write(real)) > 0)
                continue;       /* request not completed */
            else if (err < 0)   /* error */
                LOGDEBUG("Error on sd_write: (real %s:%s) %s\n", real->virt->name, real->name, strerror(errno));
        }
        else if (REQ_EOF(real->req)) { /* drain the socket */
            LOGDETAIL("REQ EOF: (real %s:%s)", real->virt->name, real->name);
            if ((err = sd_eof(real)) > 0)
                continue;
            else if (err < 0)   /* error */
                LOGDEBUG("Error on sd_eof: (real %s:%s) %s\n", real->virt->name, real->name, strerror(errno));
        }

        LOGDETAIL("ERR = %d (real %s:%s)", err, real->virt->name, real->name);
        /* we get to this point only if error occured on socket (or EOF) */
        /* or if request is complete */
        real->req = real->tester->mops->m_process_event(real);
        real->error = real->bytes_read = 0; /* very important to reset bytes_read */

        if (REQ_END(real->req)) {
            LOGDEBUG("Requesting END (real = %s:%s, fd = %d)", real->virt->name, real->name, real->fd);
            if (real->fd != -1)
                close(real->fd);
            real->fd = -1;
            if (real->ssl)
                SSL_clear(real->ssl);
            gettimeofday(&real->end_time, NULL); /* set real's end time */

            LOGDEBUG("Eval: process events (request end)");
            if (sd_tester_eval_state(real)) {
                LOGDEBUG("ipvssync diffcfg real (request end)");
                sd_ipvssync_diffcfg_real(real, FALSE); 
            }
        }
        else if (REQ_WRITE(real->req))
            sd_epoll_ctl(epoll, EPOLL_CTL_MOD, real->fd, real, EPOLLOUT);

        else if (REQ_READ(real->req) || REQ_READ_AV(real->req))
            sd_epoll_ctl(epoll, EPOLL_CTL_MOD, real->fd, real, EPOLLIN);

        else if (REQ_EOF(real->req))
            sd_epoll_ctl(epoll, EPOLL_CTL_MOD, real->fd, real, EPOLLIN);

        real->pos = 0;
    }
}

gint sd_tester_master_loop(SDTester *sdtest) {
    gint        nr_events = 0;
    gint        retv = 0;
    sigset_t    blockset;
    sigset_t    oldset;

    sigfillset(&blockset);

    /* avoid receiving signals */
    sigprocmask(SIG_BLOCK, &blockset, &oldset);
    sd_tester_loop(sdtest);
    sd_tester_debug(sdtest);
    sigprocmask(SIG_SETMASK, &oldset, NULL); 

    LOGDETAIL("epoll_interval_ms = %lu\n", G_epoll_interval_ms);
    while (1) {
        sigprocmask(SIG_BLOCK, &blockset, &oldset); /* start critical section */
        if (epoll)
            nr_events = sd_epoll_wait(epoll, G_epoll_interval_ms);
        else
            usleep(G_epoll_interval_ms*1000);
        if (nr_events)
            LOGDETAIL("received: %d events!", nr_events);

        sd_tester_process_events(nr_events);

        sd_tester_loop(sdtest);

        /* if signal detected - sighup, sigint, sigterm - cleanup and exit loop */
        //retv = ... and break

        if (logic_epoll)        /* deal with clients */
            sd_cmd_loop(sdtest->VCfgArr);
        
        sigprocmask(SIG_SETMASK, &oldset, NULL); /* release critical section */

        if (stop && !tests_running) /* we need to stop NOW */
            break;
    }

    return retv;
}

void sd_tester_debug(SDTester *sdtest) {
    guint       i,j;
    CfgVirtual  *virt;
    CfgReal     *real;

    if (!sdtest || !sdtest->VCfgArr)
        return;

    for (i = 0; i < sdtest->VCfgArr->len; i++) {
        virt = g_ptr_array_index(sdtest->VCfgArr, i);
        printf("-----------------------------\n");
        printf("Virtual: %s [%s:%d]\n\t\t\tStart: %lds %ldus\n\t\t\tEnd  : %lds %ldus\n\n",
            virt->name, inet_ntoa(*((struct in_addr*)&virt->addr)), ntohs(virt->port),
            (long)virt->start_time.tv_sec, (long)virt->start_time.tv_usec,
            (long)virt->end_time.tv_sec, (long)virt->end_time.tv_usec);

        printf("\tTester: [%s] loopdelay=%d timeout=%d testport=%d exec=%s\n",
            virt->tester->proto, virt->tester->loopdelay, virt->tester->timeout,
            ntohs(virt->tester->testport), virt->tester->exec ? virt->tester->exec : "null");

        if (virt->realArr) {
            for (j = 0; j < virt->realArr->len; j++) {
                real = g_ptr_array_index(virt->realArr, j);
                printf("\tReal: [%s:%d] (%s:%d)\tteststate=%s, rstate=%s\n",
                       real->name, ntohs(real->testport),
                       inet_ntoa(*((struct in_addr *)&real->addr)), ntohs(real->port),
                       real->online ? "ONLINE" : "OFFLINE",
                       sd_rstate_str(real->rstate));
            }
            printf("\n");
        }
    }
}
