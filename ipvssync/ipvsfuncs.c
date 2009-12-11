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
#include <ipvsfuncs.h>
#include <maincfg.h>
#include <logging.h>

/* ------------------------------------------------------------ */
int ipvsfuncs_modprobe_ipvs(void)
{
	char *argv[] = { "/sbin/modprobe", "--", "ip_vs", NULL };
	int child;
	int status;
	int rc;

	if (!(child = fork())) {
		execv(argv[0], argv);
		exit(1);
	}

	rc = waitpid(child, &status, 0);

	if (!WIFEXITED(status) || WEXITSTATUS(status)) {
		return 1;
	}

	return 0;
}

/* ------------------------------------------------------------ */
void ipvsfuncs_initialize(void) {
    int ret;

	ret = ipvs_init();
	if (ret) {
		LOGERROR("%s\n", ipvs_strerror(errno));
		if (ipvsfuncs_modprobe_ipvs() || ipvs_init()) {
			LOGERROR("Can't initialize ipvs [%s]", ipvs_strerror(errno));
			exit(1);
		}
	}
    
	ret = ipvs_getinfo();
	if (ret) {
		LOGINFO("%s", ipvs_strerror(errno));
        exit(1);
	} else {
		LOGINFO("ipvs version: %d", ipvs_version());
	}
}

/* ------------------------------------------------------------ */
void ipvsfuncs_reinitialize(void) {
    int ret;

    ipvs_close();
	ret = ipvs_init();
	if (ret) {
		LOGERROR("%s\n", ipvs_strerror(errno));
		if (ipvsfuncs_modprobe_ipvs() || ipvs_init()) {
			LOGERROR("Can't initialize ipvs [%s]", ipvs_strerror(errno));
			exit(1);
		}
	}
}

/* ------------------------------------------------------------ */
ipvs_service_t *ipvsfuncs_set_svc(u_int16_t protocol, char *taddr, char *tport,
								  u_int32_t fwmark, char *sched_name, unsigned  flags,
								  unsigned  timeout, __be32 netmask, ipvs_service_t *svc)
{	
	memset(svc, 0, sizeof(ipvs_service_t));
	
	svc->protocol = protocol;
	inet_pton(AF_INET, (const char *) taddr, &svc->addr);	
	svc->port = htons(atoi(tport));
	svc->fwmark   = fwmark;
	strncpy(svc->sched_name, sched_name, strlen(sched_name));
	svc->flags    = flags;
	svc->timeout  = timeout;
	svc->netmask  = netmask;
	
	return svc;
}

/* ------------------------------------------------------------ */
ipvs_dest_t *ipvsfuncs_set_dest(char *taddr, char *tport, 
								unsigned conn_flags, int weight, 
								u_int32_t u_threshold, u_int32_t l_threshold, 
								ipvs_dest_t *dest) {
	
   
	memset(dest, 0, sizeof(ipvs_dest_t));
	
	inet_pton(AF_INET, (const char *) taddr, &dest->addr);	
	dest->port = htons(atoi(tport));
	dest->conn_flags  = conn_flags;
	dest->weight      = weight;
	dest->u_threshold = u_threshold;
	dest->l_threshold = l_threshold;
	
	return dest;
}

/* ------------------------------------------------------------ */
int ipvsfuncs_add_service(ipvs_service_t *svc) {
	int ret;

    LOGDEBUG("add_service: [%s:%d proto=%d fwmark=%d]", INETTXTADDR(svc->addr), 
             ntohs(svc->port), svc->protocol, svc->fwmark);

	ret = ipvs_add_service(svc);

	if (ret) {
		LOGDEBUG("ERROR add_service: [%s:%d proto=%d fwmark=%d] [%s]", INETTXTADDR(svc->addr), 
                 ntohs(svc->port), svc->protocol, svc->fwmark, ipvs_strerror(errno));
		return -1;
	}

	return 0;
}

/* ------------------------------------------------------------ */
int ipvsfuncs_del_service(ipvs_service_t *svc) {
	int ret;

    LOGDEBUG("del_service: [%s:%d proto=%d fwmark=%d]", INETTXTADDR(svc->addr), 
             ntohs(svc->port), svc->protocol, svc->fwmark);

	ret = ipvs_del_service(svc);
	if (ret) {
		LOGDEBUG("ERROR del_service: [%s:%d proto=%d fwmark=%d] [%s]", INETTXTADDR(svc->addr), 
                 ntohs(svc->port), svc->protocol, svc->fwmark, ipvs_strerror(errno));
		return -1;
	}

	return 0;
}

