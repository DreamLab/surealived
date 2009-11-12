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
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/debugXML.h>

#include <xmlparser.h>
#include <sd_defs.h>
#include <sd_tester.h>
#include <sys/types.h>
#include <sys/stat.h>

gboolean        parse_error = FALSE;

static CfgTester  CfgDefault = {
    .proto          = "",
    .mops           = NULL,
    .loopdelay      = 3,
    .timeout        = 5,
    .retries2fail   = 1,
    .retries2ok     = 1,
    .remove_on_fail = 0,
    .debugcomm      = 0
};

static void *sd_xml_attr(xmlNode *node, gchar *attr, void *dst, SD_MODARG_TYPE arg_type, int param, SD_ATTR_TYPE attr_type, gchar *error_fmt, ...) {
    gchar       errmsg[256];
    xmlChar     *tmp;
    va_list     args;
    u_int16_t   p;

    va_start(args, error_fmt);
    vsnprintf(errmsg, 256, error_fmt, args);
    va_end(args);
    tmp = xmlGetProp(node, BAD_CAST attr);
    if (!tmp) {
        if (attr_type == BASIC_ATTR) {
            LOGERROR(errmsg);
            parse_error = TRUE;
            free(tmp);
            return NULL;
        }
        else {                  /* not mandatory but we could set default value */
            LOGDEBUG(errmsg);
            if (arg_type == STRING)
                return NULL;
            if (arg_type == PORT)
                *(u_int16_t *)dst = htons(param);
            else
                *(int *)dst = param;
            free(tmp);
            return dst;            
        }
    }
    switch (arg_type) {
    case STRING:
        if (!strncpy(dst, (gchar *) tmp, param)) {
            LOGERROR("Can't strncpy [attr = %s, maxlen = %d]", attr, param);
            parse_error = TRUE; 
            free(tmp);
            return NULL;
        }
        break;
    case PORT:
        p = atoi((const char *)tmp);
        if (p < 0 || p > 65535) {
            LOGERROR("Invalid uint16 [attr = %s, port = %s]", attr, tmp);
            parse_error = TRUE;
            free(tmp);
            return NULL;
        }
        *(u_int16_t *)dst = htons(p);

        break;
    case UINT:
        *(int *)dst = atoi((const char *)tmp);
        if (*(int *)dst < 0) {
            if (attr_type == BASIC_ATTR) {
                LOGERROR("Invalid u_int32_t value [attr = %s, value = %s]", attr, tmp);
                parse_error = TRUE;
            }
            else
                LOGWARN("Invalid u_int32_t value [attr = %s, value = %s]", attr, tmp);

            *(int *)dst = 0;
            free(tmp);
            return NULL;
        }

        break;
    case INT:
        *(int *)dst = atoi((const char *)tmp);
        break;
    case BOOL:
        *(gboolean *)dst = 0;

        if (!strcasecmp((char *)tmp, "true") || !strcasecmp((char *)tmp, "on"))
            *(gboolean *)dst = TRUE;
        else if (!strcasecmp((char *)tmp, "false") || !strcasecmp((char *)tmp, "off"))
            *(gboolean *)dst = FALSE;
        else if (strlen((char *)tmp) == 1 && tmp[0] == '1')
            *(gboolean *)dst = TRUE;
        else if (strlen((char *)tmp) == 1 && tmp[0] == '0')
            *(gboolean *)dst = FALSE;
        else if (attr_type == EXTRA_ATTR)
            LOGWARN("Invalid boolean value [attr = %s, value = %s]", attr, tmp);
        else {
            LOGERROR("Invalid boolean value [attr = %s, value = %s]", attr, tmp);
            parse_error = TRUE;
            free(tmp);
            return NULL;
        }
            
        break;
    default:
        LOGDEBUG("unknown argument type: %d\n", arg_type);
        free(tmp);
        return NULL;
    }
    free(tmp);

    return dst;
}

