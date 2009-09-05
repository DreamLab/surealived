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

#if !defined __XMLPARSER_H
#define __XMLPARSER_H
#include <glib.h>
#include <libxml/parser.h>

typedef enum{
    BASIC_ATTR,
    EXTRA_ATTR
} xml_attr_type;

GPtrArray   *sd_xmlParseFile(gchar *filename);
gint        sd_VCfgArr_free(GPtrArray *VCfgArr);

#endif
