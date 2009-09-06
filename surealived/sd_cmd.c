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
#include <modloader.h>
#include <sd_socket.h>
#include <sd_epoll.h>
#include <sd_override.h>
#include <sd_ipvssync.h>
#include <xmlparser.h>

#define MAX_CLIENTS 16

int      listen_sock;
SDepoll *logic_epoll;
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
        g_string_append_printf(s, "%d. vname=%s vproto=%s vaddr=%s vport=%d vfwmark=%d vrt=%s vsched=%s\n",
                               i,
                               virt->name, 
                               sd_proto_str(virt->ipvs_proto),
                               virt->addrtxt, ntohs(virt->port), 
                               virt->ipvs_fwmark,
                               sd_rt_str(virt->ipvs_rt),
                               virt->ipvs_sched);
    }
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
    
    for (i = 0; i < virt->realArr->len; i++) {
        real = (CfgReal *) g_ptr_array_index(virt->realArr, i);
        currwgt = sd_ipvssync_calculate_real_weight(real);
        g_string_append_printf(s, "%d. rname=%s raddr=%s rport=%d currwgt=%d confwgt=%d ronline=%s rstate=%s\n",
                               i+1,
                               real->name, 
                               real->addrtxt, ntohs(real->port),
                               currwgt, real->ipvs_weight, GBOOLSTR(real->online), sd_rstate_str(real->rstate));
    }
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
    if (!rstate)
        rstate = sd_rstate_str(real->rstate); //leave rstate untouched
    else {
        rst = sd_rstate_no(rstate);
        if (rst < 0)
            return g_strdup_printf("cmd_rset: rstate unknown [%s]\n", rstate);
    }

    rweight = g_hash_table_lookup(ht, "rweight");
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

    return g_string_free(s, FALSE);
}

gint sd_cmd_delreal(GPtrArray *VCfgArr, gchar *vip, u_int16_t vport, 
                    SDIPVSProtocol proto, gchar *rip, u_int16_t rport) {
    CfgReal *real = NULL; //sd_cmd_find_real(sd_cmd_find_virt(VCfgArr, vip, vport, proto), rip, rport);

    LOGINFO("[^] logic: deleting real (%s:%d_%d)[%s:%d]", vip, vport, proto, rip, rport);

    if (!real) {
        LOGINFO("\t-> unable to find real!");
        return -1;
    }
    if (real->ssl)
        SSL_free(real->ssl);
    if (real->moddata)
        free(real->moddata);

    g_ptr_array_remove_fast(real->virt->realArr, real); /* super strange */
    free(real);

    return 0;
}

gint sd_cmd_addreal(gchar *xml_info) {
    /* i suppose the best way to add real's/virtuals is to pass complete tree */
    /* in an XML form (ie. <real .... /> ) and user sd_xml_parse_* */
    return 0;
}

gint sd_cmd_addvirt(gchar *xml_info) {

    return 0;
}

gint sd_cmd_delvirt(GPtrArray *VCfgArr, gchar *vip, u_int16_t vport, SDIPVSProtocol proto) {
    gint        i;
    CfgReal     *real;
    CfgVirtual  *virt = NULL; //sd_cmd_find_virt(VCfgArr, vip, vport, proto);

    if (!virt) {
        LOGINFO("[^] logic: unable to find virt %s:%d_%d", vip, vport, proto);
        return 1;
    }
    LOGINFO("[^] logic: deleting virtual %s (%s:%d)", virt->name, vip, vport);
    if (virt->realArr) {
        for (i = 0; i < virt->realArr->len; i++) { /* free all real's */
            real = g_ptr_array_index(virt->realArr, i);
            LOGDEBUG("\tfreeing real: %s", real->name);
            virt->tester->mops->m_free(real); /* free real's memory */
            if (real->ssl)
                SSL_free(real->ssl);
            if (real->moddata)
                free(real->moddata);
            g_ptr_array_remove_fast(virt->realArr, real);
            free(real);
        }
        g_ptr_array_free(virt->realArr, FALSE); /* free real array */
    }
    if (virt->tester) {
        if (virt->tester->moddata) {
            LOGDEBUG("\tfreeing tester");
            free(virt->tester->moddata); /* free module private data */
        }
    }
    free(virt);                 /* free virtual at last */
    return 0;
}