static gint sd_parse_mod_args(mod_args  *m,
                              gpointer   mdptr,
                              xmlNode   *node, 
                              CfgTester *tester)
{
    /* this function puts module request into memory */
    /* even though it looks awful it works - don't break it */
    gchar           *md = (gchar *)mdptr;
    gint            ret = 0;
    SD_ATTR_TYPE    attr_type = EXTRA_ATTR;
    gint            attr_size = 0;

    for (; m && m->name; m++) {
        attr_type = BASIC_ATTR;
        if (!tester)
            attr_type = m->attr_type;
        else { /* copy value from parent (tester) we will overwrite it if requested */
            switch (m->type) {
            case STRING:
                attr_size = m->param;
                break;
            case PORT:
                attr_size = sizeof(u_int16_t);
                break;
            case BOOL:
                attr_size = sizeof(char);
                break;
            default:
                attr_size = sizeof(u_int32_t);
                break;
            }
            memcpy((void *)((gchar *)md + m->offset), (void *)((gchar *)tester->moddata + m->offset), attr_size);
        }
        if (sd_xml_attr(node, (gchar *)m->name, (void *)((gchar *)md + m->offset), m->type, m->param, attr_type,
                "\t[-] 404 - param '%s' not found", m->name))
            ret++;
    }
    LOGDEBUG("sd_parse_mod_args() = %d\n", ret);
    return ret;
}

void free_real(gpointer realptr, gpointer unused) {
    CfgReal *real = realptr;

    if (!real)
        return;
    LOGDEBUG("Freeing real: %s", real->name);

    if (real->moddata && real->tester->mops->m_free)
        real->tester->mops->m_free(real);
/*
    if (real->moddynamic)
        free(real->moddynamic);
    if (real->ssl)
        SSL_free(real->ssl);
    if (real->moddata && real->moddata != real->tester->moddata)
        free(real->moddata);
    free(real);
*/
}

