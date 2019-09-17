# Copyright 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This file contains a set of functions that make it easier to configure the toolchain used by the emulator
# It's main responsibility is to create the toolchain by invoking the toolchain shell script that generates
# the proper wrappers around the compilers and sysroot that are found in the build tree.
#
# We need this to make sure that are builds are consistent accross different development environments.


# Gets the desired clang version used by the various toolchains.
# Update this if you need to change the compiler
function(get_clang_version RET_VAL)
  set(${RET_VAL} "clang-r365631b" PARENT_SCOPE)
endfunction()


# This invokes the toolchain generator
# HOST
#   The host to use
# PARAM1, PARAM2
#   Parameters to pass on to the gen-android-sdk-toolchain.sh script.
#
# Sets STD_OUT
#   The output produced by the script
function(toolchain_cmd HOST PARAM1 PARAM2)
    set(VERBOSITY "2")
    if (PARAM2 MATCHES ".*unused.*")
        set(VERBOSITY "0")
    endif()
    get_filename_component(GEN_SDK "${CMAKE_CURRENT_LIST_FILE}/../../../scripts/unix/gen-android-sdk-toolchain.sh" ABSOLUTE)
    get_filename_component(AOSP "${CMAKE_CURRENT_LIST_DIR}/../../../../.." ABSOLUTE)
    get_clang_version(CLANG_VER)

    message("Running ${GEN_SDK} '--host=${HOST}' '${PARAM1}' '${PARAM2}' '--aosp-dir=${AOSP}' '--aosp-clang_ver=${CLANG_VER}' '--verbosity=${VERBOSITY}'")
    execute_process(COMMAND ${GEN_SDK} "--host=${HOST}" "${PARAM1}" "${PARAM2}"
                                       "--aosp-clang_ver=${CLANG_VER}"
                                       "--aosp-dir=${AOSP}"
                                       "--verbosity=${VERBOSITY}"
        RESULT_VARIABLE GEN_SDK_RES
        OUTPUT_VARIABLE STD_OUT
        ERROR_VARIABLE STD_ERR)
    if(NOT "${GEN_SDK_RES}" STREQUAL "0")
        message(FATAL_ERROR "Unable to retrieve sdk info from ${GEN_SDK} --host=${HOST} ${PARAM1} ${PARAM2}: ${STD_OUT}, ${STD_ERR}")
    endif()
    message("${STD_OUT}")

    # Clean up and make visibile
    string(REPLACE "\n" "" STD_OUT "${STD_OUT}")
    set(STD_OUT ${STD_OUT} PARENT_SCOPE)
endfunction ()

# Generates the toolchain for the given host.
# The toolchain is generated by calling the gen-android-sdk script
#
# It will set the all the necesary CMAKE compiler flags to use the
# android toolchain for the given host.
#
# TARGET_OS:
#   The target environment for which we are creating the toolchain
#
# Returns:
#   The variable ANDROID_SYSROOT will be set to point to the sysroot for the
#   target os.
#   The variablke ANDROID_COMPILER_PREFIX will point to the prefix for
#     gcc, g++, ar, ld etc..
function(toolchain_generate_internal TARGET_OS)
    set(TOOLCHAIN "${PROJECT_BINARY_DIR}/toolchain")

    # First we generate the toolchain.
    if (NOT EXISTS ${TOOLCHAIN})
        # Force update the windows sdk if the flag is provided
        if (${OPTION_WINTOOLCHAIN})
		message(WARNING "Force downloading the Windows toolchain. This may take a couple of minutes...")
            toolchain_cmd("${TARGET_OS}" "--force-fetch-wintoolchain" "unused")
        endif ()
        toolchain_cmd("${TARGET_OS}" "${TOOLCHAIN}" "")
    endif ()

    # Let's find the bin-prefix
    toolchain_cmd("${TARGET_OS}" "--print=binprefix" "unused")
    set(ANDROID_COMPILER_PREFIX "${TOOLCHAIN}/${STD_OUT}" PARENT_SCOPE)

    # And define all the compilers..
    toolchain_cmd("${TARGET_OS}" "--print=sysroot" "unused")
    set(ANDROID_SYSROOT "${STD_OUT}" PARENT_SCOPE)
