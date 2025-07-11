#!/usr/bin/env python3

"""Usage: printchplenv [options]

Print the current Chapel configuration. Arguments allow selection of
what gets printed [content], how it gets printed [format], and what gets
filtered [filter].

The default [content] provides user-facing variables.

Options:
  -h, --help       Show this help message and exit

  [shortcut]
  --all            Shortcut for --compiler --runtime --launcher, includes defaults

  [content]
  --compiler       Select variables describing the configuration of the compiler
  --runtime        Select variables describing the configuration of the runtime
  --launcher       Select variables describing the configuration of the launcher
  --internal       Select additional variables used during builds
                    this flag is incompatible with [format]: --path

  [filter]
  --[no-]tidy      (default) [don't] Omit sub-variables irrelevant to the current
                    configuration
  --anonymize      Omit machine specific details, script location, and CHPL_HOME
  --overrides      Omit variables that have not been user supplied via environment
                    or chplconfig
  --only-path      Omit variables that do not contibute to the build path
  --only=VARS      Print only the variables listed in VARS, separated by commas.

  [format]
  --pretty         (default) Print variables in format: CHPL_KEY: VALUE
                    indicating which options are set by environment variables (*)
                    and which are set by configuration files (+)
  --simple         Print variables in format: CHPL_KEY=VALUE
                    output is compatible with chplconfig format
  --cmake          Print variables in format: CHPL_KEY VALUE with quotes stripped
                    from values. Output is compatible with cmake format
  --make           Print variables in format: CHPL_MAKE_KEY=VALUE
  --path           Print variables in format: VALUE1/VALUE2/...
                    this flag always excludes CHPL_HOME and CHPL_MAKE
  --bash           Print variables in format: export CHPL_KEY=VALUE
  --csh            Print variables in format: setenv CHPL_KEY VALUE
  --value          Print only the values of the variables

  [misc]
  --ignore-errors  Continue processing even if an error occurs
   --verify        Run verification tests on the current Chapel configuration
"""

from collections import namedtuple
from functools import partial
import optparse
import os
import re
import unittest
from sys import stdout, path
import textwrap

from chplenv import *
import chplenv_verify

ChapelEnv = namedtuple('ChapelEnv', ['name', 'content', 'shortname'])

# Set default argument of 'shortname' to None
ChapelEnv.__new__.__defaults__ = (None,)

# Define base sets
COMPILER = set(['compiler'])
LAUNCHER = set(['launcher'])
RUNTIME = set(['runtime'])
INTERNAL = set(['internal'])
NOPATH = set(['nopath'])     # for variables to be skipped for --path or --only-path
DEFAULT = set(['default'])

