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

#if !defined __MODLOADER_H
#define __MODLOADER_H

#include <glib.h>
#include <gmodule.h>

#if defined(offsetof)
#define SDOFF(type, m) offsetof(type, m)
#else
#define SDOFF(type, m) ((size_t)((char*)&((type*) 0)->m - (char*)((type*) 0)))
#endif

typedef enum {
    SD_PROTO_TCP,
    SD_PROTO_UDP,
    SD_PROTO_EXEC,
    SD_PROTO_NO_TEST
} SDTestProtocol;

typedef enum {
    STRING,
    PORT,
    UINT,
    INT,
    BOOL,
} SD_MODARG_TYPE;

typedef struct {
    gchar           *name;
    SD_MODARG_TYPE  type;
    guint           param;
    SD_MODARG_TYPE  mandatory;
    unsigned long   offset;
} mod_args;

typedef struct mod_operations {
    const gchar*    (*m_name) (void);
    SDTestProtocol  m_test_protocol;
    gpointer        (*m_set_args) (void);
    mod_args        *m_args;
    void            (*m_prepare)(CfgReal *);
    void            (*m_cleanup)(CfgReal *);
    u_int32_t       (*m_process_event)(CfgReal *);
    void            (*m_check)(CfgReal *); /* exec */
    void            (*m_start)(CfgReal *); /* exec */
    void            (*m_free)(CfgReal *);  /* free all allocated memory for real! */
} mod_operations;

typedef mod_operations (*InitModuleFunc) (gpointer data);

mod_operations *module_lookup(const gchar *name);
void            modules_load(gchar *modpath, gchar *modlist);
void            modules_print_to_log(void);

#endif
