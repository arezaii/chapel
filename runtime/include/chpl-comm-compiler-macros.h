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

#ifndef _chpl_comm_compiler_macros_h_
#define _chpl_comm_compiler_macros_h_

#ifndef LAUNCHER

#include "chpl-comm.h"
#include "chpl-mem.h"
#include "error.h"
#include "chpl-wide-ptr-fns.h"

#include "chpl-prefetch.h" // for chpl_prefetch
#include "chpl-cache.h" // chpl_cache_enabled, chpl_cache_comm_get etc
#include "chpl-gpu.h" // for comm between device and host

// Don't warn about chpl_comm_get e.g. in this file.
#include "chpl-comm-no-warning-macros.h"

#ifdef __cplusplus
extern "C" {
#endif

//
// Multi-locale macros used for compiler code generation
//
// Note: Macros starting with CHPL_COMM involve some kind of communication
//

#define CHPL_COMM_UNKNOWN_ID -1


static ___always_inline
void chpl_gen_comm_get(void *addr, c_nodeid_t node, void* raddr,
                       size_t size, int32_t commID, int ln, int32_t fn)
{
  if (chpl_nodeID == node) {
    memmove(addr, raddr, size);
#ifdef HAS_CHPL_CACHE_FNS
  } else if( chpl_cache_enabled() ) {
    chpl_cache_comm_get(addr, node, raddr, size, commID, ln, fn);
#endif
  } else {
    chpl_comm_get(addr, node, raddr, size, commID, ln, fn);
  }
}
#ifdef HAS_GPU_LOCALE
static inline
void chpl_gen_comm_get_from_subloc(void *addr, c_nodeid_t src_node,
                                   c_sublocid_t src_subloc, void* raddr,
                                   size_t size, int32_t commID, int ln,
                                   int32_t fn)
{
  c_sublocid_t dst_subloc = chpl_task_getRequestedSubloc();

  if (chpl_nodeID == src_node) {
    chpl_gpu_memcpy(dst_subloc, addr, src_subloc, raddr, size, commID, ln, fn);
  } else if (dst_subloc >= 0 || src_subloc >= 0) {
    chpl_gpu_comm_get(dst_subloc, addr, src_node, src_subloc, raddr, size,
                      commID, ln, fn);
#ifdef HAS_CHPL_CACHE_FNS
  } else if( chpl_cache_enabled() ) {
    chpl_cache_comm_get(addr, src_node, raddr, size, commID, ln, fn);
#endif
  } else {
    chpl_comm_get(addr, src_node, raddr, size, commID, ln, fn);
  }
}
#endif // HAS_GPU_LOCALE

static inline
void chpl_gen_comm_prefetch(c_nodeid_t node, void* raddr,
                            size_t size, int32_t commID, int ln, int32_t fn)
{
  const size_t MAX_BYTES_LOCAL_PREFETCH = 1024;
  size_t offset;

  if (chpl_nodeID == node) {
    // Prefetch only the first part since we don't want to blow
    // out a cache...
    for( offset = 0;
         offset < size && offset < MAX_BYTES_LOCAL_PREFETCH;
         offset += 64 ) {
      chpl_prefetch((unsigned char*)raddr + offset);
    }
#ifdef HAS_CHPL_CACHE_FNS
  } else if( chpl_cache_enabled() ) {
    chpl_cache_comm_prefetch(node, raddr, size, commID, ln, fn);
#endif
  } else {
    // Can't do anything if we don't have a remote data cache
    // and the data is remote.
  }
}


static ___always_inline
void chpl_gen_comm_put(void* addr, c_nodeid_t node, void* raddr,
                       size_t size, int32_t commID, int ln, int32_t fn)
{
  if (chpl_nodeID == node) {
    memmove(raddr, addr, size);
#ifdef HAS_CHPL_CACHE_FNS
  } else if( chpl_cache_enabled() ) {
    chpl_cache_comm_put(addr, node, raddr, size, commID, ln, fn);
#endif
  } else {
    chpl_comm_put(addr, node, raddr, size, commID, ln, fn);
  }
}
#ifdef HAS_GPU_LOCALE
static inline
void chpl_gen_comm_put_to_subloc(void* addr,
                                 c_nodeid_t dst_node, c_sublocid_t dst_subloc,
                                 void* raddr, size_t size, int32_t commID,
                                 int ln, int32_t fn)
{

  c_sublocid_t src_subloc = chpl_task_getRequestedSubloc();

  if (chpl_nodeID == dst_node) {
    chpl_gpu_memcpy(dst_subloc, raddr, src_subloc, addr, size, commID, ln, fn);
  } else if (dst_subloc >= 0 || src_subloc >= 0) {
    chpl_gpu_comm_put(dst_node, dst_subloc, raddr, src_subloc, addr, size,
                      commID, ln, fn);
#ifdef HAS_CHPL_CACHE_FNS
  } else if( chpl_cache_enabled() ) {
      chpl_cache_comm_put(addr, dst_node, raddr, size, commID, ln, fn);
#endif
  } else {
      chpl_comm_put(addr, dst_node, raddr, size, commID, ln, fn);
  }
}
#endif // HAS_GPU_LOCALE

static inline
void chpl_gen_comm_get_strd(void *addr, void *dststr, c_nodeid_t node, c_sublocid_t src_subloc, void *raddr,
                       void *srcstr, void *count, int32_t strlevels,
                       size_t elemSize, int32_t commID, int ln, int32_t fn)
{
#ifdef HAS_GPU_LOCALE
  c_sublocid_t dst_subloc = chpl_task_getRequestedSubloc();

  if (dst_subloc >= 0 || src_subloc >= 0) {
    chpl_gpu_comm_get_strd(dst_subloc, addr, (size_t*)dststr,
                           node, src_subloc, raddr, (size_t*)srcstr,
                           (size_t*)count, strlevels, elemSize,
                           commID, ln, fn);
#else
  if( 0 ) {
#endif // HAS_GPU_LOCALE
#ifdef HAS_CHPL_CACHE_FNS
  } else if( chpl_cache_enabled() ) {
    chpl_cache_comm_get_strd(addr, (size_t*)dststr, node, raddr, (size_t*)srcstr, (size_t*)count, strlevels, elemSize, commID, ln, fn);
#endif
  } else {
    chpl_comm_get_strd(addr, (size_t*)dststr, node, raddr, (size_t*)srcstr, (size_t*)count, strlevels, elemSize, commID, ln, fn);
  }
}

static inline
void chpl_gen_comm_put_strd(void *addr, void *dststr, c_nodeid_t node, c_sublocid_t dst_subloc, void *raddr,
                       void *srcstr, void *count, int32_t strlevels,
                       size_t elemSize, int32_t commID, int ln, int32_t fn)
{
#ifdef HAS_GPU_LOCALE
  c_sublocid_t src_subloc = chpl_task_getRequestedSubloc();

  if (dst_subloc >= 0 || src_subloc >= 0) {
    chpl_gpu_comm_put_strd(src_subloc, addr, (size_t*)dststr,
                           node, dst_subloc, raddr, (size_t*)srcstr,
                           (size_t*)count, strlevels, elemSize,
                           commID, ln, fn);
#else
  if( 0 ) {
#endif // HAS_GPU_LOCALE
#ifdef HAS_CHPL_CACHE_FNS
  } else if( chpl_cache_enabled() ) {
    chpl_cache_comm_put_strd(addr, (size_t*)dststr, node, raddr, (size_t*)srcstr, (size_t*)count, strlevels, elemSize, commID, ln, fn);
#endif
  } else {
    chpl_comm_put_strd(addr, (size_t*)dststr, node, raddr, (size_t*)srcstr, (size_t*)count, strlevels, elemSize, commID, ln, fn);
  }
}


static inline
void chpl_gen_comm_get_unordered(void *addr, c_nodeid_t node, void* raddr,
                                 size_t size, int32_t commID, int ln, int32_t fn)
{
  if (0) {
#ifdef HAS_CHPL_CACHE_FNS
  } else if( chpl_cache_enabled() ) {
    chpl_cache_comm_get_unordered(addr, node, raddr, size, commID, ln, fn);
#endif
  } else {
    chpl_comm_get_unordered(addr, node, raddr, size, commID, ln, fn);
  }
}

static inline
void chpl_gen_comm_put_unordered(void* addr, c_nodeid_t node, void* raddr,
                                 size_t size, int32_t commID, int ln, int32_t fn)
{
  if (0) {
#ifdef HAS_CHPL_CACHE_FNS
  } else if( chpl_cache_enabled() ) {
    chpl_cache_comm_put_unordered(addr, node, raddr, size, commID, ln, fn);
#endif
  } else {
    chpl_comm_put_unordered(addr, node, raddr, size, commID, ln, fn);
  }
}

static inline
void chpl_gen_comm_getput_unordered(c_nodeid_t dstnode, void* dstaddr,
                                    c_nodeid_t srcnode, void* srcaddr,
                                    size_t size, int32_t commID,
                                    int ln, int32_t fn)
{
  if (0) {
#ifdef HAS_CHPL_CACHE_FNS
  } else if( chpl_cache_enabled() ) {
    chpl_cache_comm_getput_unordered(dstnode, dstaddr, srcnode, srcaddr, size, commID, ln, fn);
#endif
  } else {
    chpl_comm_getput_unordered(dstnode, dstaddr, srcnode, srcaddr, size, commID, ln, fn);
  }
}

static inline
void chpl_gen_comm_getput_unordered_task_fence(void)
{
  if (0) {
#ifdef HAS_CHPL_CACHE_FNS
  } else if( chpl_cache_enabled() ) {
    chpl_cache_comm_getput_unordered_task_fence();
#endif
  } else {
    chpl_comm_getput_unordered_task_fence();
  }
}

// Returns true if the given node ID matches the ID of the currently node,
// false otherwise.
static inline
chpl_bool chpl_is_node_local(c_nodeid_t node)
{ return node == chpl_nodeID; }

#ifdef HAS_GPU_LOCALE
static inline
chpl_bool chpl_is_local(chpl_localeID_t loc)
{
  return loc.node == chpl_nodeID &&
         loc.subloc == chpl_task_getRequestedSubloc();
}
#else
static inline
chpl_bool chpl_is_local(chpl_localeID_t loc)
{
  return loc.node == chpl_nodeID;
}

#endif

// Assert that the given node ID matches that of the currently-running image
// If not, format the given error message with the given filename and line number
// and then halt the current task.  (The exact behavior is dictated by
// chpl_error()).
static inline
void chpl_check_local(c_nodeid_t node, int32_t ln, int32_t file, const char* error)
{
  if (! chpl_is_node_local(node))
    chpl_error(error, ln, file);
}

static inline
void chpl_check_nil(const void* ptr, int32_t lineno, int32_t filename)
{
  if (ptr == nil)
    chpl_error("attempt to dereference nil", lineno, filename);
}

#ifdef __cplusplus
}
#endif

// Include LLVM support functions for --llvm-wide-opt
#include "chpl-comm-compiler-llvm-support.h"

#endif // LAUNCHER

// Turn warnings for chpl_comm_get back on
#include "chpl-comm-warning-macros.h"

#endif