# Global ordered list that stores names, content-categories, and shortnames
CHPL_ENVS = [
    ChapelEnv('CHPL_HOST_PLATFORM', COMPILER | LAUNCHER),
    ChapelEnv('CHPL_HOST_COMPILER', COMPILER | LAUNCHER),
    ChapelEnv('  CHPL_HOST_CC', COMPILER | NOPATH),
    ChapelEnv('  CHPL_HOST_CXX', COMPILER | NOPATH),
    ChapelEnv('  CHPL_HOST_BUNDLED_COMPILE_ARGS', INTERNAL),
    ChapelEnv('  CHPL_HOST_SYSTEM_COMPILE_ARGS', INTERNAL),
    ChapelEnv('  CHPL_HOST_BUNDLED_LINK_ARGS', INTERNAL),
    ChapelEnv('  CHPL_HOST_SYSTEM_LINK_ARGS', INTERNAL),
    ChapelEnv('CHPL_HOST_ARCH', COMPILER | LAUNCHER),
    ChapelEnv('CHPL_HOST_CPU', INTERNAL),
    ChapelEnv('CHPL_TARGET_PLATFORM', RUNTIME | DEFAULT),
    ChapelEnv('CHPL_TARGET_COMPILER', RUNTIME | DEFAULT),
    ChapelEnv('  CHPL_TARGET_CC', RUNTIME | NOPATH),
    ChapelEnv('  CHPL_TARGET_CXX', RUNTIME | NOPATH),
    ChapelEnv('  CHPL_TARGET_COMPILER_PRGENV', INTERNAL),
    ChapelEnv('  CHPL_TARGET_BUNDLED_COMPILE_ARGS', INTERNAL),
    ChapelEnv('  CHPL_TARGET_SYSTEM_COMPILE_ARGS', INTERNAL),
    ChapelEnv('  CHPL_TARGET_LD', RUNTIME | NOPATH),
    ChapelEnv('  CHPL_TARGET_BUNDLED_LINK_ARGS', INTERNAL),
    ChapelEnv('  CHPL_TARGET_SYSTEM_LINK_ARGS', INTERNAL),
    ChapelEnv('CHPL_TARGET_ARCH', RUNTIME | DEFAULT),
    ChapelEnv('CHPL_TARGET_CPU', RUNTIME | DEFAULT, 'cpu'),
    ChapelEnv('CHPL_RUNTIME_CPU', INTERNAL),
    ChapelEnv('CHPL_TARGET_CPU_FLAG', INTERNAL),
    ChapelEnv('CHPL_TARGET_BACKEND_CPU', INTERNAL),
    ChapelEnv('CHPL_LOCALE_MODEL', RUNTIME | LAUNCHER | DEFAULT, 'loc'),
    ChapelEnv('  CHPL_GPU', RUNTIME | DEFAULT, 'gpu'),
    ChapelEnv('  CHPL_GPU_SDK_VERSION', RUNTIME, 'gpu_vers'),
    ChapelEnv('  CHPL_GPU_ARCH', INTERNAL),
    ChapelEnv('  CHPL_GPU_MEM_STRATEGY', RUNTIME , 'gpu_mem'),
    ChapelEnv('  CHPL_CUDA_PATH', INTERNAL),
    ChapelEnv('  CHPL_ROCM_PATH', INTERNAL),
    ChapelEnv('  CHPL_CUDA_LIBDEVICE_PATH', INTERNAL),
    ChapelEnv('  CHPL_ROCM_LLVM_PATH', INTERNAL),
    ChapelEnv('  CHPL_ROCM_AMDGCN_PATH', INTERNAL),
    ChapelEnv('CHPL_COMM', RUNTIME | LAUNCHER | DEFAULT, 'comm'),
    ChapelEnv('  CHPL_COMM_SUBSTRATE', RUNTIME | LAUNCHER | DEFAULT),
    ChapelEnv('  CHPL_GASNET_SEGMENT', RUNTIME | LAUNCHER | DEFAULT),
    ChapelEnv('  CHPL_LIBFABRIC', RUNTIME | INTERNAL | DEFAULT),
    ChapelEnv('  CHPL_COMM_OFI_OOB', RUNTIME | INTERNAL | DEFAULT),
    ChapelEnv('CHPL_TASKS', RUNTIME | LAUNCHER | DEFAULT, 'tasks'),
    ChapelEnv('CHPL_LAUNCHER', LAUNCHER | DEFAULT, 'launch'),
    ChapelEnv('CHPL_TIMERS', RUNTIME | LAUNCHER | DEFAULT, 'tmr'),
    ChapelEnv('CHPL_UNWIND', RUNTIME | LAUNCHER | DEFAULT, 'unwind'),
    ChapelEnv('CHPL_HOST_MEM', COMPILER, 'hostmem'),
    ChapelEnv('  CHPL_HOST_JEMALLOC', COMPILER | NOPATH, 'hostjemalloc'),
    ChapelEnv('  CHPL_HOST_MIMALLOC', COMPILER | NOPATH, 'hostmimalloc'),
    ChapelEnv('CHPL_MEM', INTERNAL, 'mem'), # deprecated and will be removed
    ChapelEnv('CHPL_TARGET_MEM', RUNTIME | LAUNCHER | DEFAULT, 'mem'),
    ChapelEnv('  CHPL_TARGET_JEMALLOC', RUNTIME | NOPATH, 'tgtjemalloc'),
    ChapelEnv('  CHPL_TARGET_MIMALLOC', RUNTIME | NOPATH, 'tgtmimalloc'),
    ChapelEnv('CHPL_MAKE', INTERNAL, 'make'),
    ChapelEnv('CHPL_ATOMICS', RUNTIME | LAUNCHER | DEFAULT, 'atomics'),
    ChapelEnv('  CHPL_NETWORK_ATOMICS', INTERNAL | DEFAULT),
    ChapelEnv('CHPL_GMP', INTERNAL | DEFAULT, 'gmp'),
    ChapelEnv('  CHPL_GMP_IS_OVERRIDDEN', INTERNAL),
    ChapelEnv('CHPL_HWLOC', RUNTIME | DEFAULT, 'hwloc'),
    ChapelEnv('  CHPL_HWLOC_PCI', RUNTIME | INTERNAL, 'pci'),
    ChapelEnv('CHPL_RE2', RUNTIME | DEFAULT, 're2'),
    ChapelEnv('  CHPL_RE2_IS_OVERRIDDEN', INTERNAL),
    ChapelEnv('CHPL_LLVM', COMPILER | DEFAULT, 'llvm'),
    ChapelEnv('  CHPL_LLVM_SUPPORT', COMPILER | NOPATH, 'llvm'),
    ChapelEnv('  CHPL_LLVM_CONFIG', COMPILER | NOPATH),
    ChapelEnv('  CHPL_LLVM_VERSION', COMPILER),
    ChapelEnv('  CHPL_LLVM_CLANG_C', INTERNAL),
    ChapelEnv('  CHPL_LLVM_CLANG_CXX', INTERNAL),
    ChapelEnv('  CHPL_LLVM_STATIC_DYNAMIC', INTERNAL),
    ChapelEnv('  CHPL_LLVM_BIN_DIR', INTERNAL),
    ChapelEnv('  CHPL_LLVM_TARGET_CPU', INTERNAL),
    ChapelEnv('CHPL_AUX_FILESYS', RUNTIME | DEFAULT, 'fs'),
    ChapelEnv('CHPL_LIB_PIC', RUNTIME | LAUNCHER, 'lib_pic'),
    ChapelEnv('CHPL_SANITIZE', COMPILER | LAUNCHER, 'san'),
    ChapelEnv('CHPL_SANITIZE_EXE', RUNTIME, 'san'),
    ChapelEnv('CHPL_RUNTIME_SUBDIR', INTERNAL),
    ChapelEnv('CHPL_LAUNCHER_SUBDIR', INTERNAL),
    ChapelEnv('CHPL_COMPILER_SUBDIR', INTERNAL),
    ChapelEnv('CHPL_HOST_BIN_SUBDIR', INTERNAL),
    ChapelEnv('CHPL_TARGET_BIN_SUBDIR', INTERNAL),
    ChapelEnv('CHPL_SYS_MODULES_SUBDIR', INTERNAL),
    ChapelEnv('  CHPL_LLVM_UNIQ_CFG_PATH', INTERNAL),
    ChapelEnv('  CHPL_GASNET_UNIQ_CFG_PATH', INTERNAL),
    ChapelEnv('  CHPL_GMP_UNIQ_CFG_PATH', INTERNAL),
    ChapelEnv('  CHPL_HWLOC_UNIQ_CFG_PATH', INTERNAL),
    ChapelEnv('  CHPL_HOST_JEMALLOC_UNIQ_CFG_PATH', INTERNAL),
    ChapelEnv('  CHPL_TARGET_JEMALLOC_UNIQ_CFG_PATH', INTERNAL),
    ChapelEnv('  CHPL_HOST_MIMALLOC_UNIQ_CFG_PATH', INTERNAL),
    ChapelEnv('  CHPL_TARGET_MIMALLOC_UNIQ_CFG_PATH', INTERNAL),
    ChapelEnv('  CHPL_LIBFABRIC_UNIQ_CFG_PATH', INTERNAL),
    ChapelEnv('  CHPL_LIBUNWIND_UNIQ_CFG_PATH', INTERNAL),
    ChapelEnv('  CHPL_QTHREAD_UNIQ_CFG_PATH', INTERNAL),
    ChapelEnv('  CHPL_RE2_UNIQ_CFG_PATH', INTERNAL),
    ChapelEnv('  CHPL_PE_CHPL_PKGCONFIG_LIBS', INTERNAL),
]

