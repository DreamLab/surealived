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

#if !defined __SD_OVERRIDE_H
#define __SD_OVERRIDE_H

void sd_override_dump_add(CfgReal *real);
void sd_override_dump_del(CfgReal *real);
gint sd_override_dump_save();
void sd_override_dump_merge(GPtrArray *VCfgArr);

#endif
