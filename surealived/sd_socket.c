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

#include <sd_socket.h>
#include <common.h>
#include <glib.h>
#include <openssl/ssl.h>
#include <sd_maincfg.h>

SSL_CTX *ctx = NULL;

int sd_socket_nb(int socket_type) {
    int fd, val;
    switch (socket_type){
    case SOCK_STREAM:
        fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        break;
    case SOCK_DGRAM:
        fd = socket(PF_INET, SOCK_DGRAM, 0);
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

int sd_socket_connect(int fd, in_addr_t addr, u_int16_t port) {
    struct sockaddr_in sa;
    int r;

    sa.sin_addr.s_addr = addr;
    sa.sin_port = port;
    sa.sin_family = PF_INET;

    r = connect(fd, (struct sockaddr *) &sa, sizeof(struct sockaddr_in));
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