# Global map of environment variable names to values
ENV_VALS = {}

"""Compute '--all' env var values and populate global dict, ENV_VALS"""
def compute_all_values():
    global ENV_VALS

    ENV_VALS['CHPL_HOME'] = chpl_home_utils.get_chpl_home()
    ENV_VALS['CHPL_HOST_PLATFORM'] = chpl_platform.get('host')

    host_compiler = chpl_compiler.get('host')
    host_compiler_c = chpl_compiler.get_compiler_command('host', 'c')
    host_compiler_cpp = chpl_compiler.get_compiler_command('host', 'c++')
    ENV_VALS['CHPL_HOST_COMPILER'] = host_compiler
    ENV_VALS['  CHPL_HOST_CC'] = " ".join(host_compiler_c)
    ENV_VALS['  CHPL_HOST_CXX'] = " ".join(host_compiler_cpp)
    ENV_VALS['CHPL_HOST_ARCH'] = chpl_arch.get('host')
    ENV_VALS['CHPL_HOST_CPU'] = chpl_cpu.get('host').cpu
    ENV_VALS['CHPL_TARGET_PLATFORM'] = chpl_platform.get('target')

    target_compiler = chpl_compiler.get('target')
    target_compiler_c = chpl_compiler.get_compiler_command('target', 'c')
    target_compiler_cpp = chpl_compiler.get_compiler_command('target', 'c++')
    target_compiler_prgenv = chpl_compiler.get_prgenv_compiler()
    target_linker = compile_link_args_utils.get_target_link_command()
    ENV_VALS['CHPL_TARGET_COMPILER'] = target_compiler
    ENV_VALS['  CHPL_TARGET_CC'] = " ".join(target_compiler_c)
    ENV_VALS['  CHPL_TARGET_CXX'] = " ".join(target_compiler_cpp)
    ENV_VALS['  CHPL_TARGET_COMPILER_PRGENV'] = target_compiler_prgenv
    ENV_VALS['  CHPL_TARGET_LD'] = " ".join(target_linker)

    ENV_VALS['CHPL_TARGET_ARCH'] = chpl_arch.get('target')
    ENV_VALS['CHPL_TARGET_CPU'] = chpl_cpu.get('target').cpu

    # Use module's LCD architecture in case it was built before
    # Internal, but this value is used in place of CHPL_TARGET_CPU for --path
    ENV_VALS['CHPL_RUNTIME_CPU'] = chpl_cpu.get('target',
            get_lcd=chpl_home_utils.using_chapel_module()).cpu

    ENV_VALS['CHPL_LOCALE_MODEL'] = chpl_locale_model.get()
    ENV_VALS['  CHPL_GPU'] = chpl_gpu.get()
    ENV_VALS['  CHPL_GPU_SDK_VERSION'] = chpl_gpu.get_sdk_version()
    ENV_VALS['  CHPL_CUDA_LIBDEVICE_PATH'] = chpl_gpu.get_cuda_libdevice_path()
    ENV_VALS['  CHPL_ROCM_LLVM_PATH'] = chpl_gpu.get_rocm_llvm_path()
    ENV_VALS['  CHPL_ROCM_AMDGCN_PATH'] = chpl_gpu.get_rocm_amdgcn_path()
    ENV_VALS['  CHPL_GPU_MEM_STRATEGY'] = chpl_gpu.get_gpu_mem_strategy()
    ENV_VALS['CHPL_COMM'] = chpl_comm.get()
    ENV_VALS['  CHPL_COMM_SUBSTRATE'] = chpl_comm_substrate.get()
    ENV_VALS['  CHPL_GASNET_SEGMENT'] = chpl_comm_segment.get()
    ENV_VALS['  CHPL_LIBFABRIC'] = chpl_libfabric.get()
    ENV_VALS['  CHPL_COMM_OFI_OOB'] = chpl_comm_ofi_oob.get()
    ENV_VALS['CHPL_TASKS'] = chpl_tasks.get()
    ENV_VALS['CHPL_LAUNCHER'] = chpl_launcher.get()
    ENV_VALS['CHPL_TIMERS'] = chpl_timers.get()
    ENV_VALS['CHPL_UNWIND'] = chpl_unwind.get()
    ENV_VALS['CHPL_HOST_MEM'] = chpl_mem.get('host')
    ENV_VALS['CHPL_TARGET_MEM'] = chpl_mem.get('target')
    ENV_VALS['  CHPL_HOST_JEMALLOC'] = chpl_jemalloc.get('host')
    ENV_VALS['  CHPL_TARGET_JEMALLOC'] = chpl_jemalloc.get('target')
    ENV_VALS['  CHPL_HOST_MIMALLOC'] = chpl_mimalloc.get('host')
    ENV_VALS['  CHPL_TARGET_MIMALLOC'] = chpl_mimalloc.get('target')
    ENV_VALS['CHPL_MAKE'] = chpl_make.get()
    ENV_VALS['CHPL_ATOMICS'] = chpl_atomics.get()
    ENV_VALS['  CHPL_NETWORK_ATOMICS'] = chpl_atomics.get('network')
    ENV_VALS['CHPL_GMP'] = chpl_gmp.get()
    ENV_VALS['  CHPL_GMP_IS_OVERRIDDEN'] = chpl_gmp.is_overridden()
    ENV_VALS['CHPL_HWLOC'] = chpl_hwloc.get()
    ENV_VALS['  CHPL_HWLOC_PCI'] = chpl_hwloc_pci.get()
    ENV_VALS['CHPL_RE2'] = chpl_re2.get()
    ENV_VALS['  CHPL_RE2_IS_OVERRIDDEN'] = chpl_re2.is_overridden()
    ENV_VALS['CHPL_LLVM'] = chpl_llvm.get()
    ENV_VALS['  CHPL_LLVM_SUPPORT'] = chpl_llvm.get_llvm_support()
    ENV_VALS['  CHPL_LLVM_CONFIG'] = chpl_llvm.get_llvm_config()
    ENV_VALS['  CHPL_LLVM_VERSION'] = chpl_llvm.get_llvm_version()
    llvm_clang_c = chpl_llvm.get_llvm_clang('c')
    llvm_clang_cxx = chpl_llvm.get_llvm_clang('c++')
    ENV_VALS['  CHPL_LLVM_CLANG_C'] = " ".join(llvm_clang_c)
    ENV_VALS['  CHPL_LLVM_CLANG_CXX'] = " ".join(llvm_clang_cxx)
    ENV_VALS['  CHPL_LLVM_STATIC_DYNAMIC'] = chpl_llvm.get_static_dynamic()
    ENV_VALS['  CHPL_LLVM_BIN_DIR'] = chpl_llvm.get_llvm_config_bindir()
    aux_filesys = chpl_aux_filesys.get()
    ENV_VALS['CHPL_AUX_FILESYS'] = '_'.join(sorted(aux_filesys.split(' ')))
    ENV_VALS['CHPL_LIB_PIC'] = chpl_lib_pic.get()
    ENV_VALS['CHPL_SANITIZE'] = chpl_sanitizers.get()
    ENV_VALS['CHPL_SANITIZE_EXE'] = chpl_sanitizers.get('exe')

    # error checking that would be hard to do in the .get functions
    # due to circular dependencies
    chpl_arch.validate('host')
    chpl_arch.validate('target')
    chpl_llvm.validate_llvm_config()
    chpl_compiler.validate_compiler_settings()
    chpl_gpu.validate(ENV_VALS['CHPL_LOCALE_MODEL'])


