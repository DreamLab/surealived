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

static mod_operations mops;

static const gchar *name = "tcp";

static const gchar *module_name(void) {
    return name;
}

static u_int32_t module_process_event(CfgReal *real) {
    LOGDEBUG("Connection successfull: fd=%d\n", real->fd);
    real->test_state = TRUE;
    return WANT_END;
}

mod_operations __init_module(gpointer data) {
    LOGINFO(" ** Init module: setting mod_operations for tester [%s]", name);

    mops.m_name          = module_name;
    mops.m_test_protocol = SD_PROTO_TCP;
    mops.m_process_event = module_process_event;

    return mops;
}