gint sd_cmd_listen_socket_create(u_int16_t lport) {
    struct sockaddr_in  sa;
    SDClient            *server;

    LOGDETAIL("%s()", __PRETTY_FUNCTION__);
    listen_sock = sd_socket_nb(SOCK_STREAM);
    sd_socket_solinger(listen_sock);

    sa.sin_family = PF_INET;
    sa.sin_port = htons(lport);
    sa.sin_addr.s_addr = INADDR_ANY;

    if (!logic_epoll)
        logic_epoll = sd_epoll_new(MAX_CLIENTS+1);

    if (bind(listen_sock, (struct sockaddr *)&sa, sizeof(sa))) {
        LOGERROR("Unable to bind to socket: port %d", lport);
        return -1;
    }

    if (listen(listen_sock, MAX_CLIENTS)) {
        LOGERROR("Unable to listen()");
        return -1;
    }
    server = (SDClient *)malloc(sizeof(SDClient));
    server->fd = listen_sock;
    sd_epoll_ctl(logic_epoll, EPOLL_CTL_ADD, listen_sock, server, EPOLLIN);

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
        LOGINFO("[@] Client disconnected!");
        return -1;
    }
    LOGINFO("[@] New client connected!\n");

    client = (SDClient *)malloc(sizeof(SDClient));
    memset(client, 0, sizeof(SDClient));
    client->fd = client_sock;
    sd_epoll_ctl(logic_epoll, EPOLL_CTL_ADD, client_sock, client, EPOLLIN); /* waiting for read */
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
    else if (!strncmp(cmd, "rset", 4)) {
        client->wbuf = sd_cmd_rset(VCfgArr, ht);        
    }
    else{
        client->wbuf = g_strdup_printf("Invalid request! [%s]\n", cmd);
    }

cleanup:
    client->wlen = strlen(client->wbuf);
    g_hash_table_destroy(ht);
}

static void sd_cmd_process_client(SDClient *client, GPtrArray *VCfgArr) {
    gint    ret;
    gchar   *npos;

    LOGDEBUG("Processing client!");
    if (client->wpos < client->wlen) {      /* that means we have to write something to client */
        ret = write(client->fd, client->wbuf + client->wpos, client->wlen - client->wpos); /* write correct amount */
        if (!ret || (ret < 0 && errno != EAGAIN)) { /* check if we have an incident */
            close(client->fd);
            if (client->wbuf)
                free(client->wbuf);
            free(client);
            connected_clients--;
            return;
        }

        client->wpos += ret;
        if (client->wpos == client->wlen) { /* are we there yet? */
            if (client->retcode == -1) {    /* close connection */
                free(client->wbuf);
                close(client->fd);
                free(client);
                return;
            }
            client->wpos = client->wlen = 0;
            client->retcode = 0;
            sd_epoll_ctl(logic_epoll, EPOLL_CTL_MOD, client->fd, client, EPOLLIN); /* we need to read now */
            return;
        }
        return;
    }
    ret = read(client->fd, client->rbuf + client->rpos, sizeof(client->rbuf) - client->rpos-1);

    if (ret < 0 && errno != EAGAIN)
        return;

    else if (ret == 0) {
        close(client->fd);
        free(client);
        connected_clients--;
        return;
    }

    if (client->rpos + ret == sizeof(client->rbuf)-1) { /* too long request */
        sd_epoll_ctl(logic_epoll, EPOLL_CTL_MOD, client->fd, client, EPOLLOUT);
        if (client->wbuf)
            free(client->wbuf);
        client->wbuf = g_strdup_printf("Your request was too long (max=%d bytes)!", (int)sizeof(client->rbuf));
        client->wlen = strlen(client->wbuf);
        client->rpos = 0;
        client->retcode = -1;   /* close connection */
        return;
    }

    client->rpos += ret;
    if ((npos = strchr(client->rbuf + client->rpos - ret, '\n'))) { /* wee! we've got command */
        *npos = 0;              /* substitute \n with \0 */
        if (npos > &client->rbuf[0] && *(npos-1) == '\r')
            *(--npos) = 0;        /* kill \r */

        npos++;                   /* get next char */
        sd_cmd_execute(client, VCfgArr); /* try to execute command */
        client->rpos = 0;
        while (*npos != '\0')
            client->rbuf[client->rpos++] = *(npos++); /* that looks awful! */

        client->rbuf[client->rpos] = 0;
        sd_epoll_ctl(logic_epoll, EPOLL_CTL_MOD, client->fd, client, EPOLLOUT);
    }
}

void sd_cmd_loop(GPtrArray *VCfgArr) {
    gint        nr_events = 0;
    SDClient    *client;

    nr_events = sd_epoll_wait(logic_epoll, 0);

    if (nr_events < 1)
        return;

    for (; nr_events > 0 ; nr_events--) {
        client = (SDClient *)sd_epoll_event_dataptr(logic_epoll, nr_events-1);
        if (sd_epoll_event_events(logic_epoll, nr_events-1) & EPOLLERR) {
            close(client->fd);
            if (client->wbuf)
                free(client->wbuf);
            free(client);
            connected_clients--;
        }

        LOGDETAIL("event [%d] on clients!", nr_events-1);
        if (client->fd == listen_sock) {
            sd_cmd_accept_client();
            continue;
        }
        sd_cmd_process_client(sd_epoll_event_dataptr(logic_epoll, nr_events-1), VCfgArr);
    }
}