"""Compute '--internal' env var values and populate global dict, ENV_VALS"""
def compute_internal_values():
    global ENV_VALS

    # Maps architecture name that Chapel uses to the name that can be included
    # in an argument like -march e.g. for gcc-4.7: 'ivybridge' -> 'core-avx-i'
    backend_info = chpl_cpu.get('target', map_to_compiler=True)
    ENV_VALS['CHPL_TARGET_CPU_FLAG'] = backend_info.flag
    ENV_VALS['CHPL_TARGET_BACKEND_CPU'] = backend_info.cpu
    ENV_VALS['  CHPL_LLVM_TARGET_CPU'] = chpl_cpu.get_llvm_target_cpu().cpu

    ENV_VALS['CHPL_MEM'] = chpl_mem.get('target')
    ENV_VALS['CHPL_RUNTIME_SUBDIR'] = printchplenv(set(['runtime']), print_format='path').rstrip('\n')
    ENV_VALS['CHPL_LAUNCHER_SUBDIR'] = printchplenv(set(['launcher']), print_format='path').rstrip('\n')
    ENV_VALS['CHPL_COMPILER_SUBDIR'] = printchplenv(set(['compiler']), print_format='path').rstrip('\n')
    ENV_VALS['CHPL_HOST_BIN_SUBDIR'] = chpl_bin_subdir.get('host')
    ENV_VALS['CHPL_TARGET_BIN_SUBDIR'] = chpl_bin_subdir.get('target')

    sys_modules_subdir = (chpl_platform.get('target') + "-" +
                          chpl_arch.get('target') + "-" +
                          chpl_compiler.get_path_component('target'))
    ENV_VALS['CHPL_SYS_MODULES_SUBDIR'] = sys_modules_subdir

    ENV_VALS['  CHPL_LLVM_UNIQ_CFG_PATH'] = chpl_llvm.get_uniq_cfg_path()

    ENV_VALS['  CHPL_GASNET_UNIQ_CFG_PATH'] = chpl_gasnet.get_uniq_cfg_path()

    ENV_VALS['  CHPL_GMP_UNIQ_CFG_PATH'] = chpl_gmp.get_uniq_cfg_path()

    ENV_VALS['  CHPL_HWLOC_UNIQ_CFG_PATH'] = chpl_hwloc.get_uniq_cfg_path()

    ENV_VALS['  CHPL_HOST_JEMALLOC_UNIQ_CFG_PATH'] = chpl_jemalloc.get_uniq_cfg_path('host')
    ENV_VALS['  CHPL_TARGET_JEMALLOC_UNIQ_CFG_PATH'] = chpl_jemalloc.get_uniq_cfg_path('target')
    ENV_VALS['  CHPL_HOST_MIMALLOC_UNIQ_CFG_PATH'] = chpl_mimalloc.get_uniq_cfg_path('host')
    ENV_VALS['  CHPL_TARGET_MIMALLOC_UNIQ_CFG_PATH'] = chpl_mimalloc.get_uniq_cfg_path('target')
    ENV_VALS['  CHPL_LIBFABRIC_UNIQ_CFG_PATH'] = chpl_libfabric.get_uniq_cfg_path()
    ENV_VALS['  CHPL_LIBUNWIND_UNIQ_CFG_PATH'] = chpl_unwind.get_uniq_cfg_path()

    ENV_VALS['  CHPL_QTHREAD_UNIQ_CFG_PATH'] = chpl_qthreads.get_uniq_cfg_path()

    ENV_VALS['  CHPL_RE2_UNIQ_CFG_PATH'] = chpl_re2.get_uniq_cfg_path()

    ENV_VALS['  CHPL_PE_CHPL_PKGCONFIG_LIBS'] = chpl_llvm.gather_pe_chpl_pkgconfig_libs()

    # fill in ENV_VALS['  CHPL_HOST_BUNDLED_COMPILE_ARGS'] etc
    r_subdir = ENV_VALS['CHPL_RUNTIME_SUBDIR']
    d = compile_link_args_utils.compute_internal_compile_link_args(r_subdir)
    host_compile = d['host_compile']
    host_link = d['host_link']
    tgt_compile = d['target_compile']
    tgt_link = d['target_link']

    ENV_VALS['  CHPL_HOST_BUNDLED_COMPILE_ARGS'] = " ".join(host_compile[0])
    ENV_VALS['  CHPL_HOST_SYSTEM_COMPILE_ARGS'] = " ".join(host_compile[1])
    ENV_VALS['  CHPL_HOST_BUNDLED_LINK_ARGS'] = " ".join(host_link[0])
    ENV_VALS['  CHPL_HOST_SYSTEM_LINK_ARGS'] = " ".join(host_link[1])

    ENV_VALS['  CHPL_TARGET_BUNDLED_COMPILE_ARGS'] = " ".join(tgt_compile[0])
    ENV_VALS['  CHPL_TARGET_SYSTEM_COMPILE_ARGS'] = " ".join(tgt_compile[1])
    ENV_VALS['  CHPL_TARGET_BUNDLED_LINK_ARGS'] = " ".join(tgt_link[0])
    ENV_VALS['  CHPL_TARGET_SYSTEM_LINK_ARGS'] = " ".join(tgt_link[1])

    ENV_VALS['  CHPL_GPU_ARCH'] = chpl_gpu.get_arch()
    ENV_VALS['  CHPL_CUDA_PATH'] = chpl_gpu.get_sdk_path("nvidia")
    ENV_VALS['  CHPL_ROCM_PATH'] = chpl_gpu.get_sdk_path("amd")



