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
