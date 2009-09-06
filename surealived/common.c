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
#include <sd_maincfg.h>

static gchar *protostr[] = { "tcp", "udp", "fwmark", NULL };
static gchar *rtstr[] = { "dr", "masq", "tun", NULL };
static gchar *rstatestr[] = { "ONLINE", "OFFLINE", "DOWN", NULL };

inline void time_inc(struct timeval *t, guint s, guint ms) {
    guint usd;

    usd = t->tv_usec + ms*1000;

    t->tv_sec += s + usd / 1000000;
    t->tv_usec = usd % 1000000;
}

/*! \brief Set the communication log filename for a specified real 
  \param real CfgReal 
  \param fname A buffer at least BUFSIZ length to store a filename
*/
static gchar *sd_commlog_fname(CfgReal *real, gchar *fname) {
    snprintf(fname, BUFSIZ, "%s/%s_%s", G_debug_comm_path, real->virt->name, real->name);
    return fname;
}

/*! \brief Unlink the communication log based on a filename constructed from a \a real
  \param real CfgReal
  \return \a TRUE if unlink succeed, otherwise \a FALSE
*/
gboolean sd_unlink_commlog(CfgReal *real) {
    gchar fname[BUFSIZ];
    sd_commlog_fname(real, fname);
    if (unlink(fname))
        return FALSE;
    return TRUE;
}

/*! \brief Append bufsiz length message from buf to \a fname file 
  \param fname A filename to append to
  \param buf A buffer to write
  \param bufsiz A buffer message len
  \return \a TRUE if \a bufsiz length write succeed, otherwise \a FALSE
*/
gboolean sd_append_to_file(gchar *fname, gchar *buf, gint bufsiz) {
    struct timeval tv;
    gchar buft[32];
    gchar micro[16];
    FILE *out;
    int w;

    out = fopen(fname, "a");
    if (!out)
        LOGERROR("Can't append to file %s", fname);
    assert(out);

    gettimeofday(&tv, NULL);
    strftime(buft, 32, "[%F %T.", localtime(&tv.tv_sec));
    fprintf(out, buft);
    snprintf(micro, 16, "%06d] ", (int) tv.tv_usec);
    fprintf(out, micro);

    w = fwrite(buf, bufsiz, 1, out);
    fprintf(out, "\n");
    fclose(out);

    if (!w)
        return FALSE;

    return TRUE;
}

/*! \brief Append to a real based filename communication log
  \param real CfgReal used to build a filename
  \param buf A buffer to write to the communication log
  \param bufsiz A buffer length
*/
gboolean sd_append_to_commlog(CfgReal *real, gchar *buf, gint bufsiz) {
    gchar fname[BUFSIZ];

    sd_commlog_fname(real, fname);

    return sd_append_to_file(fname, buf, bufsiz);
}

gchar *sd_proto_str(SDIPVSProtocol proto) {
    return protostr[proto];
}

SDIPVSProtocol sd_proto_no(gchar *str) {
    gint i = 0;

    while (1) {
        if (!protostr[i])
            return -1;

        if (!strcmp(protostr[i], str))
            return i;

        i++;
    }
    return -1;
}

gchar *sd_rt_str(SDIPVSRt rt) {
    return rtstr[rt];
}

gchar *sd_rstate_str(RState st) {
    return rstatestr[st];
}

RState sd_rstate_no(gchar *str) {
    gint i = 0;

    while (1) {
        if (!rstatestr[i])
            return -1;

        if (!strcmp(rstatestr[i], str))
            return i;

        i++;
    }
    return -1;
}


/* ---------------------------------------------------------------------- */
/* Split line into hash table, returns GHashTable */
GHashTable *sd_parse_line(gchar *line) {
    GHashTable *ht = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_assert(ht);

    gchar **strv = g_strsplit_set(line, " \t", 0);
    gchar **v;
	
    v = strv;
    while ( *v ) {
        gchar **kvstrv = g_strsplit_set(*v, "=", 0);
        if (kvstrv[0] && kvstrv[1]) {
            LOGDETAIL("* key = %s, val = %s", kvstrv[0], kvstrv[1]);
            g_hash_table_insert(ht, g_strdup(kvstrv[0]), g_strdup(kvstrv[1]));
        }
        g_strfreev(kvstrv);
        v++;
    }

    g_strfreev(strv);

    return ht;
}