"""Return non-empty string if var is set via environment or chplconfig"""
def user_set(env):
    if env_set(env):
        return ' *'
    elif config_set(env):
        return ' +'
    return ''

def env_set(env):
    env_stripped = env.strip()
    var = overrides.get_environ(env_stripped, None)
    return var is not None

def config_set(env):
    env_stripped = env.strip()
    var = overrides.get_chplconfig(env_stripped, None)
    return var is not None

"""Filter out variables that are marked with NOPATH"""
def filter_path(chpl_env):
    return not 'nopath' in chpl_env.content

"""Filter variables that are not user set"""
def filter_overrides(chpl_env):
    return bool(user_set(chpl_env.name))


"""Filter variables irrelevant to configuration for --tidy flag"""
def filter_tidy(chpl_env):
    comm = ENV_VALS['CHPL_COMM']
    llvm = ENV_VALS['CHPL_LLVM']
    locale = ENV_VALS['CHPL_LOCALE_MODEL']
    gpu = ENV_VALS['  CHPL_GPU']
    host_mem = ENV_VALS['CHPL_HOST_MEM']
    tgt_mem = ENV_VALS['CHPL_TARGET_MEM']
    hwloc = ENV_VALS['CHPL_HWLOC']

    if chpl_env.name == '  CHPL_COMM_SUBSTRATE':
        return comm == 'gasnet'
    elif chpl_env.name == '  CHPL_GASNET_SEGMENT':
        return comm == 'gasnet'
    elif chpl_env.name == '  CHPL_LIBFABRIC':
        return comm == 'ofi'
    elif chpl_env.name == '  CHPL_COMM_OFI_OOB':
        return comm == 'ofi'
    elif chpl_env.name == '  CHPL_NETWORK_ATOMICS':
        return comm != 'none'
    elif chpl_env.name == '  CHPL_GPU':
        return locale == 'gpu'
    elif chpl_env.name == '  CHPL_GPU_MEM_STRATEGY':
        return gpu != 'none'
    elif chpl_env.name == '  CHPL_CUDA_PATH':
        return gpu == 'nvidia'
    elif chpl_env.name == '  CHPL_CUDA_LIBDEVICE_PATH':
        return gpu == 'nvidia'
    elif chpl_env.name == '  CHPL_ROCM_LLVM_PATH':
        return gpu == 'amd'
    elif chpl_env.name == '  CHPL_ROCM_AMDGCN_PATH':
        return gpu == 'amd'
    elif chpl_env.name == '  CHPL_ROCM_PATH':
        return gpu == 'amd'
    elif chpl_env.name == '  CHPL_GPU_ARCH':
        return gpu == 'nvidia' or gpu == 'amd'
    elif chpl_env.name == '  CHPL_GPU_SDK_VERSION':
        return gpu != 'none'
    elif chpl_env.name == '  CHPL_HOST_JEMALLOC':
        return host_mem == 'jemalloc'
    elif chpl_env.name == '  CHPL_TARGET_JEMALLOC':
        return tgt_mem == 'jemalloc'
    elif chpl_env.name == '  CHPL_HOST_MIMALLOC':
        return host_mem == 'mimalloc'
    elif chpl_env.name == '  CHPL_TARGET_MIMALLOC':
        return tgt_mem == 'mimalloc'
    elif chpl_env.name == '  CHPL_HWLOC_PCI':
        return hwloc == 'bundled'
    return True


