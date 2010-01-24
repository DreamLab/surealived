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
#include <modloader.h>

static GHashTable *THash = NULL;

static gint module_loader(const gchar *modpath, const gchar *modname) {
    GModule        *module;
    gchar           fname[BUFSIZ];
    InitModuleFunc  initmod;
    mod_operations *mops = g_new0(mod_operations, 1);

    snprintf(fname, BUFSIZ, "%s/%s", modpath, modname);

    FLOGINFO(TRUE, FALSE, "Trying to open module [%s] \t", modname);
    module = g_module_open (fname, G_MODULE_BIND_LAZY);
    if (!module) {
        FLOGERROR(FALSE, TRUE, "NOT FOUND, ERROR");
        return -1;
    }
    FLOGINFO(FALSE, TRUE, "OK");

    FLOGDEBUG(TRUE, FALSE, " * symbol __init_module\t\t\t");
    if (!g_module_symbol (module, "__init_module", (gpointer *) &initmod))
        {
            LOGERROR(FALSE, TRUE, "NOT FOUND, ERROR");
            return -1;
        }
    FLOGDEBUG(FALSE, TRUE, "FOUND");

    FLOGDEBUG(TRUE, FALSE, " * __init_module address\t\t");
    if (initmod == NULL)
        {
            LOGERROR(FALSE, TRUE, "NULL, ERROR [%s]", g_module_error());
            return -1;
        }
    FLOGDEBUG(FALSE, TRUE, "%p", initmod);
    memset((void *)mops, 0, sizeof(mod_operations));
    *mops = initmod();

    if (!THash)
        THash = g_hash_table_new(g_str_hash, g_str_equal);
    g_hash_table_insert(THash, (gpointer) mops->m_name, (gpointer) mops);

    return 0;
}

/*! Find a module by \a name.
  \param name Module name. Should be previously registered.
*/
mod_operations *module_lookup(const gchar *name) {
    return (mod_operations *) g_hash_table_lookup(THash, name);
}

/* ---------------------------------------------------------------------- */
static void _load_all(gchar *modpath) {
    GDir *dir = g_dir_open(modpath, 0, NULL);
    const gchar *de;

    if (!dir) {
        G_flog = stderr;
        LOGERROR("Can't open directory: %s", modpath);
        exit(1);
    }

    while ((de = g_dir_read_name(dir))) {
        if (!g_str_has_prefix(de, "mod_"))
            continue;
        if (!g_str_has_suffix(de, ".so"))
            continue;
        LOGDEBUG("matching entry: %s", de);
        if (module_loader(modpath, de))
            exit(1);
    };

    g_dir_close(dir);
}

/*! \brief Load modules located in \a modpath 
  \param modpath Path to modules
  \param modlist Coma separated module list (ex. "http,tcp"). No blank characters allowed after coma sign.
*/
void modules_load(gchar *modpath, gchar *modlist) {
    gchar **sv, **mod;

    if (!strcmp(modlist, "all"))
        _load_all(modpath);
    else {
        sv = g_strsplit(modlist, ",", 0);
        mod = sv;
        while (*mod) {
            gchar *soname = g_strdup_printf("mod_%s.so", *mod);
            if (module_loader(modpath, soname))
                exit(1);
            mod++;
            g_free(soname);
        }
        g_strfreev(sv);
    }
}

/* ---------------------------------------------------------------------- */
static void _module_print_tester(gpointer key, gpointer value, gpointer userdata) {
    LOGINFO(" * %s", (gchar *) key);
}

void modules_print_to_log(void) {
    LOGINFO("Registered tester modules:");
    g_hash_table_foreach(THash, _module_print_tester, NULL);
}
