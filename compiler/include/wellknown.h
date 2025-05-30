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

#ifndef _WELL_KNOWN_H_
#define _WELL_KNOWN_H_

#include <vector>

class AggregateType;
class Type;
class FnSymbol;

void initializeWellKnown();

// for use in build.cpp when processing a class/record declaration.
// If the name matches a well-known type that is needed early,
// this function returns the AggregateType* which should have its
// fields replaced with the type that is being created.
// Otherwise, this function returns nullptr.
AggregateType* shouldWireWellKnownType(const char* name);

void gatherIteratorTags();
void gatherWellKnownTypes();
void gatherWellKnownFns();

std::vector<Type*> getWellKnownTypes();
void clearGenericWellKnownTypes();
Type* getWellKnownTypeWithName(const char* name);

std::vector<FnSymbol*> getWellKnownFunctions();
void clearGenericWellKnownFunctions();

// The well-known types
extern AggregateType* dtArray;
extern AggregateType* dtBaseArr;
extern AggregateType* dtBaseDom;
extern AggregateType* dtBytes;
extern AggregateType* dtCFI_cdesc_t;
extern AggregateType* dtDist;
extern AggregateType* dtDomain;
extern AggregateType* dtError;
extern AggregateType* dtExternalArray;
extern AggregateType* dtLocale;
extern AggregateType* dtLocaleID;
extern AggregateType* dtMainArgument;
extern AggregateType* dtObject;
extern AggregateType* dtOnBundleRecord;
extern AggregateType* dtOpaqueArray;
extern AggregateType* dtOwned;
extern AggregateType* dtRange;
extern AggregateType* dtRef;
extern AggregateType* dtShared;
extern AggregateType* dtString;
extern AggregateType* dtTaskBundleRecord;
extern AggregateType* dtTuple;

// these are only used when the dyno resolver is active
extern AggregateType* dtCPointer;
extern AggregateType* dtCPointerConst;
extern AggregateType* dtHeapBuffer;

extern Type* dt_c_int;
extern Type* dt_c_uint;
extern Type* dt_c_long;
extern Type* dt_c_ulong;
extern Type* dt_c_longlong;
extern Type* dt_c_ulonglong;
extern Type* dt_c_char;
extern Type* dt_c_schar;
extern Type* dt_c_uchar;
extern Type* dt_c_short;
extern Type* dt_c_ushort;
extern Type* dt_c_intptr;
extern Type* dt_c_uintptr;
extern Type* dt_c_ptrdiff;
extern Type* dt_ssize_t;
extern Type* dt_size_t;
extern Type* dt_wchar;

// The well-known functions
extern FnSymbol *gChplHereAlloc;
extern FnSymbol *gChplHereFree;
extern FnSymbol *gChplDecRunningTask;
extern FnSymbol *gChplIncRunningTask;
extern FnSymbol *gChplDoDirectExecuteOn;
extern FnSymbol *gBuildTupleType;
extern FnSymbol *gBuildTupleTypeNoRef;
extern FnSymbol *gBuildStarTupleType;
extern FnSymbol *gBuildStarTupleTypeNoRef;
extern FnSymbol *gPrintModuleInitFn;
extern FnSymbol *gGetDynamicEndCount;
extern FnSymbol *gSetDynamicEndCount;
extern FnSymbol *gChplDeleteError;
extern FnSymbol *gChplUncaughtError;
extern FnSymbol *gChplPropagateError;
extern FnSymbol *gChplSaveTaskError;
extern FnSymbol *gChplForallError;
extern FnSymbol *gAtomicFenceFn;
extern FnSymbol *gChplAfterForallFence;
extern FnSymbol *gAllocateStringLiteralsBuf;
extern FnSymbol *gChplCreateStringWithLiteral;
extern FnSymbol *gChplCreateBytesWithLiteral;
extern FnSymbol *gChplBuildLocaleId;

#endif
