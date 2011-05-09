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

#include <stdio.h>
#include <glib.h>
#include <common.h>
#include <modloader.h>
#include <sd_defs.h>
#include <xmlparser.h>

#define REQUEST_TEMPLATE "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: SureAliveD\r\n\r\n"

typedef struct {
    gchar       url[BUFSIZ];
    gchar       host[256];
    gboolean    naive;
    gchar      *request;
    gint        req_len;
    gchar       ans[12];
    gchar       retcode[4];
} TestHTTP;

enum {
    CONNECTED,
    REQUEST_SENT,
    REQUEST_RECEIVED,
    RECEIVING
} State;

static mod_operations mops;

static mod_args m_args[] = {
    { "url",    STRING,     BUFSIZ, BASIC_ATTR, SDOFF(TestHTTP, url)    },
    { "host",   STRING,     256,    BASIC_ATTR, SDOFF(TestHTTP, host)   },
    { "naive",  BOOL,       -1,     EXTRA_ATTR, SDOFF(TestHTTP, naive)  },
    { "retcode", STRING,    4, EXTRA_ATTR, SDOFF(TestHTTP, retcode) },
    { NULL, 0,  0,  0,  0 },
};

static gpointer module_alloc_args(void) {
    TestHTTP    *ret = g_new0(TestHTTP, 1);
    memset(ret, 0, sizeof(TestHTTP));
    return ret;
}

static void module_test_prepare(CfgReal *real) {
    TestHTTP    *t = (TestHTTP *)real->moddata;
    LOGDETAIL("http module test prepare");

    real->intstate = CONNECTED;
    if (!t->retcode[0])
        strncpy(t->retcode, "200", 3);
}

static u_int32_t module_process_event(CfgReal *real) {
    TestHTTP    *t = (TestHTTP *)real->moddata;

    if (real->error)
        return WANT_END;

    if (real->intstate == CONNECTED) {
        real->intstate = REQUEST_SENT;     /* set next state */

        real->buf = t->request;
        if (t->request)
            return WANT_WRITE(t->req_len);

        t->request = g_strdup_printf(REQUEST_TEMPLATE, t->url, t->host);

        real->buf  = t->request;
        t->req_len = strlen(t->request); /* and try to remember the length of it */

        LOGDETAIL("MOD_HTTP: requesting to write %d(%d) bytes", t->req_len, REQ_LEN(WANT_WRITE(t->req_len)));
        return WANT_WRITE(t->req_len);
    }
    else if (real->intstate == REQUEST_SENT) {
        real->buf = t->ans;     /* drop answer to buf */
        memset(real->buf, 0, sizeof(t->ans));
        LOGDETAIL("MOD_HTTP: requesting to read %d(%d) bytes", sizeof(t->ans), REQ_LEN(WANT_READ(sizeof(t->ans))));
        real->intstate = REQUEST_RECEIVED;
        return WANT_READ(sizeof(t->ans));
    }
    else if (real->intstate == REQUEST_RECEIVED) {
        if (!strncmp(real->buf+9, t->retcode, 3)) {
            LOGDETAIL("REAL ONLINE [virt:%s,%s]!", real->virt->name, real->name);
            real->test_state = TRUE;
        }
        else
            LOGDETAIL("REAL OFFLINE [virt:%s,%s]!", real->virt->name, real->name);

        if (t->naive)
            return WANT_END;

        real->intstate = EOF;
        return WANT_EOF;        /* notify me when you notice eof */
    }
    else {                      /* for statistics */
        LOGDETAIL("EOF on socket - statistics should be sent!");
    }

    return WANT_END;
}

static void module_free(CfgReal *real) {
    TestHTTP    *t = (TestHTTP *)real->moddata;
    if (t->request)
        free(t->request);
    t->request = NULL;
}

mod_operations __init_module(void) {
    LOGINFO(" ** Init module: setting mod_operations for tester [http]");

    mops.m_name             = "http";
    mops.m_test_protocol    = SD_PROTO_TCP;
    mops.m_args             = m_args;

    mops.m_alloc_args       = module_alloc_args;
    mops.m_prepare          = module_test_prepare;
    mops.m_process_event    = module_process_event;
    mops.m_free             = module_free;

    return mops;
}
