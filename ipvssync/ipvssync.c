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
#include <getopt.h>
#include <config.h>
#include <ipvsfuncs.h>
#include <diffs.h>
#include <maincfg.h>
#include <logging.h>
#include <sdversion.h>

#define SDCONF      "/etc/surealived/surealived.cfg"

void print_usage(gchar *pname) {
    fprintf(stderr, "=== IPVSSync v.%s ===\n", VERSION);
    fprintf(stderr, "Usage: %s [options]\n", pname);
    fprintf(stderr, "Ex   : %s -c /home/surealived/surealived.cfg\n", pname);
    fprintf(stderr, "\nOptions:\n"
        "\t--help\t\t-h\t\tThis help info\n"
        "\t--test-config\t-t\t\tTest ipvsfull.cfg configuration and exit\n"
        "\t--config\t-c\t\tConfig file (default /etc/surealived/surealived.cfg)\n"
        "\t--verbose\t-v\t\tIncrease verbosity level\n"
        "\t--daemonize\t-d\t\tRun in background (daemonize)\n"
        "\t--del-umanaged\t-u\t\tDelete unmanaged virtuals from IPVS table\n"
        "\t--keep-diffs\t-k\t\tDon't remove processed diff files\n"
        "\t--version\t-V\t\tShow Version information\n");
    exit(1);
}

void print_version() {
    fprintf(stderr, "=== IPVSSync v.%s ===\n", VERSION);
    exit(1);
}

gboolean file_exists(gchar *fname) {
    gboolean chkv = TRUE;
    struct stat st;
 
    if (stat(fname, &st))
        chkv = FALSE;

    return chkv;
}

void reloadhandler(int signal) {
    int fd;
    LOGWARN("IPVSSync: received signal SIGHUP - reloading config.");

    fd = open(G_full_reload_file, O_CREAT, S_IRWXU | S_IRGRP | S_IROTH);

    if (fd < 0) {
        fprintf(stderr, "Can't touch full reload file: %s [error=%s]\n", 
		G_full_reload_file, STRERROR);
        exit(1);
    }

    LOGINFO("Full reload file touched [%s]", G_full_reload_file);
    close(fd);
}

void inthandler(int signal) {
    switch (signal) {
    case SIGINT:
        LOGWARN("IPVSSync: received signal SIGINT - exiting.");
        break;
    case SIGTERM:
        LOGWARN("IPVSSync: received signal SIGTERM - exiting.");
        break;
    }
    LOG_NB_FLUSH_QUEUES();
    exit(0);
}

void sigdefhandler(int signal) {
  LOGWARN("IPVSSync: Received signal [%d] - exiting", signal);
  exit(1);
}

