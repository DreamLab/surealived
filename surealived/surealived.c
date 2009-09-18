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
#include <getopt.h>
#include <sd_defs.h>
#include <sd_maincfg.h>
#include <modloader.h>
#include <xmlparser.h>
#include <sd_tester.h>
#include <sd_log.h>
#include <sd_override.h>
#include <sd_offline.h>
#include <sd_ipvssync.h>
#include <sd_cmd.h>

#define VERSION     "0.8.1"
#define SLEEP_TIME  1           /* defines delay between watchdog checks */
#define SDCONF      "/etc/surealived/surealived.cfg"

gchar              *configpath  = SDCONF;
pid_t               sd_pid;              /* child pid (if daemonized) */
gboolean            test_config = FALSE;
gboolean            daemonize   = FALSE;

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
    if (!sd_pid)                /* what?! */
        return;

    switch (signal) {
    case SIGHUP:
        LOGWARN("Watchdog: Received SIGHUP - gently restarting surealived");
        kill(sd_pid, SIGHUP);
        waitpid(sd_pid, NULL, 0); /* wait for child to exit */
        break;
    case SIGTERM:
        LOGWARN("Watchdog: Received SIGTERM - forcing restart of surealived");
        kill(sd_pid, SIGTERM);
        waitpid(sd_pid, NULL, 0);
        break;
    case SIGINT:
        LOGWARN("Watchdog: Received SIGINT - shutting down!");
        kill(sd_pid, SIGKILL);
        _exit(1);               /* man 2 _exit */
        break;
    default:
        LOGERROR("Watchdog: Received Unknown signal!");
        break;
    }
}

void setstop(int unused) {      /* HUP signal handler for child */
    LOGWARN("Surealived: received signal SIGHUP - stopping...");
    stop = TRUE;                /* we should set stop flag and wait for tests to finish */
}

void inthandler(int signal) {
    switch (signal) {
    case SIGINT:
        LOGWARN("Surealived: received signal SIGINT - killing myself - notifying parent!");
        if (daemonize)          /* this means we should have a parent */
            kill(getppid(), signal); /* delete this line if You don't want to notify parent */
        break;
    case SIGTERM:
        LOGWARN("Surealived: received signal SIGTERM - killing myself!"); /* those handlers are so depressing... */
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
        LOGERROR("Watchdog: Unable to open /dev/null!");
        exit(1);
    }

    dup2(nullfd, fileno(stdin));
    dup2(nullfd, fileno(stdout));
    dup2(nullfd, fileno(stderr));
    daemon(1, 0);

    while (TRUE) {               
        sd_pid = 0;
        signal(SIGINT, SIG_DFL); /* reset signals (for child after respawn) */
        signal(SIGTERM, SIG_DFL);
        signal(SIGHUP, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);

        if ((sd_pid = fork())) { /* parent */
            signal(SIGINT, sighandler);
            signal(SIGTERM, sighandler);
            signal(SIGHUP, sighandler);
            signal(SIGCHLD, SIG_IGN); /* ignore child signals */

            LOGINFO("Watchdog started - memory limit set to: %ld kB", G_memlimit);
            snprintf(sfilename, sizeof(sfilename), "/proc/%d/status", sd_pid);
            if (status)
                fclose(status);

            status = fopen(sfilename, "r");
            if (!status && G_memlimit) {
                LOGERROR("Watchdog: Unable to open %s - is our child dead?", sfilename);
                sleep(SLEEP_TIME);
                continue;          /* respawn! */
            }

            while (TRUE) {         /* watch our child */
                sleep(SLEEP_TIME); /* sleep till next glimpse */
                if (waitpid(sd_pid, NULL, WNOHANG) != 0) /* error or our child passed away */
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
                    LOGWARN("Watchdog: Child exceeded memory limit - exterminating!");
                    kill(sd_pid, SIGHUP); /* kill our beloved child */
                    waitpid(sd_pid, NULL, 0);
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
    SDTester       *Tester;

    gchar          *modules = NULL;
    gchar          *modpath = NULL;
    gint            ret;
    gint            next_opt;
//    struct stat     tmpstat;
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
            configpath = optarg; /* user supplied external config file */
            break;
        case 'v':
            G_logging++;         /* increase verbosity */
            break;
        case 't':
            test_config = TRUE;
            break;
        case 'd':
            daemonize = TRUE;
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

    if (!sd_maincfg_new(configpath)) {
        fprintf(stderr, "No surealived.cfg, setting defaults\n");
    }

    if (ud_override)
        G_use_offline_dump = ud;

    if (!daemonize)
        G_flog = stderr; //overwrite surealived.cfg configuration log (no make sense)

    LOGINFO("SureAliveD %s starting...", VERSION);

    sd_maincfg_print_to_log();
    modpath = sd_maincfg_get_value("modules_path");
    assert(modpath);
    modules = sd_maincfg_get_value("modules");
    assert(modules);

    modules_load(modpath, modules);
    modules_print_to_log();

    if (!test_config && daemonize)
        watchdog();

    /* here is code used ONLY by the child */
    signal(SIGHUP, setstop);      /* set HUP (restart) handler */
    signal(SIGINT, inthandler);
    signal(SIGTERM, inthandler);

    VCfgArr = sd_xmlParseFile(argv[optind]);
    if (test_config) {
        G_flog = stderr;
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
    /* update VCfgArr by offline.dump */
    sd_override_dump_merge(VCfgArr);
    sd_offline_dump_merge(VCfgArr);

    if (G_no_sync == FALSE) /* if full sync file does not exist! */
        sd_ipvssync_save_fullcfg(VCfgArr, TRUE); /* force writing full sync */
    
    Tester = sd_tester_create(VCfgArr);
    sd_cmd_listen_socket_create(G_listen_port); /* set listen port and so on */

    ret = sd_tester_master_loop(Tester);

    exit(0);
}
