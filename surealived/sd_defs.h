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

#if !defined __SD_DEFS_H
#define __SD_DEFS_H
#include <glib.h>
#include <openssl/ssl.h>

#ifndef TIME_HAS_COME
#define TIME_HAS_COME(t1,t2) ( (t2.tv_sec - t1.tv_sec > 0) ? TRUE :     \
        ((t2.tv_sec - t1.tv_sec == 0) && (t2.tv_usec - t1.tv_usec > 0)) ? TRUE : FALSE )
#endif

#define MAXNAME         128
#define MAXIPTXT        16          /* 123.567.901.345 */
#define MAXPORTTXT      8           /* 65535 */
#define MAXSCHED        16          /* wrr */
#define MAXPROTO        16

/* those macros define what module can request from SD */
#define SD_READ         0x10000000
#define SD_WRITE        0x20000000
#define SD_READ_AV      0x30000000
#define SD_EOF          0x40000000
#define SD_END          0x00000000
#define SD_MASK         0xF0000000

#define WANT_READ(x)    ((u_int32_t)(x)|SD_READ)
#define WANT_WRITE(x)   ((u_int32_t)(x)|SD_WRITE)
#define WANT_READ_AV(x) ((u_int32_t)(x)|SD_READ_AV)
#define WANT_EOF        SD_EOF
#define WANT_END        SD_END

#define REQ_READ(x)     (((u_int32_t)(x)&SD_MASK) == SD_READ ? 1 : 0)
#define REQ_WRITE(x)    (((u_int32_t)(x)&SD_MASK) == SD_WRITE ? 1 : 0)
#define REQ_READ_AV(x)  (((u_int32_t)(x)&SD_MASK) == SD_READ_AV ? 1 : 0)
#define REQ_EOF(x)      (((u_int32_t)(x)&SD_MASK) == SD_EOF ? 1 : 0)
#define REQ_END(x)      ((u_int32_t)(x)&SD_MASK ? 0 : 1)
#define REQ_LEN(x)      ((u_int32_t)(x)&(u_int32_t)(~SD_MASK))

typedef u_int32_t REQUEST;

typedef enum {
    SD_IPVS_TCP,
    SD_IPVS_UDP,
    SD_IPVS_FWMARK
} SDIPVSProtocol;

typedef enum {
    SD_IPVS_RT_DR,
    SD_IPVS_RT_MASQ,
    SD_IPVS_RT_TUN
} SDIPVSRt;

typedef enum {
    NOT_READY,
    READY_TO_GO,
    RUNNING,
} VState;

/* a) REAL_ONLINE - real is tested and 'online' is used when conf data
      for ipvssync are created
   b) REAL_OFFLINE - user manually turned off real. For real which has:
      - remove_on_fail == 0 weight is set to 0
      - remove_on_fail == 1 real is removed from ipvs
   c) REAL_DOWN - real is removed from ipvs 
*/
typedef enum {
    REAL_ONLINE,
    REAL_OFFLINE,
    REAL_DOWN,
} RState;

typedef struct {
    gchar           proto[MAXPROTO];
    struct mod_operations  *mops;
    guint           loopdelay;
    guint           timeout;
    u_int16_t       testport;
    guint           retries2ok;
    guint           retries2fail;
    guint           remove_on_fail;
    guint           debugcomm;            /* debug communication */
    guint           logmicro;             /* save statistics in microseconds instead of miliseconds */
    gchar          *exec;                 /* command to execute for EXEC */
    gchar           ssl;
    gpointer        moddata;              /* module data (ie url for HTTP tester) */
} CfgTester;

typedef struct {
    /* virtual data */
    CfgTester      *tester;
    GPtrArray      *realArr;              /* real Array */
    gboolean        tested;               /* did we test it already? */
    gboolean        changed;              /* is changed in one virtual real test loop? need to offline dump save */
    gchar           name[MAXNAME];
    gchar           addrtxt[MAXIPTXT];
    gchar           porttxt[MAXPORTTXT];
    in_addr_t       addr;
    u_int16_t       port;
    VState          state;                /* NOT_READY by default */

    guint           last_real;            /* last real started */
    guint           nodes_to_run;         /* nodes to run per loop interval */
    guint           nodes_rest;           /* rest of the nodes (modulo) */

    struct timeval  start_time;           /* this->end_time+this->tester->loopdelay */
    struct timeval  end_time;             /* this->start_time + this->tester->timeout */

    /* ipvs data */
    SDIPVSProtocol  ipvs_proto;           /* ipvs protocol - tcp/udp/fwmark */
    SDIPVSRt        ipvs_rt;              /* ipvs rt */
    gchar           ipvs_sched[MAXSCHED]; /* ipvs scheduler - wrr/wrc/...*/
    unsigned        ipvs_persistent;      /* persistent timeout (0 by default) */
    u_int32_t       ipvs_fwmark;          /* ipvs fwmark - (0 by default) */
} CfgVirtual;

typedef struct CfgReal {
    CfgVirtual     *virt;
    CfgTester      *tester;
    struct timeval  start_time;           /* those times are useful for statistics */
    struct timeval  conn_time;
    struct timeval  end_time;

    gchar          *buf;                  /* SD uses this buf for ALL operations */
    gpointer        moddynamic;           /* real's dynamic data */
    gint            pos;                  /* module should not use it (WANT_READ/WANT_WRITE) */
    u_int32_t       req;                  /* module request [ie. WANT_READ_AV(250) ] */
    gchar           error;                /* we set it when read/write operations fail */
    u_int32_t       bytes_read;           /* module should know how many bytes was read */
    SSL            *ssl;                  /* SSL should be NULL if not used */
    pid_t           pid;                  /* for EXEC */
    int             pgrp;                 /* process group (kill all childs spawned by tester)*/
    gboolean        diff;                 /* does this real changed? */

    gint            retries_ok;
    gint            retries_fail;
    RState          rstate;               /* real state set by user (ONLINE/OFFLINE/DOWN) */
    RState          last_rstate;          /* required to detect if a chgreal or addreal should be called */
    gboolean        test_state;           /* single test state */
    gboolean        online;               /* real state - evaluated in tests */
    gboolean        last_online;          /* to detect when real status changed {retries2(ok|fail)} */
    guint           intstate;             /* internal state (used by modules) */
    gint            fd;                   /* module should NOT use it */
    u_int16_t       testport;             /* (default = tester->testport) */

    gchar           name[MAXNAME];
    gchar           addrtxt[MAXIPTXT];
    gchar           porttxt[MAXPORTTXT];
    in_addr_t       addr;
    u_int16_t       port;

    unsigned        ipvs_rt;
    gint            ipvs_weight;
    gint            override_weight;
    gboolean        override_weight_in_percent;
    u_int32_t       ipvs_uthresh;
    u_int32_t       ipvs_lthresh;

    gchar           bindaddrtxt[MAXIPTXT]; /* not implemented yet :( */
    in_addr_t       bindaddr;

    gpointer        moddata;              /* real's private data (if different from tester's) */
} CfgReal;

#endif