/* ------------------------------------------------------------ */
int ipvsfuncs_update_service(ipvs_service_t *svc) {
	int ret;

    LOGDEBUG("update_service: [%s:%d proto=%d fwmark=%d]", INETTXTADDR(svc->addr), 
             ntohs(svc->port), svc->protocol, svc->fwmark);

	ret = ipvs_update_service(svc);
	if (ret) {
		LOGDEBUG("ERROR update_service: [%s:%d proto=%d fwmark=%d] [%s]", INETTXTADDR(svc->addr), 
                 ntohs(svc->port), svc->protocol, svc->fwmark, ipvs_strerror(errno));
		return -1;
	}

	return 0;
}


/* ------------------------------------------------------------ */
int ipvsfuncs_add_dest(ipvs_service_t *svc, ipvs_dest_t *dest) {
	int ret;

    gchar *s1 = g_strdup_printf("%s:%d proto=%d fwmark=%d", INETTXTADDR(svc->addr), ntohs(svc->port),
                                svc->protocol, svc->fwmark);
    gchar *s2 = g_strdup_printf("%s:%d proto=%d fwmark=%d", INETTXTADDR(dest->addr), ntohs(dest->port),
                                svc->protocol, svc->fwmark);

    LOGDEBUG(" * add_dest: [%s :: %s]", s1, s2);

	ret = ipvs_add_dest(svc, dest);
	if (ret) {
        LOGDEBUG(" * ERROR add_dest: [%s :: %s] [%s]", s1, s2, ipvs_strerror(errno));
        free(s1); 
        free(s2);
		return -1;
	}
    free(s1); 
    free(s2);

	return 0;
}

/* ------------------------------------------------------------ */
int ipvsfuncs_del_dest(ipvs_service_t *svc, ipvs_dest_t *dest) {
	int ret;

    gchar *s1 = g_strdup_printf("%s:%d proto=%d fwmark=%d", INETTXTADDR(svc->addr), ntohs(svc->port),
                                svc->protocol, svc->fwmark);
    gchar *s2 = g_strdup_printf("%s:%d proto=%d fwmark=%d", INETTXTADDR(dest->addr), ntohs(dest->port),
                                svc->protocol, svc->fwmark);

    LOGDEBUG(" * del_dest: [%s :: %s]", s1, s2);

	ret = ipvs_del_dest(svc, dest);
	if (ret) {
        LOGDEBUG(" * ERROR del_dest: [%s :: %s] [%s]", s1, s2, ipvs_strerror(errno));
        free(s1); 
        free(s2);
		return -1;
	}
    free(s1); 
    free(s2);

	return 0;
}

/* ------------------------------------------------------------ */
int ipvsfuncs_update_dest(ipvs_service_t *svc, ipvs_dest_t *dest) {
	int ret;

    gchar *s1 = g_strdup_printf("%s:%d proto=%d fwmark=%d", INETTXTADDR(svc->addr), ntohs(svc->port),
                                svc->protocol, svc->fwmark);
    gchar *s2 = g_strdup_printf("%s:%d proto=%d fwmark=%d", INETTXTADDR(dest->addr), ntohs(dest->port),
                                svc->protocol, svc->fwmark);

    LOGDEBUG(" * update_dest: [%s :: %s]", s1, s2);

	ret = ipvs_update_dest(svc, dest);
	if (ret) {
        LOGDEBUG(" * ERROR update_dest: [%s :: %s] [%s]", s1, s2, ipvs_strerror(errno));
        free(s1); 
        free(s2);
		return -1;
	}
    free(s1); 
    free(s2);

	return 0;
}

/* ------------------------------------------------------------ */
int ipvsfuncs_fprintf_services(FILE *f) {
	int i;

	struct ip_vs_service_entry *se;
	struct ip_vs_get_services *vs = ipvs_get_services();
	for (i = 0; i < vs->num_services; i++) {
		se = &vs->entrytable[i];
		fprintf(f, "service [%d], ip=%x, port=%d\n", 
				i, ntohl(se->addr), ntohs(se->port));
	}
	free(vs);
	
	return 0;
}

