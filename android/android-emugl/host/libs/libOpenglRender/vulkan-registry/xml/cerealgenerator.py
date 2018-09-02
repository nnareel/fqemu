#!/usr/bin/python3 -i
#
# Copyright (c) 2013-2018 The Khronos Group Inc.
# Copyright (c) 2013-2018 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os, re, sys
from generator import *

import cereal

from copy import deepcopy

# CerealGenerator - generates set of driver sources
# while being agnostic to the stream implementation

copyrightHeader = """// Copyright (C) 2018 The Android Open Source Project
// Copyright (C) 2018 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
"""

autogeneratedHeaderTemplate = """
// Autogenerated module %s
// %s
// Please do not modify directly;
// re-run android/scripts/generate-vulkan-sources.sh,
// or directly from Python by defining:
// VULKAN_REGISTRY_XML_DIR : Directory containing genvk.py and vk.xml
// CEREAL_OUTPUT_DIR: Where to put the generated sources.
// python3 $VULKAN_REGISTRY_XML_DIR/genvk.py -registry $VULKAN_REGISTRY_XML_DIR/vk.xml cereal -o $CEREAL_OUTPUT_DIR
"""

autogeneratedMkTemplate = """
# Autogenerated makefile
# %s
# Please do not modify directly;
# re-run android/scripts/generate-vulkan-sources.sh,
# or directly from Python by defining:
# VULKAN_REGISTRY_XML_DIR : Directory containing genvk.py and vk.xml
# CEREAL_OUTPUT_DIR: Where to put the generated sources.
# python3 $VULKAN_REGISTRY_XML_DIR/genvk.py -registry $VULKAN_REGISTRY_XML_DIR/vk.xml cereal -o $CEREAL_OUTPUT_DIR
"""

def banner_command(argv):
    """Return sanitized command-line description.
       |argv| must be a list of command-line parameters, e.g. sys.argv.
       Return a string corresponding to the command, with platform-specific
       paths removed."""

    def makeRelative(someArg):
        if os.path.exists(someArg):
            return os.path.relpath(someArg)
        return someArg

    return ' '.join(map(makeRelative, argv))

# ---- methods overriding base class ----
# beginFile(genOpts)
# endFile()
# beginFeature(interface, emit)
# endFeature()
# genType(typeinfo,name)
# genStruct(typeinfo,name)
# genGroup(groupinfo,name)
# genEnum(enuminfo, name)
# genCmd(cmdinfo)
class CerealGenerator(OutputGenerator):

    """Generate serialization code"""
    def __init__(self, errFile = sys.stderr,
                       warnFile = sys.stderr,
                       diagFile = sys.stdout):
        OutputGenerator.__init__(self, errFile, warnFile, diagFile)

        self.typeInfo = cereal.VulkanTypeInfo()

        self.modules = {}
        self.moduleList = []

        self.wrappers = []

        self.codegen = cereal.CodeGen()

        self.cereal_Android_mk_header = """%s
LOCAL_PATH := $(call my-dir)

# For Vulkan libraries

cereal_C_INCLUDES := \\
    $(LOCAL_PATH) \\
    $(LOCAL_PATH)/../ \\
    $(EMUGL_PATH)/host/include/vulkan \\

cereal_STATIC_LIBRARIES := \\
    android-emu \\
    android-emu-base \\
""" % (autogeneratedMkTemplate % banner_command(sys.argv))

        # Define our generated modules and wrappers here

        self.addModule("common", "goldfish_vk_marshaling")
        self.addModule("guest", "goldfish_vk_frontend")

        self.addWrapper(cereal.VulkanMarshaling(self.modules["goldfish_vk_marshaling"], self.typeInfo))
        self.addWrapper(cereal.VulkanFrontend(self.modules["goldfish_vk_frontend"], self.typeInfo))

        # Testing module
        self.addModule("common", "goldfish_vk_testing")
        self.addWrapper(cereal.VulkanTesting(self.modules["goldfish_vk_testing"], self.typeInfo))

        # Utility module
        self.addModule("common", "goldfish_vk_deepcopy")
        self.addWrapper(cereal.VulkanDeepcopy(self.modules["goldfish_vk_deepcopy"], self.typeInfo))

        self.cereal_Android_mk_body = """
$(call emugl-begin-static-library,lib$(BUILD_TARGET_SUFFIX)OpenglRender_vulkan_cereal)

LOCAL_C_INCLUDES += $(cereal_C_INCLUDES)

LOCAL_STATIC_LIBRARIES += $(cereal_STATIC_LIBRARIES)

LOCAL_SRC_FILES := \\
"""
        def addSrcEntry(m):
            self.cereal_Android_mk_body += m.getMakefileSrcEntry()

        self.forEachModule(addSrcEntry)

        self.cereal_Android_mk_body += """
$(call emugl-end-module)
"""

        # Marshaling header/impl pre/post#######################################
        self.modules["goldfish_vk_marshaling"].headerPreamble += """
#pragma once

#include <vulkan.h>

namespace goldfish_vk {

class VulkanStream;

"""
        self.modules["goldfish_vk_marshaling"].headerPostamble = """
} // namespace goldfish_vk
"""

        self.modules["goldfish_vk_marshaling"].implPreamble += """
#include "goldfish_vk_marshaling.h"

#include "VulkanStream.h"

#include "android/base/files/StreamSerializing.h"

namespace goldfish_vk {

"""
        self.modules["goldfish_vk_marshaling"].implPostamble = """
} // namespace goldfish_vk
"""

        # Frontend header/impl pre/post#########################################
        self.modules["goldfish_vk_frontend"].headerPreamble += """
#pragma once

#include <vulkan.h>

namespace goldfish_vk {

class VulkanStream;
"""
        self.modules["goldfish_vk_frontend"].headerPostamble = """
} // namespace goldfish_vk
"""

        self.modules["goldfish_vk_frontend"].implPreamble += """
#include "guest/goldfish_vk_frontend.h"

#include "common/goldfish_vk_marshaling.h"

#include "VulkanStream.h"

namespace goldfish_vk {

"""
        self.modules["goldfish_vk_frontend"].implPostamble = """
} // namespace goldfish_vk
"""
        # Testing header/impl pre/post##########################################
        self.modules["goldfish_vk_testing"].headerPreamble += """
#pragma once

#include <vulkan.h>

#include <functional>

namespace goldfish_vk {

using OnFailCompareFunc = std::function<void(const char*)>;

"""
        self.modules["goldfish_vk_testing"].headerPostamble = """
} // namespace goldfish_vk
"""

        self.modules["goldfish_vk_testing"].implPreamble += """
#include "goldfish_vk_testing.h"

#include <string.h>

namespace goldfish_vk {

"""
        self.modules["goldfish_vk_testing"].implPostamble = """
} // namespace goldfish_vk
"""
        # Deepcopy pre/postambles########3######################################
        self.modules["goldfish_vk_deepcopy"].headerPreamble += """
#pragma once

#include <vulkan.h>

#include "android/base/Pool.h"

using android::base::Pool;

namespace goldfish_vk {

"""
        self.modules["goldfish_vk_deepcopy"].headerPostamble = """
} // namespace goldfish_vk
"""

        self.modules["goldfish_vk_deepcopy"].implPreamble += """
#include "goldfish_vk_deepcopy.h"

#include <string.h>

namespace goldfish_vk {

"""
        self.modules["goldfish_vk_deepcopy"].implPostamble = """
} // namespace goldfish_vk
"""

    def addModule(self, directory, basename):
        self.moduleList.append(basename)
        self.modules[basename] = cereal.Module(directory, basename)
        self.modules[basename].headerPreamble = copyrightHeader
        self.modules[basename].headerPreamble += \
                autogeneratedHeaderTemplate % (basename, "(header) generated by %s" % banner_command(sys.argv))
        self.modules[basename].implPreamble = copyrightHeader
        self.modules[basename].implPreamble += \
                autogeneratedHeaderTemplate % (basename, "(impl) generated by %s" % banner_command(sys.argv))

    def addWrapper(self, wrapper):
        self.wrappers.append(wrapper)

    def forEachModule(self, func):
        for moduleName in self.moduleList:
            func(self.modules[moduleName])

    def forEachWrapper(self, func):
        for wrapper in self.wrappers:
            func(wrapper)

