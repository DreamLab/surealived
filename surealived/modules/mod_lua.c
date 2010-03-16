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
#include <xmlparser.h>
#include <ctype.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define PARAMS_LENGTH 1024
static mod_operations mops;

typedef struct {
    gchar   script_path[1024];     /* path to lua script to be called */
    gchar   params[PARAMS_LENGTH]; /* additional parameters */
} TestLUA;

typedef struct {
    lua_State *L;
    gchar   buf[BUFSIZ];
} LUAData;

static mod_args m_args[] = {    /* this struct defines what we want from XML */
    { "script", STRING, 1024,          BASIC_ATTR, SDOFF(TestLUA, script_path) },
    { "params", STRING, PARAMS_LENGTH, EXTRA_ATTR, SDOFF(TestLUA, params) },
    { NULL, 0, 0, 0, 0 },       /* we need to end this struct with NULLs */
};

static gpointer module_alloc_args(void) {
    TestLUA *ret = g_new(TestLUA, 1);
    memset(ret, 0, sizeof(TestLUA));
    return ret;
}

static void mod_lua_test_prepare(CfgReal *real) {
    TestLUA *tlua  = (TestLUA *)real->moddata;
    LUAData *luadata = (LUAData *)real->moddynamic;

    LOGDETAIL("mod_lua: process prepare");
    LOGDETAIL("mod_lua: script: %s", tlua->script_path);
    LOGDETAIL("mod_lua: params: %s", tlua->params);

    LOGDETAIL("mod_lua: moddynamic = %x", real->moddynamic);
    if (!real->moddynamic) {
        real->moddynamic = malloc(sizeof(LUAData));
        memset(real->moddynamic, 0, sizeof(LUAData));
        luadata = (LUAData *)real->moddynamic;
        real->buf = luadata->buf;
    }
    LOGDETAIL("mod_lua: after allocation moddynamic = %x", real->moddynamic);

    luadata->L = lua_open();
    luaL_openlibs(luadata->L);
    if (luaL_loadfile(luadata->L, tlua->script_path) || lua_pcall(luadata->L, 0, 0, 0)) {
        LOGERROR("mod_lua: can't load lua script [%s, error=%s]\n",
                 tlua->script_path, lua_tostring(luadata->L, -1));
        real->error = 1;
        return;
    }
    LOGDETAIL("mod_lua: after load file");

    lua_getglobal(luadata->L, "prepare");
    if (!lua_isfunction(luadata->L, -1)) {
        LOGERROR("mod_lua: can't find prepare() function in script [%s]", tlua->script_path);
        lua_pop(luadata->L, 1);
        real->error = 1;
        return;
    }
    LOGDETAIL("mod_lua: calling prepare() function");
    lua_pushstring(luadata->L, tlua->params);
    lua_pushstring(luadata->L, real->virt->name);
    lua_pushstring(luadata->L, real->virt->addrtxt);
    lua_pushinteger(luadata->L, ntohs(real->virt->port));
    lua_pushstring(luadata->L, sd_proto_str(real->virt->ipvs_proto));
    lua_pushinteger(luadata->L, ntohs(real->virt->ipvs_fwmark));
    lua_pushstring(luadata->L, real->name);
    lua_pushstring(luadata->L, real->addrtxt);
    lua_pushinteger(luadata->L, ntohs(real->port));
    lua_pushinteger(luadata->L, ntohs(real->testport));

    if (lua_pcall(luadata->L, 10, 0, 0) != 0) {
        LOGERROR("mod_lua: error calling prepare() function [error = %s]", lua_tostring(luadata->L, -1));
        real->error = 1;
    }
}

