/*   $Source: bitbucket.org:berkeleylab/gasnet.git/other/kinds/gasnet_mk.h $
 * Description: GASNet Memory Kinds API types and declarations
 * Copyright (c) 2020, The Regents of the University of California
 * Terms of use are as specified in license.txt
 */

#ifndef _GASNET_MK_H
#define _GASNET_MK_H

#if defined(_INCLUDED_GASNET_INTERNAL_H) && !defined(_IN_GASNET_INTERNAL_H)
  #error Internal GASNet code should not directly include gasnet_mk.h, just gasnet_internal.h
#endif

#include <gasnetex.h>

GASNETI_BEGIN_EXTERNC
GASNETI_BEGIN_NOWARN

typedef enum {
    GEX_MK_CLASS_HOST,      // "normal" memory (eg GEX_MK_HOST)
    GEX_MK_CLASS_CUDA_UVA,  // CUDA UVA memory
    GEX_MK_CLASS_HIP,       // HIP device memory
    GEX_MK_CLASS_ZE,        // Level Zero device memory
    _GEX_MK_CLASS_COUNT
} gex_MK_Class_t;

// Struct containing a union and an enum to indicate which member has been populated.
// Each union member is a struct named with the lowercase of the enum identifier.
// All types in here are basic types, possibly type-erased/indirected versions of types
// provided in device headers.
typedef struct {
    uint64_t            gex_flags; // Reserved.  Must be 0 currently.
    gex_MK_Class_t      gex_class;
    union {
        struct {
            int                    gex_CUdevice;
        }                    gex_class_cuda_uva;
        struct {
            int                    gex_hipDevice;
        }                    gex_class_hip;
        struct {
            void*                  gex_zeDevice;
            void*                  gex_zeContext;
            uint32_t               gex_zeMemoryOrdinal;
        }                    gex_class_ze;
    }                    gex_args;
} gex_MK_Create_args_t;

// Constructor for gex_MK_t
// This is a non-collective call
extern int gex_MK_Create(
            gex_MK_t                         *_memkind_p, // OUT
            gex_Client_t                     _client,
            const gex_MK_Create_args_t       *_args,      // IN
            gex_Flags_t                      _flags       // Reserved.  Must be 0 currently.
            );

// Destructor
extern void gex_MK_Destroy(gex_MK_t, gex_Flags_t);

// Per-class impl(ementation) constants and function pointers
struct gasneti_mk_impl_s;
typedef struct gasneti_mk_impl_s gasneti_mk_impl_t;

#ifndef _GEX_MK_T
  #define GASNETI_MK_COMMON        \
    GASNETI_OBJECT_HEADER               \
    gasneti_Client_t       _client;     \
    gex_MK_Class_t         _mk_class;   \
    gasneti_mk_impl_t     *_mk_impl;    \
    void                  *_mk_conduit; \
    gasneti_weakatomic32_t _ref_count;
  typedef struct { GASNETI_MK_COMMON } *gasneti_MK_t;
  #if GASNET_DEBUG
    extern gasneti_MK_t gasneti_import_mk(gex_MK_t _mk);
    extern gasneti_MK_t gasneti_import_mk_nonhost(gex_MK_t _mk);
    extern gex_MK_t gasneti_export_mk(gasneti_MK_t _real_mk);
  #else
    #define gasneti_import_mk(x) ((gasneti_MK_t)(x))
    #define gasneti_import_mk_nonhost(x) ((gasneti_MK_t)(x))
    #define gasneti_export_mk(x) ((gex_MK_t)(x))
  #endif
  // TODO: Either document the following, prohibiting GEX_MK_HOST, or fix them for that case
  #define gex_MK_SetCData(mk,val)    ((void)(gasneti_import_mk_nonhost(mk)->_cdata = (val)))
  #define gex_MK_QueryCData(mk)      ((void*)gasneti_import_mk_nonhost(mk)->_cdata)
  #define gex_MK_QueryFlags(mk)      ((gex_Flags_t)gasneti_import_mk_nonhost(mk)->_flags)
  #define gex_MK_QueryClient(mk)     gasneti_export_client(gasneti_import_mk_nonhost(mk)->_client)
  #define gex_MK_QueryClass(mk)      ((gex_MK_Class_t)gasneti_import_mk_nonhost(mk)->_mk_class)
#endif

GASNETI_END_NOWARN
GASNETI_END_EXTERNC

#if GASNET_HAVE_MK_CLASS_HIP
  // HIP platform was determined at GASNet-EX configure time.
  // If these conflict with client code, then this is not the right GASNet-EX build
  #if GASNETI_HIP_PLATFORM_NVIDIA
    #if defined(__HIP_PLATFORM_AMD__) || defined(__HIP_PLATFORM_HCC__)
    #error Conflicting HIP platform at GASNet configure vs gasnet_mk.h compile
    #endif
    #ifndef __HIP_PLATFORM_NVCC__
    #define __HIP_PLATFORM_NVCC__ // legacy
    #endif
    #ifndef __HIP_PLATFORM_NVIDIA__
    #define __HIP_PLATFORM_NVIDIA__
    #endif
  #else
    #if defined(__HIP_PLATFORM_NVIDIA__) || defined(__HIP_PLATFORM_NVCC__)
    #error Conflicting HIP platform at GASNet configure vs gasnet_mk.h compile
    #endif
    #ifndef __HIP_PLATFORM_HCC__
    #define __HIP_PLATFORM_HCC__ // legacy
    #endif
    #ifndef __HIP_PLATFORM_AMD__
    #define __HIP_PLATFORM_AMD__
    #endif
  #endif
#endif

// Things for use only in conduit code
#if GASNETI_BUILDING_CONDUIT
  #if GASNET_HAVE_MK_CLASS_ZE
    extern int gasneti_mk_ze_device_ordinal(void *device_handle_arg, int gpu_only);
    extern const char *gasneti_mk_ze_strerror(unsigned int result);
  #endif
#endif

#endif