static gint sd_parse_real(CfgVirtual *virt, xmlNode *node) {
    CfgReal *real = g_new0(CfgReal, 1);
    xmlChar *tmp  = NULL;

    memset((void *) real, 0, sizeof(CfgReal));

    real->tester       = virt->tester;
    real->virt         = virt;
    real->rstate       = REAL_ONLINE;
    real->last_rstate  = REAL_ONLINE;
    real->retries_ok   = 0;
    real->retries_fail = 0;
    real->diff         = FALSE;
    real->online       = TRUE;          /* default, overwritten by sd_offline_merge_dump */
    real->last_online  = TRUE;
    parse_error        = FALSE;

    sd_xml_attr(node, "name", real->name, STRING, MAXNAME, BASIC_ATTR, "\t[-] No name specified for real!");
    LOGDEBUG("\t[i] Parsing real: %s",real->name);
    sd_xml_attr(node, "addr", real->addrtxt, STRING, MAXIPTXT, BASIC_ATTR, "\t[-] Real (%s): No addr specified!",real->name);

    if (parse_error) {
        free_real(real, NULL);
        return 1;
    }
    if (!sd_str_to_addr(real->addrtxt, &real->addr)) {
        LOGERROR("\t[-] Real (%s): Invalid addr '%s'", real->name, real->addrtxt);
        free_real(real, NULL);
        return 1;
    }
    else {
        if (IN_MULTICAST(ntohl(real->addr.ipv4))) { /* FIX! This only supports checking for ipv4 multicasts */
            LOGERROR("\t[-] Real (%s): Multicast address not supported '%s'",
                real->name, real->addrtxt);
            free_real(real, NULL);
            return 1;
        }
        
    }
    if (strchr(real->addrtxt, '.'))
        real->ip_v = 4;
    else
        real->ip_v = 6;

    sd_xml_attr(node, "testport", &real->testport, PORT, ntohs(real->tester->testport), EXTRA_ATTR,
        "\t[-] Real (%s): No testport specified-default used", real->name);
    sd_xml_attr(node, "port", &real->port, PORT, 0, BASIC_ATTR,
        "\t[-] Real (%s): No port specified!", real->name);
    sd_xml_attr(node, "weight", &real->ipvs_weight, INT, 1, BASIC_ATTR,
        "\t[-] Real (%s): Invalid or no weight!", real->name);

    real->override_weight = -1;
    real->override_weight_in_percent = FALSE;
    sd_xml_attr(node, "uthresh", &real->ipvs_uthresh, UINT, 0, EXTRA_ATTR,
        "\t[-] Real (%s): No uthresh specified!", real->name);
    sd_xml_attr(node, "lthresh", &real->ipvs_lthresh, UINT, 0, EXTRA_ATTR,
        "\t[-] Real (%s): No lthresh specified!", real->name);

    if (sd_xml_attr(node, "bindaddr", real->bindaddrtxt, STRING, MAXIPTXT, EXTRA_ATTR, NULL)) {
        /* strncpy(real->bindaddrtxt, (gchar *)tmp, MAXIPTXT); */ // is it a bug ?
        if (!inet_aton(real->bindaddrtxt, (struct in_addr *) &real->bindaddr)) {
            LOGERROR("\t[-] Real (%s): Invalid bindaddr='%s'", real->name, real->bindaddrtxt);
            free_real(real, NULL);
            return 1;
        }
    }

    if (parse_error) {
        free_real(real, NULL);
        return 1;
    }

    if ((tmp = xmlGetProp(node, BAD_CAST "rt"))) {
        switch(tmp[0]) {
        case 'd':
            real->ipvs_rt = SD_IPVS_RT_DR;
            break;
        case 'm':
            real->ipvs_rt = SD_IPVS_RT_MASQ;
            break;
        case 't':
            real->ipvs_rt = SD_IPVS_RT_TUN;
            break;
        default:
            LOGERROR("\t[-] Real (%s): unknown rt type '%s'!", real->name, tmp);
            free_real(real, NULL);
            return 1;
        }
    }
    else
        real->ipvs_rt = virt->ipvs_rt;
    if (real->tester && real->tester->ssl)
        real->ssl = sd_SSL_new();

    if (real->tester && real->tester->mops->m_args != NULL) { /* check if real wants to overwrite some attributes */
        real->moddata = real->tester->mops->m_alloc_args();
        if (!real->tester->mops->m_args) {
            LOGERROR("\t[-] (in real) m_alloc_args specified and no m_args struct!");
            free_real(real, NULL);
            return 1;
        }
        if (!sd_parse_mod_args(real->tester->mops->m_args, real->moddata, node, real->tester)) { /* no attributes overwritten by real */
            free(real->moddata); /* free allocated memory */
            real->moddata = real->tester->moddata; /* set real moddata to tester's moddata */
        }
    }

    if (!virt->realArr)
        virt->realArr = g_ptr_array_new();
    g_ptr_array_add(virt->realArr, real);
    return 0;
}

inline void free_tester(CfgTester *t) {
    if (!t)
        return;
    LOGDEBUG("Freeing tester: %s", t->proto);

    if (t->exec)
        free(t->exec);

    if (t->moddata) 
        free(t->moddata);

    free(t);
}