/* ------------------------------------------------------------ */
/* args:
   - managed_svc - pointer to svc table
   - is_in_ipvs  - function stores status is a virtual defined in ipvs 
                   (status is saved here only if (is_in_ipvs != NULL)
*/
int ipvsfuncs_del_unmanaged_services(ipvs_service_t **managed_svc, gint *is_in_ipvs) {
	int i, idx, found;

	ipvs_service_t **m;
	struct ip_vs_service_entry *se;
	struct ip_vs_get_services *vs;

    ipvsfuncs_reinitialize(); //if someone clears IPVS ipvs_get_services returns vs table which contains crap inside
    vs = ipvs_get_services();
    if (!vs) 
        return -1;

    LOGDETAIL("vs = %x, vs->num_services = %d", vs, vs->num_services);

	for (i = 0; i < vs->num_services; i++) {
		se = &vs->entrytable[i];
        LOGDETAIL("del_umanaged_services check: [%d], ip=%x, port=%d, protocol=%d, fwmark=%d", 
				i, ntohl(se->addr), ntohs(se->port), se->protocol, se->fwmark);
		m = managed_svc;
		found = 0;
		idx = 0;
		while (*m) {
            LOGDETAIL(" * compare to [%x:%d proto=%d fwmark=%d]",
                      ntohl((*m)->addr), ntohs((*m)->port), 
                      (*m)->protocol, (*m)->fwmark);
			if ((*m)->addr == se->addr && 
				(*m)->port == se->port &&
				(*m)->protocol == se->protocol &&
				(*m)->fwmark == se->fwmark) {
				found = 1;
				if (is_in_ipvs)
					is_in_ipvs[idx] = 1;
				break;
			}
			m++;
			idx++;
		}

		if (!found) {
            LOGDEBUG("Delete unmanaged virtual: ip=%x, port=%d, protocol=%d, fwmark=%d", 
                      ntohl(se->addr), ntohs(se->port), se->protocol, se->fwmark);
			ipvsfuncs_del_service((ipvs_service_t *) se);
        }
	}
	free(vs);

//    ipvsfuncs_fprintf_ipvs(stderr);

	return 0;
}

/* ------------------------------------------------------------ */
/* args:
   - svc          - service 
   - managed_dest - pointer to dest table
   - is_in_ipvs   - function stores status is a real defined in ipvs 
                    (status is saved here only if (is_in_ipvs != NULL)
*/
int ipvsfuncs_del_unmanaged_dests(ipvs_service_t *svc,
								  ipvs_dest_t **managed_dest, gint *is_in_ipvs) {
	int i, idx, found;
	ipvs_dest_t **m;

	ipvs_service_entry_t *se = ipvs_get_service(svc->fwmark, 
												svc->protocol, 
												svc->addr, 
												svc->port);

	if (!se) 
		return 0;

	struct ip_vs_dest_entry *de;
	struct ip_vs_get_dests *vd = ipvs_get_dests(se);

	if (!vd)
		return 0;
	
	for (i = 0; i < vd->num_dests; i++) {
		de = &vd->entrytable[i];
		m = managed_dest;
		found = 0;
		idx = 0;
		while (*m) {
			if ((*m)->addr == de->addr && 
				(*m)->port == de->port) {
				found = 1;
				if (is_in_ipvs)
					is_in_ipvs[idx] = 1;
				break;
			}
			m++;
			idx++;
		}

		if (!found)
			ipvsfuncs_del_dest(svc, (ipvs_dest_t *) de);

	}
	free(vd);
	free(se);

	return 0;
}

/* ------------------------------------------------------------ */
int ipvsfuncs_fprintf_ipvs(FILE *f) {
	int i, j;

	struct ip_vs_service_entry *se;
	struct ip_vs_dest_entry *de;
	struct ip_vs_get_services *vs = ipvs_get_services();
	
	for (i = 0; i < vs->num_services; i++) {
		se = &vs->entrytable[i];
		struct ip_vs_get_dests *vd = ipvs_get_dests(se);
        if (!vd)
            continue;
		fprintf(f, " svc dest [%d], ip=%x, port=%d, fwmark=%d, proto=%d\n", 
				i, ntohl(se->addr), ntohs(se->port), se->fwmark, se->protocol);
		for (j = 0; j < se->num_dests; j++) {
			de = &vd->entrytable[j];
			fprintf(f, "  dest [%d], ip=%x, port=%d, weight=%d\n", 
					j, ntohl(de->addr), ntohs(de->port), de->weight);
		}
		free(vd);		
	}
	free(vs);

	return 0;
}

