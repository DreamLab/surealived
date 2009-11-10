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

#include <stdio.h>
#include <glib.h>
#include <common.h>
#include <modloader.h>
#include <sd_defs.h>
#include <xmlparser.h>
#include <arpa/nameser_compat.h>
#include <ctype.h>

static mod_operations mops;

enum {
    DNS_TEST_START,
    DNS_TEST_SENT,
    DNS_TEST_END,
} DNSState;

typedef HEADER DNS_HEADER;

typedef struct {
    gchar   request_xml[256];   /* request inside xml (aka request="f1virt.onet.pl") */
    gchar   request[256];       /* parsed request (\x06f1virt\x04onet\x02pl) */
    gchar   *full_req;          /* complete DNS request */
    gint    req_len;            /* DNS request length */
} testDNS;

static mod_args m_args[] = {    /* this struct defines what we want from XML */
    { "request", STRING, 255, BASIC_ATTR, SDOFF(testDNS, request_xml)+1 }, /* to  make dots more simple */
    { NULL, 0, 0, 0, 0 },       /* we need to end this struct with NULLs */
};

typedef struct QUERY_HEADER {   /* dns query header */
    unsigned type   :16;
    unsigned class  :16;
} QUERY_HEADER;

typedef struct RESOURCE_HEADER { /* actually we're not using it for now */
    unsigned name   :32;
    unsigned type   :16;
    unsigned class  :16;
    unsigned ttl    :32;
    unsigned rdlen  :16;
    unsigned rdata  :16;
} RESOURCE_HEADER;

static gpointer module_alloc_args(void) {
    testDNS *ret = g_new(testDNS, 1);
    memset(ret, 0, sizeof(testDNS)); /* it's always a good idea to zero memory */
    return ret;
}

static void mod_dns_test_prepare(CfgReal *real) {
    testDNS *local = (testDNS *)real->moddata;
    gint    i;
    gchar   dot;

    real->intstate = DNS_TEST_START; /* reset tester state */

    if (!real->moddynamic) {
        real->moddynamic = malloc(sizeof(DNS_HEADER)); /* this buffer will hold answers from DNS */
        memset(real->moddynamic, 0, sizeof(DNS_HEADER));
    }

    /* this code is dedicated to Grzegorz Lyczba :-) */
    if (!local->request[0]) { /* check if we processed request_xml to request */
        local->request_xml[0] = '.';
        for (i = strlen(local->request_xml), dot = 0; i >= 0; i--, dot++) { /* this loop substitutes dots ('.') */
            local->request[i] = local->request_xml[i];                      /* with segment length */
            if (local->request_xml[i] == '.' && (local->request[i] = dot-1) && (dot = 0)); /* it really works */
        }
    }
    /* end of dedication to Grzegorz Lyczba */
}

static u_int32_t mod_dns_process_event(CfgReal *real) {
    DNS_HEADER      *dnsh;
    QUERY_HEADER    *qh;
    testDNS         *local = (testDNS *)real->moddata;
    gchar           *request = local->request;

    if (real->error)                        /* Error occured */
        return WANT_END;                    /* abort test */

    if (real->intstate == DNS_TEST_START) {
        if (local->full_req) {              /* request ready */
            real->buf = local->full_req;    /* remember to set buf pointer to correct buffer */
            real->intstate = DNS_TEST_SENT;
            return WANT_WRITE(local->req_len);
        }

        /* we don't have request prepared - let's do it now */
        local->req_len = sizeof(DNS_HEADER) + sizeof(QUERY_HEADER) + strlen(request) + 1; /* request length */
        local->full_req = (gchar *)malloc(local->req_len);

        dnsh    = (DNS_HEADER *)local->full_req;
        qh      = (QUERY_HEADER *)(local->full_req + sizeof(DNS_HEADER) + strlen(request) + 1);

        memset((void *)dnsh, 0, sizeof(DNS_HEADER));
        memset((void *)qh, 0, sizeof(struct QUERY_HEADER));
        dnsh->aa     = 1;       /* we want authoritative answer */
        dnsh->id     = (u_int16_t)rand(); /* randomize request id */
        dnsh->qdcount = htons(1); /* we send only one request */

        memcpy(local->full_req + sizeof(DNS_HEADER), request, strlen(request)+1);
        qh->type     = htons(6); /* SOA */
        qh->class    = htons(1);

        real->intstate = DNS_TEST_SENT;
        real->buf = local->full_req; /* remember, remember the fifth of november! */
        return WANT_WRITE(local->req_len);
    }
    else if (real->intstate == DNS_TEST_SENT) {
        memset(real->moddynamic, 0, sizeof(DNS_HEADER)); /* reset answer buffer */
        real->buf = real->moddynamic;                    /* and again switch pointer to ans buffer */
        real->intstate = DNS_TEST_END;
        return WANT_READ(sizeof(DNS_HEADER));
    }
    else if (real->intstate == DNS_TEST_END) {
        dnsh = (DNS_HEADER *)real->moddynamic;

        if (ntohs(dnsh->ancount)) { /* do we have ANY answer ? */
            real->test_state = TRUE;
            LOGDEBUG("REAL [%s] ONLINE!", real->name);
        }
        else
            LOGDEBUG("REAL [%s] OFFLINE!", real->name);
    }
    return WANT_END;
}

void mod_free(CfgReal *real) {  /* this function is called when real is removed! */
    if (real->moddynamic)       /* it's always better to double-check */
        free(real->moddynamic); /* free used ans buffer */
    if (((testDNS *)real->moddata)->full_req)
        free(((testDNS *)real->moddata)->full_req); /* free request buf */
    real->buf = NULL;                               /* zero pointers */
    real->moddynamic = NULL;
}

mod_operations __init_module(void) {
    LOGINFO(" ** Init module: setting mod_operations for tester [dns]");

    mops.m_name             = "dns";
    mops.m_test_protocol    = SD_PROTO_UDP; /* DNS test uses UDP protocol */
    mops.m_args             = m_args;       /* this struct defines what we want from xml */

    mops.m_alloc_args       = module_alloc_args;        /* this functions allocates local memory */
    mops.m_prepare          = mod_dns_test_prepare;     /* prepare test function */
    mops.m_process_event    = mod_dns_process_event;    /* here be dragons */
    mops.m_free             = mod_free;                 /* always free memory when deleting real */
    return mops;                                        /* return mod operations */
}