"""Filter out all variables except the one requested"""
def _filter_only(chpl_env, only):
    return chpl_env.name.strip() in only

"""Filter variables that are not selected in contents
Requires a content argument via functools.partial
"""
def _filter_content(chpl_env, contents=None):
    return chpl_env.content.intersection(contents)


"""Quote and/or escape spaces and [some] special symbols in 'value',
for use in a shell.
"""
def forShell(value):
    # For simplicity, just wrap 'value' in single quotes, when needed.
    # TODO: also handle single quotes occurring in 'value'.
    # needEscapingRE is the RE that has the following symbols within []:
    # \ " SPACE \t \n \r \f \v ~ ` # $ & * | ; " < > ? ! ( ) [ ] { }
    needEscapingRE = "[\\\"" + \
      r" \t\n\r\f\v\~\`\#\$\&\*\|\;\"\<\>\?\!\(\)\[\]\{\}]"
    if re.search(needEscapingRE, value):
        return "'" + value + "'"
    else:
        return value


"""Return string to be printed for a given variable and print_format
Requires a print_format argument
"""
def _print_var(key, value, print_format=None, shortname=None, raw=False):
    key_stripped = key.strip()
    if print_format == 'pretty':
        user_set_symbol = user_set(key_stripped)
        return "{0}: {1}{2}\n".format(key, value, user_set_symbol)
    elif print_format == 'verbose':
        reason = " (inferred)"
        if env_set(key_stripped):
            reason = " (environment/cli)"
        elif config_set(key_stripped):
            reason = " (chplconfig)"
        return "{}: {}{}\n".format(key_stripped, value, reason)
    elif print_format == 'simple':
        return "{0}={1}\n".format(key_stripped, value)
    elif print_format == 'cmake':
        return "{0} {1}\n".format(key_stripped, value)
    elif print_format == 'make':
        make_key = key_stripped.replace("CHPL_", "CHPL_MAKE_", 1)
        return "{0}={1}\n".format(make_key, value)
    elif print_format == 'path':
        if shortname:
            ret = "{0}-{1}".format(shortname, value)
        else:
            ret = "{0}".format(value)
        if not raw:
            return ret + '/'
        else:
            return (key, value, shortname, ret)
    elif print_format == 'bash':
        return "export {0}={1}\n".format(key_stripped, forShell(value))
    elif print_format == 'csh':
        return "setenv {0} {1}\n".format(key_stripped, forShell(value))
    elif print_format == 'value':
        return "{0}\n".format(value)
    else:
        raise ValueError("Invalid format '{0}'".format(print_format))


