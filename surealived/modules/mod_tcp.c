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
