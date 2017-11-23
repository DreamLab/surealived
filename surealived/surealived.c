/*
 * Copyright 2009-2011 DreamLab Onet.pl Sp. z o.o.
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
#include <getopt.h>
#include <sd_defs.h>
#include <sd_maincfg.h>
#include <modloader.h>
#include <xmlparser.h>
#include <sd_tester.h>
#include <sd_log.h>
#include <sd_override.h>
#include <sd_offline.h>
#include <sd_stats.h>
#include <sd_notify.h>
#include <sd_ipvssync.h>
#include <sd_cmd.h>
#include <sdversion.h>

#define SLEEP_TIME  1           /* defines delay between watchdog checks */
#define SDCONF      "/etc/surealived/surealived.cfg"

gchar              *G_config_path = SDCONF;
pid_t               G_child_pid;              /* child pid (if daemonized) */
gboolean            G_test_config = FALSE;
gboolean            G_daemonize   = FALSE;

void print_usage(gchar *pname) {
    fprintf(stderr, "=== SureAliveD v.%s ===\n", VERSION);
    fprintf(stderr, "Usage: %s [options] <xml_config_file>\n", pname);
    fprintf(stderr, "Ex   : %s -c /root/sd_new.conf -vv -d test_http.xml\n", pname);
    fprintf(stderr, "\nOptions:\n"
        "\t--help\t\t-h\t\tThis help info\n"
        "\t--test-config\t-t\t\tTest configuration and exit\n"
        "\t--config\t-c <path>\tUse <path> as config file\n"
        "\t--verbose\t-v\t\tIncrease verbosity level\n"
        "\t--daemonize\t-d\t\tRun in background (daemonize)\n"
        "\t--no-sync\t-n\t\tDo not write sync info\n"
        "\t--no-dumpfile\t-k\t\tDo not load and create offline.dump\n"
        "\t--version\t-V\t\tShow Version information\n");
    exit(1);
}

void print_version() {
    fprintf(stderr, "=== SureAliveD v.%s ===\n", VERSION);
    exit(1);
}

void sighandler(int signal) {   /* signal handler for watchdog */
    if (!G_child_pid)                /* what?! */
        return;

    switch (signal) {
    case SIGHUP:
        LOGWARN("watchdog: Received SIGHUP - gently restarting surealived");
        LOG_NB_FLUSH_QUEUES();
        kill(G_child_pid, SIGHUP);
        waitpid(G_child_pid, NULL, 0); /* wait for child to exit */
        break;
    case SIGTERM:
        LOGWARN("watchdog: Received SIGTERM - forcing restart of surealived");
        LOG_NB_FLUSH_QUEUES();
        kill(G_child_pid, SIGTERM);
        waitpid(G_child_pid, NULL, 0);
        break;
    case SIGINT:
        LOGWARN("watchdog: Received SIGINT - shutting down!");
        LOG_NB_FLUSH_QUEUES();
        kill(G_child_pid, SIGKILL);
        _exit(1);               /* man 2 _exit */
        break;
    default:
        LOGERROR("watchdog: Received Unknown signal!");
        LOG_NB_FLUSH_QUEUES();
        break;
    }
}

void setstop(int unused) {      /* HUP signal handler for child */
    LOGWARN("surealived: received signal SIGHUP - stopping...");
    LOG_NB_FLUSH_QUEUES();
    stop = TRUE;                /* we should set stop flag and wait for tests to finish */
}

void inthandler(int signal) {
    switch (signal) {
    case SIGINT:
        LOGWARN("surealived: received signal SIGINT - killing myself - notifying parent!");
        LOG_NB_FLUSH_QUEUES();
        if (G_daemonize)          /* this means we should have a parent */
            kill(getppid(), signal); /* delete this line if You don't want to notify parent */
        break;
    case SIGTERM:
        LOGWARN("surealived: received signal SIGTERM - killing myself!"); /* those handlers are so depressing... */
        LOG_NB_FLUSH_QUEUES();
        break;
    }
//    _exit(1);
    exit(0);         /* please use _exit this is just for profiling */
}

void watchdog() {
    gchar   sfilename[MAXPATHLEN];
    gchar   name[32];
    gulong  value = 0;
    FILE    *status = NULL;
    int     nullfd = open("/dev/null", O_NONBLOCK | O_RDWR);

    if (nullfd < 0) {
        LOGERROR("watchdog: Unable to open /dev/null!");
        exit(1);
    }

    dup2(nullfd, fileno(stdin));
    dup2(nullfd, fileno(stdout));
    dup2(nullfd, fileno(stderr));
    daemon(1, 0);

    while (TRUE) {               
        G_child_pid = 0;
        signal(SIGINT, SIG_DFL); /* reset signals (for child after respawn) */
        signal(SIGTERM, SIG_DFL);
        signal(SIGHUP, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);

        if ((G_child_pid = fork())) { /* parent */
            signal(SIGINT, sighandler);
            signal(SIGTERM, sighandler);
            signal(SIGHUP, sighandler);
            signal(SIGCHLD, SIG_IGN); /* ignore child signals */

            LOGINFO("watchdog: started - memory limit set to: %ld kB", G_memlimit);
            LOG_NB_FLUSH_QUEUES();            
            snprintf(sfilename, sizeof(sfilename), "/proc/%d/status", G_child_pid);
            if (status)
                fclose(status);

            status = fopen(sfilename, "r");
            if (!status && G_memlimit) {
                LOGERROR("watchdog: Unable to open %s - is our child dead?", sfilename);
                LOG_NB_FLUSH_QUEUES();
                sleep(SLEEP_TIME);
                continue;          /* respawn! */
            }

            while (TRUE) {         /* watch our child */
                sleep(SLEEP_TIME); /* sleep till next glimpse */
                if (waitpid(G_child_pid, NULL, WNOHANG) != 0) /* error or our child passed away */
                    break;         /* respawn! */

                if (!G_memlimit)     /* if memory limit is infinity */
                    continue;

                memset(name, 0, sizeof(name));
                fclose(status);
                status = fopen(sfilename, "r"); /* we need to reopen file (rewind/fseek doesn't work) */
                value = G_memlimit+1; /* if next while fails we're gonna break the loop (ie file does not exist anymore)*/

                while (fscanf(status, "%31s %lu\n", name, &value) != EOF) {
                    if (strlen(name) == 7 && !strcmp(name, "VmSize:"))
                        break;
                }

                if (value > G_memlimit) {
                    LOGWARN("watchdog: Child exceeded memory limit - exterminating!");
                    LOG_NB_FLUSH_QUEUES();
                    kill(G_child_pid, SIGHUP); /* kill our beloved child */
                    waitpid(G_child_pid, NULL, 0);
                    break;
                }
            }
        }
        else {                  /* child */
            setsid();           /* set group leader */
            break;              /* break the loop */
        }
    }
}

