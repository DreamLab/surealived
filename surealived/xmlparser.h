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

#if !defined __XMLPARSER_H
#define __XMLPARSER_H
#include <glib.h>
#include <libxml/parser.h>

typedef enum{
    BASIC_ATTR,
    EXTRA_ATTR
} SD_ATTR_TYPE;

GPtrArray   *sd_xmlParseFile(gchar *filename);
gint        sd_VCfgArr_free(GPtrArray *VCfgArr);

#endif
