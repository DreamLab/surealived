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

#include <stdio.h>
#include <glib.h>
#include <common.h>
#include <modloader.h>
#include <sd_defs.h>
#include <xmlparser.h>
#include <unistd.h>

#define PARAMS_LENGTH 1024
static mod_operations mops;

static const gchar *name = "exec";

static mod_args m_args[]={
    { "params", STRING, PARAMS_LENGTH, EXTRA_ATTR, 0 },
    { NULL, 0,  0,  0,  0 },
};

static const gchar *module_name(void) {
    return name;
}

static gpointer module_set_args(void) {
    gchar *params = (gchar *) malloc(PARAMS_LENGTH);
    memset(params, 0, PARAMS_LENGTH);
    return params;
}

static void module_check_real(CfgReal *real) {
    gint status = 0;

    if (waitpid(real->pid, &status, WNOHANG) > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0) { /* online! */
        LOGDETAIL("mod_exec: (real = %s:%s) command success (pid = %d)", real->virt->name, real->name, real->pid);
        real->test_state = TRUE;
    }
    else{
        kill(-real->pid, SIGKILL);
        waitpid(real->pid, &status, 0);
        LOGDETAIL("mod_exec: (real = %s:%s) command failed (pid = %d)", real->virt->name, real->name, real->pid);
    }
}

static void module_start_real(CfgReal *real) {
    gint i,len;
    gint tokens = 3;            /* argv[0] + addr + port */
    GString *s = NULL;

    if (real->moddynamic == NULL) {

        len = strlen((gchar *)real->moddata);
        for (i = 0; i < len; i++)
            if (((gchar *)real->moddata)[i] == ' ')
                tokens++;

        if (len > 0) 
            tokens++; 

        real->moddynamic = (gchar **)malloc((tokens+1) * sizeof(gchar *));

        memset(real->moddynamic, 0, (tokens+1)*sizeof(gchar *));
        for (i = len; i >= 0; i--)
            if (((gchar *)real->moddata)[i] == ' ') {
                ((gchar **)real->moddynamic)[--tokens] = &((gchar *)real->moddata)[i+1];
                ((gchar *)real->moddata)[i] = 0;
            }
        ((gchar **)real->moddynamic)[3] = tokens > 3 ? (gchar *)real->tester->moddata : NULL;
        ((gchar **)real->moddynamic)[2] = g_strdup_printf("%d", ntohs(real->port)); /* we will NEED to free this */
        ((gchar **)real->moddynamic)[1] = real->addrtxt;
        ((gchar **)real->moddynamic)[0] = real->tester->exec;
    }

    if (!(real->pid = fork())) { /* child */
        dup2(fileno(G_flog), 1);
        dup2(fileno(G_flog), 2);
        real->pgrp = setpgid(0, getpid());
        
        s = g_string_sized_new(PARAMS_LENGTH);
        i = 0;
        while (((gchar **)real->moddynamic)[i])
            g_string_append_printf(s, "%s ", ((gchar **)real->moddynamic)[i++]);
        g_strchomp(s->str);

        LOGDEBUG("mod_exec: execve (real = %s:%s) [exec = %s, args = %s, pid = %d]", 
                 real->virt->name, real->name, real->tester->exec, s->str, getpid());

        execve(real->tester->exec, real->moddynamic, real->moddynamic);

        LOGERROR("mod_exec: error on execve (real = %s:%s) [exec = %s, args = %s, pid = %d]!",
                 real->virt->name, real->name, real->tester->exec, s->str, getpid());
        g_string_free(s, TRUE); /* we need to free this only if execve failed */
        exit(1);                /* error on execve - fail */
    }
}

static void module_free(CfgReal *real) {
    gchar   **md = (gchar **)real->moddynamic;
    if (md) {
        if (md[2])
            free(md[2]); /* we used strdup earlier */
        free(md);
    }
}

mod_operations __init_module(gpointer data) {
    LOGINFO(" ** Init module: setting mod_operations for tester [%s]", name);

    mops.m_name             = module_name;
    mops.m_test_protocol    = SD_PROTO_EXEC;
    mops.m_set_args         = module_set_args;
    mops.m_args             = m_args;
    mops.m_process_event    = NULL;
    mops.m_check            = module_check_real;
    mops.m_start            = module_start_real;
    mops.m_free             = module_free;
    return mops;
}