gint main(gint argc, gchar **argv) {
    GPtrArray      *VCfgArr;
    GHashTable     *VCfgHash;
    SDTester       *Tester;

    gchar          *modules = NULL;
    gchar          *modpath = NULL;
    gint            ret;
    gint            next_opt;
    gboolean        ud = G_use_offline_dump;
    gboolean        ud_override = FALSE;

    const char     *short_opt = "hc:tvdnkV";
    struct option   long_opt[] = {
        { "help",        0, NULL, 'h' },
        { "test-config", 0, NULL, 't' },
        { "config",      1, NULL, 'c' },
        { "verbose",     0, NULL, 'v' },
        { "daemonize",   0, NULL, 'd' },
        { "no-sync",     0, NULL, 'n' },
        { "no-dumpfile", 0, NULL, 'k' },
        { "version",     0, NULL, 'V' },
        { NULL,          0, NULL, 0 }
    };

    do {
        next_opt = getopt_long(argc, argv, short_opt, long_opt, NULL);
        switch(next_opt) {
        case 'h':
            print_usage(argv[0]);
        case 'V':
            print_version();
        case 'c':
            G_config_path = optarg; /* user supplied external config file */
            break;
        case 'v':
            G_logging++;         /* increase verbosity */
            break;
        case 't':
            G_test_config = TRUE;
            break;
        case 'd':
            G_daemonize = TRUE;
            break;
        case 'n':
            G_no_sync = TRUE;
            break;
        case 'k':
            ud = FALSE;
            ud_override = TRUE;
            break;
        case '?':
            print_usage(argv[0]);
        default:
            break;
        }
    } while (next_opt != -1);

    if (!argv[optind])          /* no xml file specified */
        print_usage(argv[0]);

    srand(time(NULL));          /* if modules want to use rand() this will help */

    if (!sd_maincfg_new(G_config_path)) {
        fprintf(stderr, "No surealived.cfg, setting defaults\n");
    }

    if (ud_override)
        G_use_offline_dump = ud;

    if (!G_daemonize)
        G_logfd = STDERR_FILENO; //overwrite surealived.cfg configuration log (no make sense)

    LOGINFO("Initializing...");

    sd_maincfg_print_to_log();
    modpath = sd_maincfg_get_value("modules_path");
    assert(modpath);
    modules = sd_maincfg_get_value("modules");
    assert(modules);

    modules_load(modpath, modules);
    modules_print_to_log();

    LOG_NB_FLUSH_QUEUES();
    if (!G_test_config && G_daemonize)
        watchdog();

    /* here is code used ONLY by the child */
    signal(SIGHUP, setstop);      /* set HUP (restart) handler */
    signal(SIGINT, inthandler);
    signal(SIGTERM, inthandler);

    VCfgArr = sd_xmlParseFile(argv[optind]);
    
    if (G_test_config) {
        G_logfd = STDERR_FILENO;
        G_logging = LOGLEV_INFO;

        if (VCfgArr) {
            LOGINFO("XML configuration file [%s] OK", argv[optind]);
            exit(0);
        } 
        else {
            LOGERROR("XML configuration file [%s] ERROR", argv[optind]);
            exit(1);
        }
    }

    if (!VCfgArr) {
        LOGERROR("Configuration file is broken!");
        exit(1);
    }

    VCfgHash = sd_vcfg_hashmap_new(VCfgArr);

    /* update VCfgArr by offline.dump */
    sd_override_dump_merge(VCfgArr);
    sd_offline_dump_merge(VCfgArr);
    sd_stats_dump_merge(VCfgArr, VCfgHash);
    sd_notify_dump_merge(VCfgArr, VCfgHash);

    if (G_no_sync == FALSE) /* if full sync file does not exist! */
        sd_ipvssync_save_fullcfg(VCfgArr, TRUE); /* force writing full sync */
    
    Tester = sd_tester_create(VCfgArr);
    sd_cmd_listen_socket_create(G_listen_addr, G_listen_port); /* set listen addr and port */

    ret = sd_tester_master_loop(Tester);
    LOGINFO("Master loop, ret = %d\n", ret);

    exit(0);
}
