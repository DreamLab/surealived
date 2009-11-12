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

#include <sd_socket.h>
#include <common.h>
#include <glib.h>
#include <openssl/ssl.h>
#include <sd_maincfg.h>

SSL_CTX *ctx = NULL;

gboolean sd_str_to_addr(char *s, sd_addr *addr) {
    if (strchr(s, '.'))
        return inet_pton(AF_INET, s, &addr->ipv4);
    else    
        return inet_pton(AF_INET6, s, &addr->ipv6);
}

int sd_socket_nb(int socket_type, int ip_version) {
    int fd, val;
    int domain = ip_version == 4 ? AF_INET : AF_INET6;

    switch (socket_type){
    case SOCK_STREAM:
        fd = socket(domain, SOCK_STREAM, 0);
        break;
    case SOCK_DGRAM:
        fd = socket(domain, SOCK_DGRAM, 0);
        break;
    }
    assert (fd >= 0);

    /* non-block */
    val = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, val | O_NONBLOCK);

    return fd;
}

void sd_socket_solinger(int fd) {
    struct linger sl;

    sl.l_onoff = 1;
    sl.l_linger = 0;
    setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *) &sl, sizeof (struct linger));
}

int sd_socket_connect(int fd, sd_addr addr, u_int16_t port, char ip_version) {
    struct sockaddr_in sa4;
    struct sockaddr_in6 sa6;
    int r;
    if (ip_version == 4) {
        sa4.sin_addr.s_addr = addr.ipv4;
        sa4.sin_port = port;
        sa4.sin_family = PF_INET;
        r = connect(fd, (struct sockaddr *) &sa4, sizeof(struct sockaddr_in));
    }
    else if (ip_version == 6) {
        sa6.sin6_addr = addr.ipv6;
        sa6.sin6_family = PF_INET6;
        sa6.sin6_port = port;
        r = connect(fd, (struct sockaddr *) &sa6, sizeof(struct sockaddr_in6));
    }

    if (r < 0 && errno != EINPROGRESS)
        LOGERROR("Can't connect [%s]", strerror(errno));

    return 0;
}

int sd_bind_ssl(CfgReal *real) {
    BIO *sbio;
    if (!real->ssl->rbio && (sbio = BIO_new_socket(real->fd, BIO_NOCLOSE)))
        SSL_set_bio(real->ssl, sbio, sbio);
    else if (real->ssl->rbio) {
        BIO_set_fd(real->ssl->rbio, real->fd, BIO_NOCLOSE);
        BIO_set_fd(real->ssl->wbio, real->fd, BIO_NOCLOSE);
    }
    return 0;
}

void sd_ssl_free(SSL *ssl) {
    SSL_free(ssl);
}

SSL* sd_SSL_new() {
    LOGDEBUG("ssl_new()");
    if (!ctx) {
        SSL_library_init();
        ctx = SSL_CTX_new(SSLv23_method());
    }
    LOGDEBUG("ssl_new()");
    return SSL_new(ctx);
}