## Overrides####################################################################

    def beginFile(self, genOpts):
        OutputGenerator.beginFile(self, genOpts)

        write(self.cereal_Android_mk_header, file = self.outFile)
        write(self.cereal_Android_mk_body, file = self.outFile)

        self.forEachModule(lambda m: m.begin(self.genOpts.directory))

    def endFile(self):
        OutputGenerator.endFile(self)

        self.forEachModule(lambda m: m.end())

    def beginFeature(self, interface, emit):
        # Start processing in superclass
        OutputGenerator.beginFeature(self, interface, emit)

        self.forEachModule(lambda m: m.appendHeader("#ifdef %s\n" % self.featureName))
        self.forEachModule(lambda m: m.appendImpl("#ifdef %s\n" % self.featureName))

    def endFeature(self):
        # Finish processing in superclass
        OutputGenerator.endFeature(self)

        self.forEachModule(lambda m: m.appendHeader("#endif\n"))
        self.forEachModule(lambda m: m.appendImpl("#endif\n"))

    def genType(self, typeinfo, name, alias):
        OutputGenerator.genType(self, typeinfo, name, alias)
        self.typeInfo.onGenType(typeinfo, name, alias)
        self.forEachWrapper(lambda w: w.onGenType(typeinfo, name, alias))

    def genStruct(self, typeinfo, typeName, alias):
        OutputGenerator.genStruct(self, typeinfo, typeName, alias)
        self.typeInfo.onGenStruct(typeinfo, typeName, alias)
        self.forEachWrapper(lambda w: w.onGenStruct(typeinfo, typeName, alias))

    def genGroup(self, groupinfo, groupName, alias = None):
        OutputGenerator.genGroup(self, groupinfo, groupName, alias)
        self.typeInfo.onGenGroup(groupinfo, groupName, alias)
        self.forEachWrapper(lambda w: w.onGenGroup(groupinfo, groupName, alias))

    def genEnum(self, enuminfo, name, alias):
        OutputGenerator.genEnum(self, enuminfo, name, alias)
        self.typeInfo.onGenEnum(enuminfo, name, alias)
        self.forEachWrapper(lambda w: w.onGenEnum(enuminfo, name, alias))

    def genCmd(self, cmdinfo, name, alias):
        OutputGenerator.genCmd(self, cmdinfo, name, alias)
        self.typeInfo.onGenCmd(cmdinfo, name, alias)
        self.forEachWrapper(lambda w: w.onGenCmd(cmdinfo, name, alias))