/* ---------------------------------------------------------------------- */
gboolean ipvsfuncs_set_svc_from_ht(ipvs_service_t *svc, GHashTable *ht) {
    gchar       *vproto, *vaddr, *vport, *vsched, *vpers, *v;
    u_int16_t    svcproto   = IPPROTO_TCP;
    guint        vfwmark    = 0;
    __be32       netmask    = ~0;
    unsigned     timeout    = 0;
    unsigned     flags      = 0;

    /* svcproto */
    vproto = g_hash_table_lookup(ht, "vproto");
    LOGDETAIL("vproto = [%s]", vproto);
    if (!vproto) {
        LOGERROR("No vproto set");
        return FALSE;
    }

    if (strcmp(vproto, "tcp") && 
        strcmp(vproto, "udp") && 
        strcmp(vproto, "fwmark")) {
        LOGERROR("Unknown protocol [%s]", vproto);
        return FALSE;
    }

    if (!strcmp(vproto, "udp"))
        svcproto = IPPROTO_UDP;
    else if (!strcmp(vproto, "fwmark")) {
        vfwmark = 300;
        v = g_hash_table_lookup(ht, "vfwmark");
        if (v) 
            vfwmark = atoi(v);
    }

    vaddr = g_hash_table_lookup(ht, "vaddr");
    if (!vaddr) {
        LOGERROR("No vaddr set");
        return FALSE;
    }
    
    vport = g_hash_table_lookup(ht, "vport");
    if (!vport) {
        LOGERROR("No vport set");
        return FALSE;
    }

    /* vsched */
    vsched = g_hash_table_lookup(ht, "vsched");
    if (!vsched) {
        LOGERROR("No vsched set");
        return FALSE;
    }

    /* Get persistent timeout if defined */
    vpers = g_hash_table_lookup(ht, "vpers");
    if (vpers) {
        flags = IP_VS_SVC_F_PERSISTENT;
        timeout = atoi(vpers);
    }

    ipvsfuncs_set_svc(svcproto, vaddr, vport, vfwmark, vsched, flags, timeout, netmask, svc);
    
    return TRUE;
}

/* ---------------------------------------------------------------------- */
gboolean ipvsfuncs_set_dest_from_ht(ipvs_dest_t *dest, GHashTable *ht, unsigned vconn_flags) {
    gchar       *raddr, *rport, *rrt, *v;
    gint         rweight = 1;
    u_int32_t    rl_thresh = 0, ru_thresh = 0;
    unsigned     conn_flags = vconn_flags; //use vrt conn_flags, overwrite by rrt if exists

    raddr = g_hash_table_lookup(ht, "raddr");
    if (!raddr) {
        LOGERROR("No raddr set");
        return FALSE;
    }
    
    rport = g_hash_table_lookup(ht, "rport");
    if (!rport) {
        LOGERROR("No rport set");
        return FALSE;
    }

    rrt = g_hash_table_lookup(ht, "rrt");
    if (rrt) {
        if (!strcmp(rrt, "dr")) 
            conn_flags = IP_VS_CONN_F_DROUTE;
        else if (!strcmp(rrt, "masq")) 
            conn_flags = IP_VS_CONN_F_MASQ;
        else if (!strcmp(rrt, "tun")) 
            conn_flags = IP_VS_CONN_F_TUNNEL;
        else {
            LOGERROR("Unknown routing type [%s]", rrt);
            return FALSE;
        }
    }

    v = g_hash_table_lookup(ht, "rweight");
    if (v) 
        rweight = atoi(v);

    v = g_hash_table_lookup(ht, "rl_thresh");
    if (v) 
        rl_thresh = atoi(v);

    v = g_hash_table_lookup(ht, "ru_thresh");
    if (v) 
        ru_thresh = atoi(v);

    ipvsfuncs_set_dest(raddr, rport, conn_flags,
                       rweight, ru_thresh, rl_thresh, dest);

    return TRUE;
}

