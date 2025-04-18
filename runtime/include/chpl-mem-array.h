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

#ifndef _chpl_mem_array_H_
#define _chpl_mem_array_H_

#ifndef LAUNCHER

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "chpl-comm.h"
#include "chpl-gpu.h"
#include "chpl-mem.h"
#include "chpl-mem-desc.h"
#include "chpl-mem-hook.h"
#include "chpl-topo.h"
#include "chpltypes.h"
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline
chpl_bool chpl_mem_size_justifies_comm_alloc(size_t size) {
  //
  // Don't try to use comm layer allocation unless the size is beyond
  // the comm layer threshold and enough pages to make localization
  // worthwhile.
  //
  return (size >= chpl_comm_regMemAllocThreshold()
          && size >= 2 * chpl_comm_regMemHeapPageSize());
}


static inline
void* chpl_mem_array_alloc(size_t nmemb, size_t eltSize,
                           c_sublocid_t subloc, chpl_bool* callPostAlloc,
                           chpl_bool haltOnOom,
                           int32_t lineno, int32_t filename) {
  void* p = NULL;
  const size_t size = nmemb * eltSize;
#if defined(HAS_GPU_LOCALE) && !defined(GPU_RUNTIME_CPU)
  if (chpl_gpu_running_on_gpu_locale()) {
    *callPostAlloc = false;
    p = chpl_gpu_mem_array_alloc(size, CHPL_RT_MD_ARRAY_ELEMENTS,
                                 lineno, filename);
  }
  else {
#endif
  //
  // To support dynamic array registration by comm layers, in addition
  // to the address to the allocated memory this returns either true or
  // false in *callPostAlloc.  If we set *callPostAlloc==false then
  // allocation is complete when we return.  But if *callPostAlloc==true
  // then after initializing (first-touching) the memory, our caller
  // needs to call chpl_mem_array_postAlloc() with the allocated address
  // and the original nmemb and eltSize arguments.  At that point we will
  // call the comm layer post-alloc function, which typically does the
  // actual registration.  This is how we get NUMA locality correct on
  // registered memory, when that is possible.
  //
  chpl_memhook_malloc_pre(nmemb, eltSize, CHPL_RT_MD_ARRAY_ELEMENTS,
                          lineno, filename);

  *callPostAlloc = false;
  if (chpl_mem_size_justifies_comm_alloc(size)) {
    p = chpl_comm_regMemAlloc(size, CHPL_RT_MD_ARRAY_ELEMENTS,
                              lineno, filename);
    if (p != NULL) {
      *callPostAlloc = true;
    }
  }

  if (p == NULL) {
    p = chpl_malloc(size);
  }

  if (haltOnOom) {
    chpl_memhook_malloc_post(p, nmemb, eltSize, CHPL_RT_MD_ARRAY_ELEMENTS,
                             lineno, filename);
  } else if (p != NULL && CHPL_MEMHOOKS_ACTIVE) {
    chpl_track_malloc(p, nmemb, eltSize, CHPL_RT_MD_ARRAY_ELEMENTS,
                      lineno, filename);
  }
#if defined(HAS_GPU_LOCALE) && !defined(GPU_RUNTIME_CPU)
  }
#endif
  return p;
}

static inline
void chpl_mem_array_postAlloc(void* p, size_t nmemb, size_t eltSize,
                              int32_t lineno, int32_t filename) {
  //
  // Do comm layer post-allocation.
  //
  const size_t size = nmemb * eltSize;
  chpl_comm_regMemPostAlloc(p, size);
}


static inline
chpl_bool chpl_mem_array_supports_realloc(void* p, size_t oldNmemb, size_t newNmemb,
                                          size_t eltSize, int32_t lineno,
                                          int32_t filename) {
  const size_t oldSize = oldNmemb * eltSize;
  const size_t newSize = newNmemb * eltSize;
  return chpl_mem_size_justifies_comm_alloc(oldSize) == chpl_mem_size_justifies_comm_alloc(newSize);
}

static inline
void* chpl_mem_array_realloc(void* p, size_t oldNmemb, size_t newNmemb,
                             size_t eltSize,
                             c_sublocid_t subloc, chpl_bool* callPostAlloc,
                             int32_t lineno, int32_t filename) {
  void* newp = NULL;
  intptr_t oldp = (intptr_t) p;
  const size_t newSize = newNmemb * eltSize;
#ifdef HAS_GPU_LOCALE
  if (chpl_gpu_running_on_gpu_locale()) {
    *callPostAlloc = false;
    newp = chpl_gpu_mem_realloc(p, newSize, CHPL_RT_MD_ARRAY_ELEMENTS,
                                lineno, filename);
  }
  else {
#endif

  //
  // Support for dynamic array registration by comm layers is done here
  // via *callPostAlloc, the same as for chpl_mem_array_alloc().  See
  // there for further information.

  chpl_memhook_realloc_pre(p, newSize, CHPL_RT_MD_ARRAY_ELEMENTS,
                           lineno, filename);

  const size_t oldSize = oldNmemb * eltSize;
  *callPostAlloc = false;
  if (chpl_mem_size_justifies_comm_alloc(oldSize)) {
    newp = chpl_comm_regMemRealloc(p, oldSize, newSize,
                                   CHPL_RT_MD_ARRAY_ELEMENTS,
                                   lineno, filename);
    if (newp != NULL) {
      *callPostAlloc = true;
    }
  }

  if (newp == NULL) {
    newp = chpl_realloc(p, newSize);
  }

  chpl_memhook_realloc_post(newp, oldp,
                            newSize, CHPL_RT_MD_ARRAY_ELEMENTS,
                            lineno, filename);
#ifdef HAS_GPU_LOCALE
  }
#endif
  return newp;
}


static inline
void chpl_mem_array_postRealloc(void* oldp, size_t oldNmemb,
                                void* newp, size_t newNmemb,
                                size_t eltSize,
                                int32_t lineno, int32_t filename) {
  //
  // Do comm layer post-allocation.
  //
  const size_t oldSize = oldNmemb * eltSize;
  const size_t newSize = newNmemb * eltSize;
  chpl_comm_regMemPostRealloc(oldp, oldSize, newp, newSize);
}


static inline
void chpl_mem_array_free(void* p,
                         size_t nmemb, size_t eltSize, c_sublocid_t subloc,
                         int32_t lineno, int32_t filename) {
#ifdef HAS_GPU_LOCALE
  if (subloc >= 0) {
    chpl_gpu_mem_free(p, lineno, filename);
  }
  else {
#endif
  //
  // If the size indicates we might have gotten this memory from the
  // comm layer then try to free it there.  If not, or if so but the
  // comm layer says it didn't come from there, free it in the memory
  // layer.
  //
  const size_t size = nmemb * eltSize;
  chpl_memhook_free_pre(p, size, lineno, filename);

  if (chpl_mem_size_justifies_comm_alloc(size)
      && chpl_comm_regMemFree(p, size)) {
    return;
  }

  chpl_free(p);
#ifdef HAS_GPU_LOCALE
  }
#endif
}

#ifdef __cplusplus
}
#endif

#endif // LAUNCHER

#endif
