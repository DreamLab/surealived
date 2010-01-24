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

#if !defined __SD_SOCKET_H
#define __SD_SOCKET_H

#include "common.h"
#include <openssl/ssl.h>

int  sd_socket_nb(int sock_type);
void sd_socket_solinger(int fd);

int  sd_socket_connect(int fd, in_addr_t addr, u_int16_t port);
int  sd_bind_ssl(CfgReal *real);
void sd_ssl_free(SSL *ssl);
SSL *sd_SSL_new();

#endif