static CfgTester *sd_parse_tester(xmlNode *node) {
    CfgTester *tester;

    if (!node) {
        LOGWARN("\t[-] No tester tag inside virtual!");
        return NULL;
    }

    LOGDEBUG("\t[!] Parsing tester (%s)", node->name);
    if (!(tester = g_new0(CfgTester, 1))) {
        LOGERROR("\t[-] Unable to allocate memory for tester!");
        return NULL;
    }
    memcpy(tester, &CfgDefault, sizeof(CfgTester));

    if (!sd_xml_attr(node, "proto", tester->proto, STRING, MAXPROTO, BASIC_ATTR, "No proto specified")) {
        free_tester(tester);
        return NULL;
    }

    if (!strcasecmp(tester->proto, "no-test")) {
        parse_error = FALSE;
        tester->mops = (mod_operations *)malloc(sizeof(mod_operations));
        memset(tester->mops, 0, sizeof(mod_operations));
        tester->mops->m_test_protocol = SD_PROTO_NO_TEST;
        return tester;
    }

    tester->mops = module_lookup(tester->proto);
    if (!tester->mops) {
        LOGERROR("Tester %s not registered", tester->proto);
        free_tester(tester);
        return NULL;
    }
    else
        LOGDEBUG("\t[-] Tester mod_operations: %p", tester->mops);

    parse_error = FALSE;
    if (tester->mops->m_test_protocol == SD_PROTO_EXEC) {
        tester->exec = malloc(MAXPATHLEN);
        sd_xml_attr(node, "exec", tester->exec, STRING, MAXPATHLEN, BASIC_ATTR, "\t[!] EXEC tester but no 'exec' data");
    }

    sd_xml_attr(node, "testport", &tester->testport, PORT, 0, BASIC_ATTR, "\t[i] No test port specified");
    sd_xml_attr(node, "loopdelay", &tester->loopdelay, UINT, 0, EXTRA_ATTR, "\t[i] No loopdelay");
    sd_xml_attr(node, "timeout", &tester->timeout, UINT, 0, EXTRA_ATTR, "\t[i] No timeout");
    sd_xml_attr(node, "retries2ok", &tester->retries2ok, UINT, 0, EXTRA_ATTR, "\t[i] No retries2ok");
    sd_xml_attr(node, "retries2fail", &tester->retries2fail, UINT, 0, EXTRA_ATTR, "\t[i] No retries2fail");
    sd_xml_attr(node, "remove_on_fail", &tester->remove_on_fail, UINT, 0, EXTRA_ATTR, "\t[i] No remove_on_fail");
    sd_xml_attr(node, "debugcomm", &tester->debugcomm, UINT, 0, EXTRA_ATTR, "\t[i] No debugcomm");
    sd_xml_attr(node, "SSL", &tester->ssl, BOOL, 0, EXTRA_ATTR, "");

    if (parse_error) {
        free_tester(tester);
        return NULL;
    }

    if (tester->mops->m_args) {
        tester->moddata = tester->mops->m_alloc_args();
        if (!tester->mops->m_args) {
            LOGERROR("[-] m_alloc_args specified and no m_args struct!");
            free_tester(tester);
            return NULL;
        }
        sd_parse_mod_args(tester->mops->m_args, tester->moddata, node, NULL);
        if (parse_error) {
            free_tester(tester);
            return NULL;
        }
    }
    LOGDEBUG("\t[!] Tester parsed OK");
    return tester;
}

void free_virtual(gpointer virtptr, gpointer unused) {
    CfgVirtual *virt = virtptr;
    if (!virt)
        return;
    LOGDEBUG("Freeing virtual: %s", virt->name);

    if (virt->realArr) {
        g_ptr_array_foreach(virt->realArr, free_real, NULL);
        g_ptr_array_free(virt->realArr, FALSE);
    }

    free_tester(virt->tester);
    free(virt);
}

