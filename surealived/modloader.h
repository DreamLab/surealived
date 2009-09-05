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
