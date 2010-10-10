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

#include <common.h>
#include <config.h>
#include <ipvsfuncs.h>
#include <maincfg.h>
#include <logging.h>

typedef enum {
    CONTEXT_META,
    CONTEXT_VIRTUAL,
    CONTEXT_REAL,
    CONTEXT_DIFF,
} ParseContext;

static ConfVirtual *vptr = NULL;

#define PRINT_SVC fprintf(stderr, "Virtual: [%s], ip:[%08x], port:[%d], is_in_ipvs:[%d]\n", \
                          cv->vname, ntohl(cv->svc.addr), ntohs(cv->svc.port), cv->is_in_ipvs);

#define PRINT_DEST fprintf(f, " * Real: [%s], ip:[%08x], port:[%d], is_in_ipvs:[%d]\n", \
                           cr->rname, ntohl(cr->dest.addr), ntohs(cr->dest.port), cr->is_in_ipvs);

/* ---------------------------------------------------------------------- */
/* Split line from config file into hash table */
GHashTable *config_parse_line(gchar *line) {
    GHashTable *ht = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_assert(ht);

    gchar **strv = g_strsplit_set(line, " \t", 0);
    gchar **v;
	
    v = strv;
    while ( *v ) {
        //fprintf(stderr, "elem: %p %s\n", *v, *v);
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

/* ---------------------------------------------------------------------- */
gint config_add_virtual(Config *c, GHashTable *ht) {
    ConfVirtual    *cv;
    ipvs_service_t  tmpsvc;
    gchar          *vname;
    gchar          *vrt;
    unsigned        conn_flags = IP_VS_CONN_F_DROUTE;
	
    /* Verify rt */
    vrt = g_hash_table_lookup(ht, "vrt");
    if (!vrt) {
        LOGERROR("No routing type set");
        return -1;
    }
    if (!strcmp(vrt, "dr")) 
        conn_flags = IP_VS_CONN_F_DROUTE;
    else if (!strcmp(vrt, "masq")) 
        conn_flags = IP_VS_CONN_F_MASQ;
    else if (!strcmp(vrt, "tun")) 
        conn_flags = IP_VS_CONN_F_TUNNEL;
    else {
        LOGERROR("Unknown routing type [%s]", vrt);
        return -1;
    }

    if (ipvsfuncs_set_svc_from_ht(&tmpsvc, ht) == FALSE) {
        LOGERROR("Error parsing virtual");
        return -1;
    }

    cv = g_new0(ConfVirtual, 1);
    g_assert(cv);
    vptr = cv;
    cv->realarr = g_array_new(FALSE, FALSE, sizeof(ConfReal *));

    /* Set a name if defined */
    vname = g_hash_table_lookup(ht, "vname");
    if (vname) 
        strncpy(cv->vname, vname, MAXSIZE);
    else 
        strcpy(cv->vname, "not-defined");

    cv->conn_flags = conn_flags;
    cv->is_in_ipvs = FALSE;
    cv->svc = tmpsvc;
					  
    g_array_append_val(c->virtarr, cv);

    return 0;
}

/* ---------------------------------------------------------------------- */
gint config_add_real(Config *c, ConfVirtual *currcv, GHashTable *ht) {
    ConfReal    *cr;
    ipvs_dest_t  tmpdest;
    gchar       *rname;

    if (ipvsfuncs_set_dest_from_ht(&tmpdest, ht, currcv->conn_flags) == FALSE) {
        LOGERROR("Error parsing real");
        return -1;
    }

    cr = g_new0(ConfReal, 1);
    g_assert(cr);

    /* Set a name if defined */
    rname = g_hash_table_lookup(ht, "rname");
    if (rname) 
        strncpy(cr->rname, rname, MAXSIZE);
    else 
        strcpy(cr->rname, "not-defined");	

    cr->is_in_ipvs = FALSE;
    cr->dest = tmpdest;

    g_array_append_val(currcv->realarr, cr);
    return 0;
}

/* ---------------------------------------------------------------------- */
static gint _virtarr_cmp(gconstpointer p1, gconstpointer p2) {
    ConfVirtual **v1 = (ConfVirtual **) p1;
    ConfVirtual **v2 = (ConfVirtual **) p2;

    if ((*v1)->svc.fwmark > 0 && (*v2)->svc.fwmark > 0)
        return (*v1)->svc.fwmark - (*v2)->svc.fwmark;

    if (ntohl((*v1)->svc.addr) - ntohl((*v2)->svc.addr))
        return ntohl((*v1)->svc.addr) - ntohl((*v2)->svc.addr);

    if (ntohs((*v1)->svc.port) - ntohs((*v2)->svc.port))
        return ntohs((*v1)->svc.port) - ntohs((*v2)->svc.port);

    return (*v1)->svc.protocol - (*v2)->svc.protocol;
}

/* ---------------------------------------------------------------------- */
static gint _realarr_cmp(gconstpointer p1, gconstpointer p2) {
    ConfReal **r1 = (ConfReal **) p1;
    ConfReal **r2 = (ConfReal **) p2;

    if (ntohl((*r1)->dest.addr) - ntohl((*r2)->dest.addr)) 
        return ntohl((*r1)->dest.addr) - ntohl((*r2)->dest.addr);

    return ntohs((*r1)->dest.port) - ntohs((*r2)->dest.port);
}

/* ---------------------------------------------------------------------- */
gint config_diff_set(Config *c, GHashTable *ht) {
    /* Verify file */
    if (c->diff_filename)
        free(c->diff_filename);

    c->diff_filename = g_strdup(g_hash_table_lookup(ht, "file"));
    if (!c->diff_filename) {
        LOGERROR("No diff file set");
        return -1;
    }

    if (c->diff_file)
        fclose(c->diff_file);

    if (!(c->diff_file = fopen(c->diff_filename, "r"))) {
        LOGERROR("Can't open diff file [%s]", c->diff_file);
        return -1;
    }

    return 0;
}

/* ---------------------------------------------------------------------- */
static gint _config_parse(ParseContext context, Config *c, gchar *line) {
    GHashTable *ht;
    gint ret;

    if (context == CONTEXT_META) {
        LOGDETAIL("### context: META");
        c->meta = config_parse_line(line);
        c->virtarr = g_array_new(FALSE, FALSE, sizeof(ConfVirtual *));
    } 

    else if (context == CONTEXT_VIRTUAL) {
        LOGDETAIL("### context: VIRTUAL");
        ht = config_parse_line( g_strstrip(line) );
        ret = config_add_virtual(c, ht); //function set vptr to current ConfVirtual
        g_hash_table_destroy(ht);
        return ret;		
	}

    else if (context == CONTEXT_REAL) {
        LOGDETAIL("### context: REAL");
        ht = config_parse_line( g_strstrip(line) );
        ret = config_add_real(c, vptr, ht); //vptr (see above)
        g_hash_table_destroy(ht);
        return ret;		
    }

    else if (context == CONTEXT_DIFF) {
        LOGDETAIL("### context: DIFF");
        ht = config_parse_line( g_strstrip(line) );
        ret = config_diff_set(c, ht); //vptr (see above)
        g_hash_table_destroy(ht);
        return ret;		
    }

    return 0;
}

/* ---------------------------------------------------------------------- */
/* ---------------------------------------------------------------------- */
Config *config_full_read(gchar *fname) {
    FILE *in;
    Config *c;
    gchar line[BUFSIZ];
    gint lineno, ret, i;

    in = fopen(fname, "r");
    if (!in) {
        LOGERROR("Can't open full configuration [%s]", fname);
        return NULL;
    }

    c = g_new0(Config, 1);
    g_assert(c);
	
    lineno = 1;
    while (fgets(line, BUFSIZ, in)) {
        g_strstrip(line);
        LOGDETAIL("line: [%s]", line);
        if (!strncmp(line, "meta", 4))
            ret = _config_parse(CONTEXT_META, c, line);
        else if (!strncmp(line, "virtual", 7))
            ret = _config_parse(CONTEXT_VIRTUAL, c, line);
        else if (!strncmp(line, "real", 4))
            ret = _config_parse(CONTEXT_REAL, c, line);
        else if (!strncmp(line, "DIFF", 4))
            ret = _config_parse(CONTEXT_DIFF, c, line);

        /* Error parsing config file, clean up the mess */
        if (ret) {
            LOGERROR("Error parsing configuration file, line nr = %d", lineno);
            LOGERROR("* %s", line);
            config_full_free(c);
            fclose(in);
            return NULL;
        }
        lineno++;
    }
    fclose(in);

    /* Sort realarr and virtarr */
    for (i = 0; i < c->virtarr->len; i++) {
        ConfVirtual *cv = g_array_index(c->virtarr, ConfVirtual *, i);
        g_array_sort(cv->realarr, _realarr_cmp);
    }
    g_array_sort(c->virtarr, _virtarr_cmp);

    c->last_diffs_cleanup.tv_sec = c->last_diffs_cleanup.tv_usec = 0;

    return c;
}

/* ---------------------------------------------------------------------- */
void config_full_free(Config *conf) {
    gint i, j;

    for (i = 0; i < conf->virtarr->len; i++) {
        ConfVirtual *cv = g_array_index(conf->virtarr, ConfVirtual *, i);
        for (j = 0; j < cv->realarr->len; j++) {
            ConfReal *cr = g_array_index(cv->realarr, ConfReal *, j);
            g_free(cr);
        }
        g_array_free(cv->realarr, TRUE);
        g_free(cv);
    }
    g_array_free(conf->virtarr, TRUE);
    g_hash_table_destroy(conf->meta);

    if (conf->diff_file)
        fclose(conf->diff_file);
    g_free(conf->diff_filename);
    g_free(conf);

    return;
}

/* ---------------------------------------------------------------------- */
static void config_ipvs_del_unmanaged_virtuals(Config *conf) {
    ipvs_service_t **svctab;
    gint *is_in_ipvs;
    gint i;

    svctab = g_new(ipvs_service_t *, conf->virtarr->len + 1);
    is_in_ipvs = g_new0(gint, conf->virtarr->len);
    for (i = 0; i < conf->virtarr->len; i++) {
        ConfVirtual *cv = g_array_index(conf->virtarr, ConfVirtual *, i);
        svctab[i] = &cv->svc;
    }
    svctab[i] = NULL;
    ipvsfuncs_del_unmanaged_services(svctab, is_in_ipvs);
    g_free(svctab);
	
    /* Update virtual status */
    for (i = 0; i < conf->virtarr->len; i++) {
        ConfVirtual *cv = g_array_index(conf->virtarr, ConfVirtual *, i);
        if (is_in_ipvs[i])
            cv->is_in_ipvs = TRUE;
    }
    g_free(is_in_ipvs);
}

/* ---------------------------------------------------------------------- */
static void config_ipvs_update_virtuals(Config *conf) {
    gint i;

    for (i = 0; i < conf->virtarr->len; i++) {
        ConfVirtual *cv = g_array_index(conf->virtarr, ConfVirtual *, i);
        if (cv->is_in_ipvs) 
            ipvsfuncs_update_service(&cv->svc);
        else
            if (!ipvsfuncs_add_service(&cv->svc)) 
                cv->is_in_ipvs = TRUE;
    }
}

/* ---------------------------------------------------------------------- */
static void config_ipvs_update_reals(Config *conf) {
    ipvs_dest_t **desttab;
    gint *is_in_ipvs;
    gint i, j;
	
    for (i = 0; i < conf->virtarr->len; i++) {
        ConfVirtual *cv = g_array_index(conf->virtarr, ConfVirtual *, i);

        desttab = g_new(ipvs_dest_t *, cv->realarr->len + 1);
        is_in_ipvs = g_new0(gint, cv->realarr->len);
        for (j = 0; j < cv->realarr->len; j++) {		
            ConfReal *cr = g_array_index(cv->realarr, ConfReal *, j);
            desttab[j] = &cr->dest;
        }
        desttab[j] = NULL;

        ipvsfuncs_del_unmanaged_dests(&cv->svc, desttab, is_in_ipvs);
        g_free(desttab);

        /* Update reals status */
        for (j = 0; j < cv->realarr->len; j++) {
            ConfReal *cr = g_array_index(cv->realarr, ConfReal *, j);
            if (is_in_ipvs[j])
                cr->is_in_ipvs = TRUE;
        }
        g_free(is_in_ipvs);
	}

    /* Update or add reals */
    for (i = 0; i < conf->virtarr->len; i++) {
        ConfVirtual *cv = g_array_index(conf->virtarr, ConfVirtual *, i);

        for (j = 0; j < cv->realarr->len; j++) {		
            ConfReal *cr = g_array_index(cv->realarr, ConfReal *, j);

            if (cr->is_in_ipvs)
                ipvsfuncs_update_dest(&cv->svc, &cr->dest);
            else
                if (!ipvsfuncs_add_dest(&cv->svc, &cr->dest)) 
                    cr->is_in_ipvs = TRUE; 			
        }
    }	
}

/* ---------------------------------------------------------------------- */
gint config_synchronize(Config *conf, gboolean flush, gboolean del_unmanaged_virtuals) {
	gint i, j;
	
    /* Flush - clean ip_vs table */
    if (flush) {
        ipvs_flush();

        for (i = 0; i < conf->virtarr->len; i++) {
            ConfVirtual *cv = g_array_index(conf->virtarr, ConfVirtual *, i);
            if (!ipvsfuncs_add_service(&cv->svc))
                cv->is_in_ipvs = TRUE;

            for (j = cv->realarr->len - 1; j >= 0; j--) {
                ConfReal *cr = g_array_index(cv->realarr, ConfReal *, j);
                if (!ipvsfuncs_add_dest(&cv->svc, &cr->dest))
                    cr->is_in_ipvs = TRUE;
            }
        }
    }
    /* Synchronize everything */
    else {
        if (del_unmanaged_virtuals)
            config_ipvs_del_unmanaged_virtuals(conf);

        config_ipvs_update_virtuals(conf); //add and update virtuals
        config_ipvs_update_reals(conf);    //remove unmanaged and add absent reals
    }

    ipvs_close();
    ipvs_init();
    return 0;
}

/* ---------------------------------------------------------------------- */
ConfVirtual *config_find_virtual(Config *c, ipvs_service_t *svc, gint *cvindex) {
    ConfVirtual    *cv;
    ipvs_service_t *confsvc;
    gint            i;

    LOGDETAIL("find virtual = %s:%d", INETTXTADDR(svc->addr), ntohs(svc->port));

    for (i = 0; i < c->virtarr->len; i++) {
        cv = g_array_index(c->virtarr, ConfVirtual *, i);
        confsvc = &cv->svc;

        /* first try - fwmark equal */
        if (svc->fwmark > 0 && svc->fwmark == confsvc->fwmark) {
            LOGDETAIL("config find virtual: fwmark equal [fwmark = %d]", svc->fwmark);
            if (cvindex)
                *cvindex = i;
            return cv;
        }

        /* second try */
        if (svc->addr == confsvc->addr && 
            svc->port == confsvc->port &&
            svc->protocol == confsvc->protocol) {
            LOGDETAIL("config find virtual: addr:port:protocol equal [%s:%d:%d]", 
                      INETTXTADDR(svc->addr), ntohs(svc->port), ntohs(svc->protocol));
            return cv;        
        }
    }

    LOGDETAIL("config find virtual: addr:port not found [%s:%d]", 
              INETTXTADDR(svc->addr), ntohs(svc->port));

    return FALSE;
}

/* ---------------------------------------------------------------------- */
ConfReal *config_find_real(ConfVirtual *cv, ipvs_dest_t *dest, gint *crindex) {
    ConfReal       *cr;
    ipvs_dest_t    *confdest;
    gint            j;

    LOGDETAIL("find real, virtual = %s:%d", INETTXTADDR(cv->svc.addr), ntohs(cv->svc.port));

    for (j = cv->realarr->len - 1; j >= 0; j--) { 
        cr = g_array_index(cv->realarr, ConfReal *, j);
        confdest = &cr->dest;

        if (dest->addr == confdest->addr && dest->port == confdest->port) {
            LOGDETAIL("config find real: addr:port:proto equal [%s:%d]",
                      INETTXTADDR(dest->addr), ntohs(dest->port));
            if (crindex)
                *crindex = j;
            return cr;
        }
    }
    LOGDETAIL("config find real: addr:port not found [%s:%d]", 
              INETTXTADDR(dest->addr), ntohs(dest->port));

    return FALSE;
}
/* ---------------------------------------------------------------------- */
gboolean config_remove_real_by_index(ConfVirtual *cv, gint crindex) {
    ConfReal *cr = g_array_index(cv->realarr, ConfReal *, crindex);
    if (!cr)
        return FALSE;

    g_free(cr);
    g_array_remove_index(cv->realarr, crindex);
    
    return TRUE;
}

/* ---------------------------------------------------------------------- */
gint config_fprintf(FILE *f, Config *conf) {
    gint i, j;

    fprintf(f, "=== config ===\n");
    for (i = 0; i < conf->virtarr->len; i++) {
        ConfVirtual *cv = g_array_index(conf->virtarr, ConfVirtual *, i);
        PRINT_SVC;

        for (j = 0; j < cv->realarr->len; j++) {
            ConfReal *cr = g_array_index(cv->realarr, ConfReal *, j);
            PRINT_DEST;
        }
    }

    return 0;
}