endfunction ()

# Gets the given key from the enviroment. This your usual shell environment
# and is globally visuable during cmake generation. This used by the toolchain
# generator to work around some caching issues. (CMake blows away the internal
# variables when configuring the toolchain)
# No sane person would use this outside the toolchain generation.
function(internal_get_env_cache KEY)
    set(${KEY} $ENV{ENV_CACHE_${KEY}} PARENT_SCOPE)
endfunction()

# Sets the given key in the environment to the given value.
function(internal_set_env_cache KEY VAL)
    set(ENV{ENV_CACHE_${KEY}} "${VAL}")
    set(${KEY} "${VAL}" PARENT_SCOPE)
endfunction()

function(toolchain_generate TARGET_OS)
    # This is a hack to workaround the fact that cmake will keep including
    # the toolchain defintion over and over, and it will wipe out all the settings.
    # so we will just store them in the environment, which gets blown away on exit
    # of cmake anyway..
    internal_get_env_cache(COMPILER_PREFIX)
    internal_get_env_cache(ANDROID_SYSROOT)
    if ("${COMPILER_PREFIX}" STREQUAL "")
        toolchain_generate_internal(${TARGET_OS})
        internal_set_env_cache(COMPILER_PREFIX "${ANDROID_COMPILER_PREFIX}")
        internal_set_env_cache(ANDROID_SYSROOT "${ANDROID_SYSROOT}")
    endif ()

    set(CMAKE_RC_COMPILER ${COMPILER_PREFIX}windres CACHE PATH "windres")
    set(CMAKE_C_COMPILER ${COMPILER_PREFIX}gcc CACHE PATH "C compiler")
    set(CMAKE_CXX_COMPILER ${COMPILER_PREFIX}g++ CACHE PATH "C++ compiler")
    # We will use system bintools (Note, this might not work with msvc)
    # As setting the AR somehow causes all sorts of strange issues.
    # set(CMAKE_AR ${COMPILER_PREFIX}ar PARENT_SCOPE)
    set(CMAKE_RANLIB ${COMPILER_PREFIX}ranlib CACHE PATH "Ranlib")
    set(CMAKE_OBJCOPY ${COMPILER_PREFIX}objcopy CACHE PATH "Objcopy")
    set(CMAKE_STRIP ${COMPILER_PREFIX}strip CACHE PATH "strip")
    set(ANDROID_SYSROOT ${ANDROID_SYSROOT} CACHE PATH "Sysroot")
    set(ANDROID_LLVM_SYMBOLIZER ${PROJECT_BINARY_DIR}/toolchain/llvm-symbolizer CACHE PATH "symbolizer")
endfunction()

function(_get_host_tag RET_VAL)
    # Prebuilts to be used on the host os should fall under one of
    # the below tags
    if (APPLE)
        set (${RET_VAL} "darwin-x86_64" PARENT_SCOPE)
    elseif (UNIX)
        set (${RET_VAL} "linux-x86_64" PARENT_SCOPE)
    else ()
        set (${RET_VAL} "windows_msvc-x86_64" PARENT_SCOPE)
    endif ()
endfunction()