static gint sd_parse_virtual(GPtrArray *VCfgArr, xmlNode *node) {
    CfgVirtual  *virt = NULL;
    xmlChar     *tmp = NULL;
    xmlNode     *cur_node;
    gboolean    tester_found = FALSE;

    if (!node) {
        LOGERROR("got null node to parse_virtual");
        return 1;
    }

    if (!(virt = g_new0(CfgVirtual,1))) {
        LOGERROR("[-] Unable to allocate memory for virtual (while parsing)!");
        return 1;
    }

    memset(virt, 0, sizeof(CfgVirtual));

    if (!sd_xml_attr(node, "name", virt->name, STRING, MAXNAME, BASIC_ATTR, "[-] Virtual: no name specified!")) {
        free_virtual(virt, NULL);
        return 1;
    }

    if ((tmp = xmlGetProp(node, BAD_CAST "fwmark")))
        virt->ipvs_fwmark = (u_int32_t) atoi((const char *)tmp);

    parse_error = FALSE;
    if (!virt->ipvs_fwmark) {
        sd_xml_attr(node, "addr", virt->addrtxt, STRING, MAXIPTXT, BASIC_ATTR, "[-] Virtual (%s): no addr specified!", virt->name);
        sd_xml_attr(node, "port", &virt->port, PORT, 0, BASIC_ATTR, "[-] Virtual (%s): no port specified!", virt->name);
        if (parse_error) {
            free_virtual(virt, NULL);
            return 1;
        }
    } 
    else {
        strcpy(virt->addrtxt, "0.0.0.0");
        strcpy(virt->porttxt, "0");
    }

    /* ipvs data */
    if (!(tmp = xmlGetProp(node, BAD_CAST "proto"))) {
        LOGERROR("[-] Virtual (%s): no proto specified!", virt->name);
        free_virtual(virt, NULL);
        return 1;
    }
    switch (tmp[0]) {
    case 't':
        virt->ipvs_proto = SD_IPVS_TCP;
        break;
    case 'u':
        virt->ipvs_proto = SD_IPVS_UDP;
        break;
    case 'f':
        virt->ipvs_proto = SD_IPVS_FWMARK;
        break;
    default:
        LOGERROR("[-] Virtual (%s): unknown protocol '%s'", virt->name, tmp);
        free_virtual(virt, NULL);
        return 1;
    }
    sd_xml_attr(node, "sched", virt->ipvs_sched, STRING, MAXSCHED, BASIC_ATTR,
        "[-] Virtual (%s): no scheduler specified!", virt->name);
    if (parse_error) {
        free_virtual(virt, NULL);
        return 1;
    }
    if (!(tmp = xmlGetProp(node, BAD_CAST "rt"))) {
        LOGERROR("[-] Virtual (%s): no rt specified!", virt->name);
        free_virtual(virt, NULL);
        return 1;
    }

    switch(tmp[0]) {
    case 'd':
        virt->ipvs_rt = SD_IPVS_RT_DR;
        break;
    case 'm':
        virt->ipvs_rt = SD_IPVS_RT_MASQ;
        break;
    case 't':
        virt->ipvs_rt = SD_IPVS_RT_TUN;
        break;
    default:
        LOGERROR("[-] Virtual (%s): unknown rt type '%s'!", virt->name, tmp);
        free_virtual(virt, NULL);
        return 1;
    }
    if ((tmp = xmlGetProp(node, BAD_CAST "pers")))
        virt->ipvs_persistent = tmp[0]-'0' ? 1 : 0;

    if (!virt->ipvs_fwmark) {
        if (!inet_aton(virt->addrtxt, (struct in_addr *) &virt->addr)) {
            LOGERROR("[-] Virtual: addr not correct");
            free_virtual(virt, NULL);
            return 1;
        }
        else {
            if (IN_MULTICAST(ntohl(virt->addr.ipv4))) { /* FIX! this only supports ipv4 multicasts */
                LOGERROR("\t[-] Virtual (%s): Multicast address not supported '%s'",
                    virt->name, virt->addrtxt);
                free_virtual(virt, NULL);
                return 1;
            }
        }
    }

    /* parsing tester and reals */
    for (cur_node = node->children; cur_node; cur_node = cur_node->next)
        if (cur_node->type == XML_ELEMENT_NODE) {
            if (!strcmp((gchar *)cur_node->name, "tester"))
                break;
            else {
                free_virtual(virt, NULL);
                LOGERROR("[-] xml: Unknown tag inside virtual: (%s)", cur_node->name);
                return 1;
            }
        }

    if (!(virt->tester = sd_parse_tester(cur_node))) {
        LOGERROR("[-] Virtual (%s): Unable to parse tester", virt->name);
        free_virtual(virt, NULL);
        return 1;
    }

    for (cur_node = node->children; cur_node; cur_node = cur_node->next)
        if (cur_node->type == XML_ELEMENT_NODE) {
            if (!strcmp((char *) cur_node->name, "tester")) {
                if (tester_found) {
                    LOGERROR("[-] xml: double 'tester' tag!");
                    free_virtual(virt, NULL);
                    return 1;
                }
                tester_found = TRUE;
                continue;
            }

            if (strcmp((char *) cur_node->name, "real")) {
                LOGERROR("[-] xml: Unknown tag inside virtual: (%s)", cur_node->name);
                free_virtual(virt, NULL);
                return 1;
            }
            if (sd_parse_real(virt, cur_node)) {
                LOGERROR("[-] Virtual (%s): error parsing real!", virt->name);
                free_virtual(virt, NULL);
                return 1;
            }
        }

    g_ptr_array_add(VCfgArr, (gpointer)virt);
    virt->state = NOT_READY;

    LOGDEBUG("node: %s registered to tester: %s", virt->name, virt->tester->proto);

    return 0;
}