"""Return a string that contains the Chapel configuration variable info"""
def printchplenv(contents, print_filters=None, print_format='pretty', only=None, raw=False):
    global CHPL_ENVS

    if print_filters is None:
        print_filters = set(['tidy'])

    # Error checking for external python codes calling printchplenv function
    if not ENV_VALS.items:
        raise KeyError('ENV_VALS must be populated with compute_*_values()'
                       'before printchplenv is called')

    # Specialize _filter_content to use contents as default arg
    filter_content = partial(_filter_content, contents=contents)

    envs = filter(filter_content, CHPL_ENVS)

    if only:
        envs = filter(partial(_filter_only, only=only), envs)
    else:
        # --path or --only-path -- skip variables marked NOPATH
        if print_format == 'path' or 'only-path' in print_filters:
            envs = filter(filter_path, envs)

        # --overrides
        if 'overrides' in print_filters:
            envs = filter(filter_overrides, envs)

        # --tidy
        if 'tidy' in print_filters:
            envs = filter(filter_tidy, envs)

    # Specialize _print_var to use print_format as default arg
    print_var = partial(_print_var, print_format=print_format, raw=raw)

    # List of strings that will be concatenated and returned
    ret = []

    # Print header
    if 'anonymize' not in print_filters:
        if print_format == 'pretty':
            ret.append("machine info: {0} {1} {2} {3} {4}\n".format(*os.uname()))
            ret.append(print_var('CHPL_HOME', ENV_VALS['CHPL_HOME']))
            this_dir = os.path.realpath(os.path.dirname(__file__))
            ret.append("script location: {0}\n".format(this_dir))
        elif print_format in ['simple', 'cmake']:
            ret.append(print_var('CHPL_HOME', ENV_VALS['CHPL_HOME']))

    # Print environment variables and their values
    for env in envs:
        name = env.name
        value = ENV_VALS[name]
        if print_format == 'path':
            if name == 'CHPL_TARGET_CPU':
                value = ENV_VALS['CHPL_RUNTIME_CPU']
            elif name == 'CHPL_COMM' and chpl_comm_debug.get() == 'debug':
                value += '-debug'
            elif name == 'CHPL_HWLOC' and chpl_hwloc_debug.get() == 'debug':
                value += '-debug'
            elif name == 'CHPL_TASKS' and chpl_tasks_debug.get() == 'debug':
                value += '-debug'
        if only:
            name = name.strip()
        ret.append(print_var(name, value, shortname=env.shortname))

    # Handle special formatting case for --path
    if print_format == 'path' and not raw:
        # Remove trailing '/' and add a newline
        ret[-1] = ret[-1].rstrip('/')
        ret.append('\n')

    if not raw:
        return ''.join(ret)
    else:
        return ret

def diagnose_missing_library(lib_type):
    libdir = chpl_home_utils.get_chpl_runtime_lib()
    subdir = ENV_VALS['CHPL_{}_SUBDIR'.format(lib_type.upper())]
    runtime_libdir = os.path.join(libdir, subdir)
    if not os.path.exists(runtime_libdir):
        variables = printchplenv(set([lib_type]), print_format='path', raw=True)
        current_path = libdir
        for var in variables:
            new_path = os.path.join(current_path, var[3])
            shortname = var[2]
            if os.path.exists(new_path):
                current_path = new_path
                continue

            varname = var[0].strip()
            print("There is no {} for '{}={}'".format(lib_type, varname, var[1]))
            others = os.listdir(current_path)
            options = [o if not shortname else o.removeprefix("{}-".format(shortname)) for o in others]
            if options:
                print("Valid options: {}".format(", ".join(options)))
            chplconfig_path = overrides.get_chplconfig_path()
            if env_set(varname):
                # TODO: is there a way to distinguish between an env var set
                # or a compiler argument?
                # i.e. `CHPL_TARGET_COMPILER=xxx vs --target-compiler=xxx`
                print("This variable is either set in the environment or via a command line argument, consider unsetting it.")
            elif config_set(varname):
                if chplconfig_path:
                    print("This variable is set in '{}', consider removing it.".format(chplconfig_path))
                else:
                    print("This variable is set in a chplconfig, consider removing it.")
            else:
                print("This variable was inferred from the current system and currently set variables:")
                affecting_env = printchplenv(set(['runtime', 'launcher', 'compiler', 'default', 'internal']), print_filters=set(["overrides", "anonymize"]), print_format="verbose")
                affecting_env = textwrap.indent(affecting_env, '  ')
                stdout.write(affecting_env)
            break


