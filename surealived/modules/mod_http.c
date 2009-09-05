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

#include <stdio.h>
#include <glib.h>
#include <common.h>
#include <modloader.h>
#include <sd_defs.h>
#include <xmlparser.h>

const gchar *request_template = "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: SD\r\n\r\n";

typedef struct {
    gchar       url[BUFSIZ];
    gchar       host[256];
    gboolean    naive;
    gchar      *request;
    gint        req_len;
    gchar       ans[64];
    gchar       retcode[4];
} TestHTTP;

enum {
    CONNECTED,
    REQUEST_SENT,
    REQUEST_RECEIVED,
    RECEIVING
} State;

static mod_operations mops;

static const gchar *name = "http";

static mod_args m_args[]={
    { "url",    STRING,     BUFSIZ, BASIC_ATTR, SDOFF(TestHTTP, url)    },
    { "host",   STRING,     256,    BASIC_ATTR, SDOFF(TestHTTP, host)   },
    { "naive",  BOOL,       -1,     EXTRA_ATTR, SDOFF(TestHTTP, naive)  },
    { "retcode", STRING,    4, EXTRA_ATTR, SDOFF(TestHTTP, retcode) },
    { NULL, 0,  0,  0,  0 },
};

static const gchar *module_name(void) {
    return name;
}

static gpointer module_set_args(void) {
    TestHTTP    *ret = g_new0(TestHTTP, 1);
    memset(ret, 0, sizeof(TestHTTP));
    return ret;
}

static void module_test_prepare(CfgReal *real) {
    TestHTTP    *t = (TestHTTP *)real->moddata;
    LOGDETAIL("http module test prepare");
    real->req = WANT_END;
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

        t->request = g_strdup_printf(request_template, t->url, t->host);

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

mod_operations __init_module(gpointer data) {
    LOGINFO(" ** Init module: setting mod_operations for tester [%s]", name);

    mops.m_name             = module_name;
    mops.m_test_protocol    = SD_PROTO_TCP;
    mops.m_set_args         = module_set_args;
    mops.m_args             = m_args;
    mops.m_prepare          = module_test_prepare;
    mops.m_process_event    = module_process_event;
    mops.m_free             = module_free;
    return mops;
}