static u_int32_t mod_lua_process_event(CfgReal *real) {
    TestLUA *tlua = (TestLUA *)real->moddata;
    LUAData *luadata = (LUAData *)real->moddynamic;
    guint nbytes;
    const gchar *rstr;
    const gchar *wbuf;

    lua_settop(luadata->L, 0);
    LOGDETAIL("mod_lua: process event");
    LOGDETAIL(" check stack: %d, gettop = %d", lua_checkstack(luadata->L, 0), lua_gettop(luadata->L));

    if (real->error)                        /* Error occured */
        return WANT_END;                    /* abort test */

    lua_getglobal(luadata->L, "process_event");
    if (!lua_isfunction(luadata->L, -1)) {
        LOGERROR("mod_lua: can't find process_event() function in script [%s]", tlua->script_path);
        lua_pop(luadata->L, 1);
        real->error = 1;
        return WANT_END;
    }
    LOGDETAIL("mod_lua: calling process_event() function (real->buf = %x)", real->buf);
    lua_pushstring(luadata->L, real->buf);
    LOGDETAIL(" check stack: %d, gettop = %d", lua_checkstack(luadata->L, 0), lua_gettop(luadata->L));
    
    if (lua_pcall(luadata->L, 1, 3, 0) != 0) {
        LOGERROR("mod_lua: error calling prepare() function [error = %s]", lua_tostring(luadata->L, -1));
        real->error = 1;
    }
    LOGDETAIL(" check stack: %d, gettop = %d", lua_checkstack(luadata->L, 0), lua_gettop(luadata->L));
    
    rstr   = lua_tolstring(luadata->L, 1, NULL);
    nbytes = lua_tointeger(luadata->L, 2);
    wbuf   = lua_tolstring(luadata->L, 3, NULL);
    LOGDETAIL(" ret[1] = \"%s\" (%d)", rstr, 0);
    LOGDETAIL(" ret[2] = %d", nbytes);
    LOGDETAIL(" ret[3] = \"%s\" (%d)", wbuf, 0);

    if (!strcmp(rstr, "rav")) {
        LOGDETAIL(" * switch to WANT_READ_AV(%d bytes)", nbytes);
        memset(luadata->buf, 0, nbytes+1);
        return WANT_READ_AV(nbytes);
    } 
    else if (!strcmp(rstr, "r")) {
        LOGDETAIL(" * switch to WANT_READ(%d bytes)", nbytes);
        memset(luadata->buf, 0, nbytes+1);
        return WANT_READ(nbytes);
    } 
    else if (!strcmp(rstr, "w")) {
        LOGDETAIL(" * switch to WANT_WRITE(%d bytes)", nbytes);
        memcpy(real->buf, wbuf, nbytes);
        real->buf[nbytes] = 0;
        return WANT_WRITE(nbytes);
    } 
    else if (!strcmp(rstr, "eok")) {
        LOGDETAIL(" * switch to WANT_END OK");
        real->test_state = TRUE;
        return WANT_END;
    } 
    else if (!strcmp(rstr, "efail")) {
        LOGDETAIL(" * switch to WANT_END FAIL");
        return WANT_END;
    } 

    return WANT_END;
}

static void mod_lua_cleanup(CfgReal *real) {
    LUAData *luadata = (LUAData *)real->moddynamic;
    LOGDETAIL("mod_lua: cleanup");
    lua_close(luadata->L);
    free(luadata);
    luadata = NULL;
    real->moddynamic = NULL;
}

static void mod_free(CfgReal *real) {
    if (real->moddynamic)     
        free(real->moddynamic);    
    real->buf = NULL;                              
    real->moddynamic = NULL;
}

mod_operations __init_module(void) {
    LOGINFO(" ** Init module: setting mod_operations for tester [lua]");

    mops.m_name             = "lua";
    mops.m_test_protocol    = SD_PROTO_TCP;
    mops.m_args             = m_args;

    mops.m_alloc_args       = module_alloc_args;
    mops.m_prepare          = mod_lua_test_prepare;
    mops.m_process_event    = mod_lua_process_event;
    mops.m_cleanup          = mod_lua_cleanup;
    mops.m_free             = mod_free;

    return mops;
}