function(toolchain_generate_msvc TARGET_OS)
    # This is a hack to workaround the fact that cmake will keep including
    # the toolchain defintion over and over, and it will wipe out all the settings.
    # so we will just store them in the environment, which gets blown away on exit
    # anyway..
    get_env_cache(COMPILER_PREFIX)
    get_env_cache(ANDROID_SYSROOT)
    if ("${COMPILER_PREFIX}" STREQUAL "")
        toolchain_generate_internal(${TARGET_OS})
        set_env_cache(COMPILER_PREFIX "${ANDROID_COMPILER_PREFIX}")
        set_env_cache(ANDROID_SYSROOT "${ANDROID_SYSROOT}")
    endif ()

    set(triple x86_64-pc-win32)
    set(CMAKE_RC_COMPILER ${COMPILER_PREFIX}windres PARENT_SCOPE)
    set(CMAKE_C_COMPILER ${COMPILER_PREFIX}clang PARENT_SCOPE)
    set(CMAKE_C_COMPILER_TARGET ${triple} PARENT_SCOPE)
    set(CMAKE_CXX_COMPILER ${COMPILER_PREFIX}clang++ PARENT_SCOPE)
    set(CMAKE_CXX_COMPILER_TARGET ${triple} PARENT_SCOPE)
    # We will use system bintools
    # set(CMAKE_AR ${COMPILER_PREFIX}ar PARENT_SCOPE)
    set(CMAKE_RANLIB ${COMPILER_PREFIX}ranlib PARENT_SCOPE)
    set(CMAKE_OBJCOPY ${COMPILER_PREFIX}objcopy PARENT_SCOPE)
    set(CMAKE_STRIP ${COMPILER_PREFIX}strip PARENT_SCOPE)
    set(ANDROID_SYSROOT ${ANDROID_SYSROOT} PARENT_SCOPE)
endfunction()


function(toolchain_configure_tags tag)
    set(ANDROID_TARGET_TAG ${tag})
    string(REGEX REPLACE "-.*" "" ANDROID_TARGET_OS ${tag})
    string(REGEX REPLACE "[-_].*" "" ANDROID_TARGET_OS_FLAVOR ${tag})
    _get_host_tag(ANDROID_HOST_TAG)
    if (NOT ANDROID_TARGET_TAG STREQUAL ANDROID_HOST_TAG)
        set(CROSSCOMPILE TRUE PARENT_SCOPE)
    endif()

    if (ANDROID_TARGET_TAG STREQUAL "windows-x86_64")
      set(WINDOWS_X86_64 TRUE PARENT_SCOPE)
      set(WINDOWS TRUE PARENT_SCOPE)
    elseif (ANDROID_TARGET_TAG STREQUAL "windows_msvc-x86_64")
      set(WINDOWS TRUE PARENT_SCOPE)
      set(WINDOWS_MSVC_X86_64 TRUE PARENT_SCOPE)
    elseif (ANDROID_TARGET_TAG STREQUAL "linux-x86_64")
      set(LINUX_X86_64 TRUE PARENT_SCOPE)
    elseif (ANDROID_TARGET_TAG STREQUAL "darwin-x86_64")
      set(DARWIN_X86_64 TRUE PARENT_SCOPE)
    endif()

    if (ANDOID_HOST_TAG STREQUAL "windows_msvc-x86_64")
      set(HOST_WINDOWS_MSVC_X86_64 TRUE PARENT_SCOPE)
      set(HOST_WINDOWS TRUE PARENT_SCOPE)
    elseif (ANDOID_HOST_TAG STREQUAL "linux-x86_64")
      set(HOST_LINUX_X86_64 TRUE PARENT_SCOPE)
    elseif (ANDOID_HOST_TAG STREQUAL "darwin-x86_64")
      set(HOST_DARWIN_X86_64 TRUE PARENT_SCOPE)
    endif()

    # Export the oldschool tags as well.
    set(ANDROID_TARGET_TAG ${tag} PARENT_SCOPE)
    set(ANDROID_HOST_TAG ${ANDROID_HOST_TAG} PARENT_SCOPE)

    set(ANDROID_TARGET_OS "${ANDROID_TARGET_OS}" PARENT_SCOPE)
    set(ANDROID_TARGET_OS_FLAVOR "${ANDROID_TARGET_OS_FLAVOR}" PARENT_SCOPE)
endfunction()

get_filename_component(ANDROID_QEMU2_TOP_DIR "${CMAKE_CURRENT_LIST_FILE}/../../../.." ABSOLUTE)