static int sd_parse_node(GPtrArray *VCfgArr, xmlNode *a_node) {
    xmlNode *cur_node = NULL;

    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) { /* <TAG> */
            LOGDEBUG("node: %s", cur_node->name);
            if (!strcmp((char *)cur_node->name, "virtual")) {
                LOGDEBUG("Parsing virtual node");
                if (sd_parse_virtual(VCfgArr, cur_node))
                    return 1;
            }
            else if (!strcmp((char *)cur_node->name, "surealived")) {
                if (strcmp((char *)a_node->name, "surealived")) {
                    LOGDEBUG("[-] xml: surealived tag inside surealived tag!");
                    return 1;
                }
                LOGDEBUG("Surealived conf start:");
                if (sd_parse_node(VCfgArr, cur_node->children))
                    return 1;
            }
            else {
                LOGERROR("[-] xml: parse_root - unknown tag (%s)", cur_node->name);
                return 1;
            }
        }
    }
    return 0;
}

static GPtrArray *sd_xmlParseDoc(xmlDoc *doc) {    /* parse xml document */
    xmlNode     *root_node = NULL;
    GPtrArray   *VCfgArr = NULL;
    gint        ret;
    if (!VCfgArr)
        VCfgArr = g_ptr_array_new();
    assert(VCfgArr);

    if (!(root_node = xmlDocGetRootElement(doc)))
        LOGERROR("[-] XML error - no root node?!");
    ret = sd_parse_node(VCfgArr, root_node);

    xmlFreeDoc(doc);    /* free used memory */
    xmlCleanupParser(); /* it's better to make sure no leaks are present */
    if (ret) {
        g_ptr_array_foreach(VCfgArr, free_virtual, NULL);
        g_ptr_array_free(VCfgArr, FALSE);
        return NULL;
    }
    return VCfgArr;
}

/* === Public interface === */
GPtrArray *sd_xmlParseFile(gchar *filename) { /* parse xml file */
    xmlDoc      *doc = NULL;
    GPtrArray   *vcfgarr;

    if (!(doc = xmlReadFile(filename, NULL, 0))) {
        LOGERROR("[-] Failed to parse xml config from file");
        /* todo: cleanup */
        return NULL;
    }
    vcfgarr = sd_xmlParseDoc(doc);    
    if (!vcfgarr) 
        return NULL;

    return vcfgarr;
}

gint sd_VCfgArr_free(GPtrArray *VCfgArr) { /* free all memory used by Virtual Array */
    if (!VCfgArr) {
        LOGWARN("sd_VCfgArr_free: NULL array passed as argument!");
        return 1;
    }
    LOGDEBUG("Freeing virtual array");
    g_ptr_array_foreach(VCfgArr, free_virtual, NULL);
    return 0;
}