/* ------------------------------------------------------------ */
gint main(gint argc, gchar **argv) {
    Config     *conf, *newconf;
    gint        next_opt;
    gboolean    test_config = 0;
    gchar      *configpath = NULL;
    gboolean    daemonize = FALSE;
    gboolean    del_unmanaged = FALSE;
    gboolean    keep_diffs = FALSE;
    struct stat st;
    sigset_t    blockset;
    sigset_t    oldset;

    const char     *short_opt = "hc:tcvdukV";
    struct option   long_opt[] = {
        { "help",         0, NULL, 'h' },
        { "test-config",  0, NULL, 't' },
        { "config",       1, NULL, 'c' },
        { "verbose",      0, NULL, 'v' },
        { "daemonize",    0, NULL, 'd' },
        { "del-umanaged", 0, NULL, 'u' },
        { "keep-diffs",   0, NULL, 'k' },
        { "version",      0, NULL, 'V' },
        { NULL,           0, NULL, 0 }
    };

    do {
        next_opt = getopt_long(argc, argv, short_opt, long_opt, NULL);
        switch(next_opt) {
        case 'h':
            print_usage(argv[0]);
        case 'V':
            print_version();
        case 'v':
            G_logging++;         /* increase verbosity */
            break;
        case 't':
            test_config = TRUE;
            break;
        case 'd':
            daemonize = TRUE;
            break;
        case 'u':
            del_unmanaged = TRUE;
            break;
        case 'k':
            keep_diffs = TRUE;
            break;
        case 'c':
            configpath = optarg; /* another (not /etc/surealived/surealived.cfg file) */
            break;
        }
    } while (next_opt != -1);

    if (!maincfg_new(SDCONF)) {
        fprintf(stderr, "ERROR loading config [%s]\n", SDCONF);
        exit(1);
    }

    if (test_config) {
        G_logfd = STDERR_FILENO;
        conf = config_full_read(G_full_sync_file);
        if (!conf) {
            fprintf(stderr, "ipvssync config file [%s] syntax ERROR\n", G_full_sync_file);
            exit(1);
        }
        fprintf(stderr, "ipvssync config file [%s] syntax OK\n", G_full_sync_file);
        exit(0);
    }

    signal(SIGHUP, reloadhandler);
    signal(SIGINT, inthandler);
    signal(SIGTERM, inthandler);

    signal(SIGFPE, sigdefhandler);
    signal(SIGILL, sigdefhandler);
    signal(SIGSEGV, sigdefhandler);

    LOG_NB_FLUSH_QUEUES();

    if (!daemonize)
        G_logfd = STDERR_FILENO; 
    else
        daemon(1, 1);

    /* Check configuration files */
    if (file_exists(G_full_sync_file))
        LOGINFO("ipvssync config file [%s] FOUND", G_full_sync_file);
    else {
        LOGERROR("ipvssync config file [%s] NOT FOUND!", G_full_sync_file);
        LOG_NB_FLUSH_QUEUES();
        exit(1);
    }

    if (file_exists(G_lock_sync_file))
        LOGINFO("ipvssync lock file [%s] FOUND", G_lock_sync_file);
    else {
        LOGERROR("ipvssync lock file [%s] NOT FOUND!", G_lock_sync_file);
        LOG_NB_FLUSH_QUEUES();
        exit(1);
    }

    LOGINFO("ipvssync reload file [%s]", G_full_reload_file);

    ipvsfuncs_initialize();
    
    /* Configuration read:
       - lock 
       - read ipvsfull.cfg
       - read diff file until EOF
       - ipvs synchronize
    */
    lock_ipvssync();

    conf = config_full_read(G_full_sync_file);
    if (!conf) {
        fprintf(stderr, "ipvssync config file [%s] syntax ERROR\n", G_full_sync_file);
        exit(1);
    }

    LOGINFO("ipvssync diff file [%s] set", conf->diff_filename);
    diffs_apply(conf, FALSE); //at start don't sync changes to ipvs (it can be empty for example)
    config_synchronize(conf, FALSE, del_unmanaged);
    LOGINFO("Synchronization del_unmanaged = %s", GBOOLSTR(del_unmanaged));
    unlink(G_full_reload_file); //delete reload flag (we make full sync at start);
    unlock_ipvssync();

    sigfillset(&blockset);
    while (1) {
        sigprocmask(SIG_BLOCK, &blockset, &oldset); //avoid signal handling inside the loop
        lock_ipvssync();
//        LOGINFO("LOOP");

        /* Check is surealived restarted (and creates ipvssync.reload) */
        if (!stat(G_full_reload_file, &st)) {
            LOGINFO("Reload file [%s] found. Trying to reload configuration...", G_full_reload_file);
            newconf = config_full_read(G_full_sync_file);
            if (!newconf) {
                LOGERROR("ipvssync config error [%s], config couln't be reloaded", G_full_sync_file);
                LOG_NB_FLUSH_QUEUES();
                sleep(1);
            }
            else {
                LOGINFO("Freeing old configuration");
                config_full_free(conf);                
                conf = newconf;
                LOGINFO("Synchronizing new one, del_unmanaged = %s", GBOOLSTR(del_unmanaged));
                config_synchronize(conf, FALSE, del_unmanaged);
            }
            unlink(G_full_reload_file);
        }
        diffs_apply(conf, TRUE);
        LOG_NB_FLUSH_QUEUES();

        if (!keep_diffs)
            diffs_clean_old_files(conf);

        unlock_ipvssync();
        sigprocmask(SIG_SETMASK, &oldset, NULL); 

        usleep(100000); //we have to wait some time to allow sd to lock 
    }

	ipvs_close();
	return 0;
}
