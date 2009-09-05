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

static CfgVirtual *sd_cmd_find_virt(GPtrArray *VCfgArr, gchar *vip, u_int16_t vport, SDIPVSProtocol proto) {
    CfgVirtual  *virt = NULL;
    gint        i;
    gboolean    found = FALSE;
    in_addr_t   vaddr = inet_addr(vip);
    u_int16_t   vport_no = ntohs(vport);

    if (vaddr == -1) {
        LOGINFO("[^] logic: find_virt() invalid IP");
        return NULL;
    }

    for (i = 0; i < VCfgArr->len; i++) {
        virt = (CfgVirtual *) g_ptr_array_index(VCfgArr, i);
        if (virt->addr != vaddr || virt->port != vport_no || virt->ipvs_proto != proto)
            continue;
        found = TRUE;
        break;
    }
    if (!found)
        return NULL;

    return virt;
}

static CfgReal *sd_cmd_find_real(CfgVirtual *virt, gchar *rip, u_int16_t rport) {
    CfgReal     *real = NULL;
    gint        i;
    gboolean    found = FALSE;
    in_addr_t   raddr = inet_addr(rip);
    u_int16_t   rport_no = htons(rport);

    if (!virt || !virt->realArr)
        return NULL;

    if (raddr == -1) {
        LOGINFO("[^] find_real() invalid IP");
        return NULL;
    }

    for (i = 0; i < virt->realArr->len; i++) {
        real = (CfgReal *)g_ptr_array_index(virt->realArr, i);
        if (real->addr != raddr || real->port != rport_no)
            continue;
        found = TRUE;
        break;
    }
    if (!found)
        return NULL;

    return real;
}

gint sd_cmd_chweight(GPtrArray *VCfgArr, gchar *vip, u_int16_t vport, 
                     SDIPVSProtocol proto, gchar *rip, u_int16_t rport, gint weight) {
    CfgReal *real = sd_cmd_find_real(sd_cmd_find_virt(VCfgArr, vip, vport, proto), rip, rport);

    if (!real) {
        LOGINFO("[^] logic: chweight - real (%s:%d_%d)[%s:%d] not found", vip, vport, proto, rip, rport);
        return -1;
    }

    real->ipvs_weight = weight;
    real->diff = TRUE;
    LOGINFO("[^] logic: weight changed (%s:%d)[%s:%d] to '%d'", vip, vport, rip, rport, weight);

    return 0;
}

gint sd_cmd_delreal(GPtrArray *VCfgArr, gchar *vip, u_int16_t vport, 
                    SDIPVSProtocol proto, gchar *rip, u_int16_t rport) {
    CfgReal *real = sd_cmd_find_real(sd_cmd_find_virt(VCfgArr, vip, vport, proto), rip, rport);

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
    CfgVirtual  *virt = sd_cmd_find_virt(VCfgArr, vip, vport, proto);

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
        LOGWARN("[@] Client disconnected!");
        return -1;
    }
    LOGERROR("[@] New client connected!\n");

    client = (SDClient *)malloc(sizeof(SDClient));
    memset(client, 0, sizeof(SDClient));
    client->fd = client_sock;
    sd_epoll_ctl(logic_epoll, EPOLL_CTL_ADD, client_sock, client, EPOLLIN); /* waiting for read */
    connected_clients++;        /* increase counter */

    return 0;
}

static void sd_cmd_execute(SDClient *client, GPtrArray *VCfgArr) {
    gchar *cmd;

    if (!client)
        return;

    cmd = client->rbuf;

    if (client->wbuf) {
        free(client->wbuf);
        client->wbuf = NULL;
    }

    if (!strncmp(cmd, "roffline", 8)) {
        client->wbuf = g_strdup_printf("OFFLINING!\n");
        client->wlen = strlen(client->wbuf);
    }
    else if (!strncmp(cmd, "ronline",7)) {
        client->wbuf = g_strdup_printf("ONLINING!\n");
        client->wlen = strlen(client->wbuf);
    }
    else{
        client->wbuf = g_strdup_printf("Invalid request!\n");
        client->wlen = strlen(client->wbuf);
    }
}

static void sd_cmd_process_client(SDClient *client, GPtrArray *VCfgArr) {
    gint    ret;
    gchar   *npos;

    LOGINFO("Processing client!");
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
        *npos = 0;              /* substitude \n with \0 */
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

    for (; nr_events>0 ; nr_events--) {
        client = (SDClient *)sd_epoll_event_dataptr(logic_epoll, nr_events-1);
        if (sd_epoll_event_events(logic_epoll, nr_events-1) & EPOLLERR) {
            close(client->fd);
            if (client->wbuf)
                free(client->wbuf);
            free(client);
            connected_clients--;
        }

        LOGERROR("event [%d] on clients!", nr_events-1);
        if (client->fd == listen_sock) {
            sd_cmd_accept_client();
            continue;
        }
        sd_cmd_process_client(sd_epoll_event_dataptr(logic_epoll, nr_events-1), VCfgArr);
    }
}