"""Define argument to parse"""
def parse_args():
    parser = optparse.OptionParser(
        usage='usage: %prog [options]',
        description = 'Print the current Chapel configuration. '
                      '[content] arguments determine what gets printed. '
                      '[filter] arguments determine what gets omitted. '
                      '[format] arguments determine how it gets printed. '
                      '[shortcut] arguments are for convenience.')

    #[shortcut]
    parser.add_option('--all', action='store_true', dest='all')

    #[content]
    parser.set_defaults(content=[])
    parser.add_option('--compiler', action='append_const', dest='content', const='compiler')
    parser.add_option('--runtime', action='append_const', dest='content', const='runtime')
    parser.add_option('--launcher', action='append_const', dest='content', const='launcher')
    parser.add_option('--internal', action='append_const', dest='content', const='internal')

    #[filter]
    parser.set_defaults(tidy=True)
    parser.set_defaults(filter=[])
    parser.add_option('--tidy', action='store_true', dest='tidy')
    parser.add_option('--no-tidy', action='store_false', dest='tidy')
    parser.add_option('--anonymize', action='append_const', dest='filter', const='anonymize')
    parser.add_option('--overrides', action='append_const', dest='filter', const='overrides')
    parser.add_option('--only-path', action='append_const', dest='filter', const='only-path')
    parser.add_option('--only', type=str, nargs=1, default=None, dest='only')

    #[format]
    parser.set_defaults(format='pretty')
    parser.add_option('--pretty', action='store_const', dest='format', const='pretty')
    parser.add_option('--simple', action='store_const', dest='format', const='simple')
    parser.add_option('--make',   action='store_const', dest='format', const='make')
    parser.add_option('--cmake',  action='store_const', dest='format', const='cmake')
    parser.add_option('--path',   action='store_const', dest='format', const='path')
    parser.add_option('--bash',   action='store_const', dest='format', const='bash')
    parser.add_option('--csh',    action='store_const', dest='format', const='csh')
    parser.add_option('--value',  action='store_const', dest='format', const='value')

    #[misc]
    parser.add_option('--ignore-errors', action='store_true', dest='ignore_errors')
    parser.add_option('--verify', action='store_true', dest='verify')
    parser.add_option('--diagnose-lib', nargs=1, type=str, dest='diagnose_lib', default=None,)

    #[hidden]
    parser.add_option('--devel', action='store_true', dest='devel')
    parser.add_option('--no-devel', action='store_true', dest='nodevel')
    parser.add_option('--verify-verbose', action='store_true', dest='verify_verbose')
    parser.add_option('--unit-tests', action='store_true', dest='do_unit_tests')
    # Hijack the help message to use the module docstring
    # optparse is not robust enough to support help msg sections for args.
    parser.print_help = lambda: stdout.write(__doc__)

    return parser.parse_args()


def main():
    (options, args) = parse_args()

    utils.init_CHPL_DEVELOPER()
    # let the command line override the environment variable
    if options.devel:
        utils.set_CHPL_DEVELOPER(True)
    if options.nodevel:
        utils.set_CHPL_DEVELOPER(False)

    # If passed hidden --unit-tests flag, perform all PyUnit tests that can we
    # can find and exit.
    if options.do_unit_tests:
      this_dir = os.path.realpath(os.path.dirname(__file__))
      test_loader = unittest.TestLoader()
      test_suite = test_loader.discover(this_dir, pattern="*.py")
      test_runner = unittest.TextTestRunner()
      test_runner.run(test_suite)
      exit(1)

    # Handle --all flag
    all_content = ['runtime', 'launcher', 'compiler', 'default']
    if options.all:
        options.content.extend(all_content)

    # Handle --tidy / --no-tidy flags
    if options.tidy:
        options.filter.append('tidy')

    # Handle --only
    only = None
    if options.only:
        only = set([o.strip() for o in options.only.split(",")])
        options.content.extend(all_content + ['internal'])
        options.filter.extend(['only', 'anonymize'])

    # Set default [content]
    if not options.content:
        options.content = ['default']

    # Convert lists to sets to pass to printchplenv
    contents = set(options.content)
    filters = set(options.filter)

    # Prevent --internal --path, because it's useless
    if options.format == 'path' and 'internal' in contents:
        stdout.write('--path and --internal are incompatible flags\n')
        exit(1)
    # Prevent --only --path, because it's useless
    if options.format == 'path' and options.only:
        stdout.write('--path and --only are incompatible flags\n')
        exit(1)

    if options.ignore_errors:
        utils.ignore_errors = True

    # Populate ENV_VALS
    compute_all_values()

    # Don't populate internal ENV_VALS unless specified
    if 'internal' in contents or options.verify or options.verify_verbose or options.diagnose_lib:
        compute_internal_values()

    print_values = not options.diagnose_lib

    if print_values:
        ret = printchplenv(contents, filters, options.format, only=only)
        stdout.write(ret)

    utils.flush_warnings()

    # at this point, we should run the verification tests
    if options.verify or options.verify_verbose:
        global ENV_VALS

        success, reason = chplenv_verify.verify(ENV_VALS, verbose=options.verify_verbose)
        if not success:
            utils.error("Verification failed: {}".format(reason))

    if options.diagnose_lib:
        diagnose_missing_library(options.diagnose_lib)


if __name__ == '__main__':
    main()
