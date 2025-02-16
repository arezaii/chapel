/*
 * Copyright 2020-2025 Hewlett Packard Enterprise Development LP
 * Copyright 2004-2019 Cray Inc.
 * Other additional copyright holders may be indicated within.
 *
 * The entirety of this work is licensed under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _INSERT_LINE_NUMBERS_H_
#define _INSERT_LINE_NUMBERS_H_

#include <map>
#include <string>
#include <vector>

// used to generate chpl_filenameTable
extern std::vector<std::string> gFilenameLookup;

// Caches the location of filenames in gFilenameLookup
extern std::map<std::string, int> gFilenameLookupCache;

int getFilenameLookupPosition(const std::string& name);

#endif //_INSERT_LINE_NUMBERS_H
