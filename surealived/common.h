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

#define __USE_LARGEFILE64
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <time.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/epoll.h>

#include <sd_defs.h>
#include <sd_maincfg.h>
#include <logging.h>

inline void time_inc(struct timeval *, guint, guint);

#ifndef MAXPATHLEN
#define MAXPATHLEN 512
#endif

gboolean sd_unlink_commlog(CfgReal *real);
gboolean sd_append_to_file(gchar *fname, gchar *buf, gint bufsiz);
gboolean sd_append_to_commlog(CfgReal *real, gchar *buf, gint bufsiz);
gchar *sd_proto_str(SDIPVSProtocol proto);
SDIPVSProtocol sd_proto_no(gchar *str);
gchar *sd_rt_str(SDIPVSRt rt);
gchar *sd_rstate_str(RState st);
RState sd_rstate_no(gchar *str);
GHashTable *sd_parse_line(gchar *line);
