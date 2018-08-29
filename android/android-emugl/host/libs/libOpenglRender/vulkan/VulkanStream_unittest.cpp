// Copyright (C) 2018 The Android Open Source Project
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
#include "VulkanStream.h"

#include "common/goldfish_vk_marshaling.h"
#include "common/goldfish_vk_testing.h"

#include "android/base/ArraySize.h"

#include <gtest/gtest.h>
#include <string.h>
#include <vulkan.h>

using android::base::arraySize;

namespace goldfish_vk {

class VulkanStreamForTesting : public VulkanStream {
public:
    ssize_t read(void* buffer, size_t size) override {
        EXPECT_LE(mReadCursor + size, mBuffer.size());
        memcpy(buffer, mBuffer.data() + mReadCursor, size);

        mReadCursor += size;

        if (mReadCursor == mWriteCursor) {
            clear();
        }
        return size;
    }

    ssize_t write(const void* buffer, size_t size) override {
        if (mBuffer.size() < mWriteCursor + size) {
            mBuffer.resize(mWriteCursor + size);
        }

        memcpy(mBuffer.data() + mWriteCursor, buffer, size);

        mWriteCursor += size;

        if (mReadCursor == mWriteCursor) {
            clear();
        }
        return size;
    }

private:
    void clear() {
        mBuffer.clear();
        mReadCursor = 0;
        mWriteCursor = 0;
    }

    size_t mReadCursor = 0;
    size_t mWriteCursor = 0;
    std::vector<char> mBuffer;
};

// Just see whether the test class is OK
TEST(VulkanStream, Basic) {
    VulkanStreamForTesting stream;

    const uint32_t testInt = 6;
    stream.putBe32(testInt);
    EXPECT_EQ(testInt, stream.getBe32());

    const std::string testString = "Hello World";
    stream.putString(testString);
    EXPECT_EQ(testString, stream.getString());
}

// Try a "basic" Vulkan struct (VkInstanceCreateInfo)
TEST(VulkanStream, testMarshalVulkanStruct) {
    VulkanStreamForTesting stream;

    VkApplicationInfo appInfo = {
        VK_STRUCTURE_TYPE_APPLICATION_INFO,
        0, // pNext
        "VulkanStreamTest", // application name
        6, // application version
        "VulkanStreamTestEngine", //engine name
        4, // engine version,
        VK_API_VERSION_1_0,
    };

    const char* const layerNames[] = {
        "layer0",
        "layer1: test layer",
    };

    const char* const extensionNames[] = {
        "VK_KHR_8bit_storage",
        "VK_KHR_android_surface",
        "VK_MVK_macos_surface",
    };

    VkInstanceCreateInfo forMarshaling = {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        0, // pNext
        0, // flags,
        &appInfo, // pApplicationInfo,
        arraySize(layerNames),
        layerNames,
        arraySize(extensionNames),
        extensionNames
    };

    marshal_VkInstanceCreateInfo(&stream, &forMarshaling);

    VkApplicationInfo forUnmarshalingAppInfo;
    VkInstanceCreateInfo forUnmarshaling;
    memset(&forUnmarshaling, 0x0, sizeof(VkInstanceCreateInfo));
    forUnmarshaling.pApplicationInfo = &forUnmarshalingAppInfo;

    // Before unmarshaling, these structs should be different.
    // Test that the generated comparator can detect inequality.
    int inequalities = 0;
    checkEqual_VkInstanceCreateInfo(
        &forMarshaling, &forUnmarshaling, [&inequalities](const char* errMsg) {
        (void)errMsg;
        ++inequalities;
    });

    EXPECT_GT(inequalities, 0);

    unmarshal_VkInstanceCreateInfo(&stream, &forUnmarshaling);

    EXPECT_EQ(forMarshaling.sType, forUnmarshaling.sType);
    EXPECT_EQ(forMarshaling.pNext, forUnmarshaling.pNext);
    EXPECT_EQ(forMarshaling.flags, forUnmarshaling.flags);
    EXPECT_EQ(forMarshaling.pApplicationInfo->sType,
              forUnmarshaling.pApplicationInfo->sType);
    EXPECT_EQ(forMarshaling.pApplicationInfo->apiVersion,
              forUnmarshaling.pApplicationInfo->apiVersion);

    checkEqual_VkInstanceCreateInfo(
        &forMarshaling, &forUnmarshaling, [](const char* errMsg) {
        EXPECT_TRUE(false) << errMsg;
    });
}

// Try a Vulkan struct that has non-ptr structs in it
TEST(VulkanStream, testMarshalVulkanStructWithNonPtrStruct) {
    VulkanStreamForTesting stream;

    VkPhysicalDeviceProperties forMarshaling = {
        VK_API_VERSION_1_0,
        0,
        0x8086,
        0x7800,
        VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
        "Intel740",
        "123456789abcdef",
        {
            0x00, // maxImageDimension1D;
            0x01, // maxImageDimension2D;
            0x02, // maxImageDimension3D;
            0x03, // maxImageDimensionCube;
            0x04, // maxImageArrayLayers;
            0x05, // maxTexelBufferElements;
            0x06, // maxUniformBufferRange;
            0x07, // maxStorageBufferRange;
            0x08, // maxPushConstantsSize;
            0x09, // maxMemoryAllocationCount;
            0x0a, // maxSamplerAllocationCount;
            0x0b, // bufferImageGranularity;
            0x0c, // sparseAddressSpaceSize;
            0x0d, // maxBoundDescriptorSets;
            0x0e, // maxPerStageDescriptorSamplers;
            0x0f, // maxPerStageDescriptorUniformBuffers;
            0x10, // maxPerStageDescriptorStorageBuffers;
            0x11, // maxPerStageDescriptorSampledImages;
            0x12, // maxPerStageDescriptorStorageImages;
            0x13, // maxPerStageDescriptorInputAttachments;
            0x14, // maxPerStageResources;
            0x15, // maxDescriptorSetSamplers;
            0x16, // maxDescriptorSetUniformBuffers;
            0x17, // maxDescriptorSetUniformBuffersDynamic;
            0x18, // maxDescriptorSetStorageBuffers;
            0x19, // maxDescriptorSetStorageBuffersDynamic;
            0x1a, // maxDescriptorSetSampledImages;
            0x1b, // maxDescriptorSetStorageImages;
            0x1c, // maxDescriptorSetInputAttachments;
            0x1d, // maxVertexInputAttributes;
            0x1e, // maxVertexInputBindings;
            0x1f, // maxVertexInputAttributeOffset;
            0x20, // maxVertexInputBindingStride;
            0x21, // maxVertexOutputComponents;
            0x22, // maxTessellationGenerationLevel;
            0x23, // maxTessellationPatchSize;
            0x24, // maxTessellationControlPerVertexInputComponents;
            0x25, // maxTessellationControlPerVertexOutputComponents;
            0x26, // maxTessellationControlPerPatchOutputComponents;
            0x27, // maxTessellationControlTotalOutputComponents;
            0x28, // maxTessellationEvaluationInputComponents;
            0x29, // maxTessellationEvaluationOutputComponents;
            0x2a, // maxGeometryShaderInvocations;
            0x2b, // maxGeometryInputComponents;
            0x2c, // maxGeometryOutputComponents;
            0x2d, // maxGeometryOutputVertices;
            0x2e, // maxGeometryTotalOutputComponents;
            0x2f, // maxFragmentInputComponents;
            0x30, // maxFragmentOutputAttachments;
            0x31, // maxFragmentDualSrcAttachments;
            0x32, // maxFragmentCombinedOutputResources;
            0x33, // maxComputeSharedMemorySize;
            { 0x1, 0x2, 0x3 }, // maxComputeWorkGroupCount[3];
            0x35, // maxComputeWorkGroupInvocations;
            { 0x4, 0x5, 0x6 }, // maxComputeWorkGroupSize[3];
            0x37, // subPixelPrecisionBits;
            0x38, // subTexelPrecisionBits;
            0x39, // mipmapPrecisionBits;
            0x3a, // maxDrawIndexedIndexValue;
            0x3b, // maxDrawIndirectCount;
            1.0f, // maxSamplerLodBias;
            1.0f, // maxSamplerAnisotropy;
            0x3e, // maxViewports;
            { 0x7, 0x8 }, // maxViewportDimensions[2];
            { 0.4f, 0.5f }, // viewportBoundsRange[2];
            0x41, // viewportSubPixelBits;
            0x42, // minMemoryMapAlignment;
            0x43, // minTexelBufferOffsetAlignment;
            0x44, // minUniformBufferOffsetAlignment;
            0x45, // minStorageBufferOffsetAlignment;
            0x46, // minTexelOffset;
            0x47, // maxTexelOffset;
            0x48, // minTexelGatherOffset;
            0x49, // maxTexelGatherOffset;
            10.0f, // minInterpolationOffset;
            11.0f, // maxInterpolationOffset;
            0x4c, // subPixelInterpolationOffsetBits;
            0x4d, // maxFramebufferWidth;
            0x4e, // maxFramebufferHeight;
            0x4f, // maxFramebufferLayers;
            0x50, // framebufferColorSampleCounts;
            0x51, // framebufferDepthSampleCounts;
            0x52, // framebufferStencilSampleCounts;
            0x53, // framebufferNoAttachmentsSampleCounts;
            0x54, // maxColorAttachments;
            0x55, // sampledImageColorSampleCounts;
            0x56, // sampledImageIntegerSampleCounts;
            0x57, // sampledImageDepthSampleCounts;
            0x58, // sampledImageStencilSampleCounts;
            0x59, // storageImageSampleCounts;
            0x5a, // maxSampleMaskWords;
            0x5b, // timestampComputeAndGraphics;
            100.0f, // timestampPeriod;
            0x5d, // maxClipDistances;
            0x5e, // maxCullDistances;
            0x5f, // maxCombinedClipAndCullDistances;
            0x60, // discreteQueuePriorities;
            { 0.0f, 1.0f }, // pointSizeRange[2];
            { 1.0f, 2.0f }, // lineWidthRange[2];
            3.0f, // pointSizeGranularity;
            4.0f, // lineWidthGranularity;
            0x65, // strictLines;
            0x66, // standardSampleLocations;
            0x67, // optimalBufferCopyOffsetAlignment;
            0x68, // optimalBufferCopyRowPitchAlignment;
            0x69, // nonCoherentAtomSize;
        },
        {
            0xff, // residencyStandard2DBlockShape;
            0x00, // residencyStandard2DMultisampleBlockShape;
            0x11, // residencyStandard3DBlockShape;
            0x22, // residencyAlignedMipSize;
            0x33, // residencyNonResidentStrict;
        },
    };

    marshal_VkPhysicalDeviceProperties(&stream, &forMarshaling);

    VkPhysicalDeviceProperties forUnmarshaling;
    memset(&forUnmarshaling, 0x0, sizeof(VkPhysicalDeviceLimits));

    // Test the autogenerated testing code
    int inequalities = 0;
    checkEqual_VkPhysicalDeviceProperties(
        &forMarshaling, &forUnmarshaling, [&inequalities](const char* errMsg) {
        (void)errMsg;
        ++inequalities;
    });

    EXPECT_GT(inequalities, 0);

    unmarshal_VkPhysicalDeviceProperties(&stream, &forUnmarshaling);

    // Test the autogenerated testing code
    EXPECT_EQ(VK_API_VERSION_1_0, forUnmarshaling.apiVersion);
    EXPECT_EQ(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, forUnmarshaling.deviceType);
    EXPECT_EQ(2.0f, forUnmarshaling.limits.lineWidthRange[1]);
    EXPECT_EQ(11.0f, forUnmarshaling.limits.maxInterpolationOffset);

    checkEqual_VkPhysicalDeviceProperties(
        &forMarshaling, &forUnmarshaling, [](const char* errMsg) {
        EXPECT_TRUE(false) << errMsg;
    });
}

// Try a Vulkan struct that has ptr fields with count (dynamic arrays)
TEST(VulkanStream, testMarshalVulkanStructWithPtrFields) {
    VulkanStreamForTesting stream;

    const uint32_t bindCount = 14;

    std::vector<VkSparseImageMemoryBind> sparseBinds;

    for (uint32_t i = 0; i < bindCount; i++) {
        VkSparseImageMemoryBind sparseBind = {
            // VkImageSubresource subresource
            {
                VK_IMAGE_ASPECT_COLOR_BIT |
                VK_IMAGE_ASPECT_DEPTH_BIT,
                i,
                i * 2,
            },
            // VkOffset3D offset
            { 1, 2 + (int32_t)i, 3},
            // VkExtent3D extent
            { 10, 20 * i, 30},
            // VkDeviceMemory memory
            (VkDeviceMemory)(uintptr_t)(0xff - i),
            // VkDeviceSize memoryOffset
            0x12345678 + i,
            // VkSparseMemoryBindFlags flags
            VK_SPARSE_MEMORY_BIND_METADATA_BIT,
        };

        sparseBinds.push_back(sparseBind);
    }

    std::vector<VkSparseImageMemoryBind> sparseBindsForUnmarshaling = sparseBinds;
    memset(sparseBindsForUnmarshaling.data(), 0x0,
           sparseBindsForUnmarshaling.size() * sizeof(VkSparseImageMemoryBind));

    VkSparseImageMemoryBindInfo forMarshaling = {
        (VkImage)(uintptr_t)54,
        bindCount,
        sparseBinds.data(),
    };

    marshal_VkSparseImageMemoryBindInfo(&stream, &forMarshaling);

    VkSparseImageMemoryBindInfo forUnmarshaling = {
        0, 0,
        sparseBindsForUnmarshaling.data(),
    };

    unmarshal_VkSparseImageMemoryBindInfo(&stream, &forUnmarshaling);

    EXPECT_EQ(bindCount, forUnmarshaling.bindCount);
    EXPECT_EQ(forMarshaling.image, forUnmarshaling.image);

    // Test some values in there so we know the autogenerated
    // compare code works.
    for (uint32_t i = 0; i < bindCount; i++) {
        EXPECT_EQ(forMarshaling.pBinds[i].memoryOffset,
                  forUnmarshaling.pBinds[i].memoryOffset);
        EXPECT_EQ(forMarshaling.pBinds[i].memoryOffset,
                  forUnmarshaling.pBinds[i].memoryOffset);
        EXPECT_EQ(forMarshaling.pBinds[i].subresource.arrayLayer,
                  forUnmarshaling.pBinds[i].subresource.arrayLayer);
    }

    checkEqual_VkSparseImageMemoryBindInfo(
        &forMarshaling, &forUnmarshaling, [](const char* errMsg) {
        EXPECT_TRUE(false) << errMsg;
    });
}

// Try a Vulkan struct that has ptr fields that are not structs
TEST(VulkanStream, testMarshalVulkanStructWithSimplePtrFields) {
    VulkanStreamForTesting stream;

    const uint32_t queueCount = 4;

    std::vector<float> queuePriorities;

    for (uint32_t i = 0; i < queueCount; i++) {
        queuePriorities.push_back(i * 4.0f);
    }

    std::vector<float> queuePrioritiesForUnmarshaling = queuePriorities;
    memset(queuePrioritiesForUnmarshaling.data(), 0, queuePriorities.size() * sizeof(float));

    VkDeviceQueueCreateInfo forMarshaling = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        0,
        0,
        1,
        queueCount,
        queuePriorities.data(),
    };

    VkDeviceQueueCreateInfo forUnmarshaling = {
        VK_STRUCTURE_TYPE_APPLICATION_INFO,
        0,
        0,
        0,
        queueCount,
        queuePrioritiesForUnmarshaling.data(),
    };

    marshal_VkDeviceQueueCreateInfo(&stream, &forMarshaling);
    unmarshal_VkDeviceQueueCreateInfo(&stream, &forUnmarshaling);

    // As always, test the autogenerated tester.
    for (uint32_t i = 0; i < queueCount; i++) {
        EXPECT_EQ(forMarshaling.pQueuePriorities[i], forUnmarshaling.pQueuePriorities[i]);
    }

    checkEqual_VkDeviceQueueCreateInfo(
        &forMarshaling, &forUnmarshaling, [](const char* errMsg) {
        EXPECT_TRUE(false) << errMsg;
    });
}

// Vulkan struct with a void* field that refers to actual data
// that needs to get transmitted over
TEST(VulkanStream, testMarshalVulkanStructWithVoidPtrToData) {
    VulkanStreamForTesting stream;

    // Not going to validate the map entries---
    // that's the validation layer's job,
    // and this is just to make sure values match.
    const uint32_t numEntries = 5;
    const size_t dataSize = 54;

    std::vector<VkSpecializationMapEntry> entries(numEntries);

    for (uint32_t i = 0; i < numEntries; i++) {
        entries[i].constantID = 8 * i + 0;
        entries[i].offset = 8 * i + 1;
        entries[i].size = 8 * i + 2;
    }

    std::vector<uint8_t> data(dataSize);

    for (size_t i = 0; i < dataSize; i++) {
        data[i] = (uint8_t)i;
    }

    VkSpecializationInfo forMarshaling = {
        numEntries,
        entries.data(),
        dataSize,
        data.data(),
    };

    VkSpecializationInfo forUnmarshaling;
    memset(&forUnmarshaling, 0x0, sizeof(VkSpecializationInfo));

    std::vector<VkSpecializationMapEntry> entriesForUnmarshaling(numEntries);
    std::vector<uint8_t> dataForUnmarshaling(dataSize);

    forUnmarshaling.pMapEntries = entriesForUnmarshaling.data();
    forUnmarshaling.pData = dataForUnmarshaling.data();

    int inequalities = 0;
    checkEqual_VkSpecializationInfo(
        &forMarshaling, &forUnmarshaling, [&inequalities](const char* errMsg) {
        (void)errMsg;
        ++inequalities;
    });

    EXPECT_GT(inequalities, 0);

    marshal_VkSpecializationInfo(&stream, &forMarshaling);
    unmarshal_VkSpecializationInfo(&stream, &forUnmarshaling);

    EXPECT_EQ(data, dataForUnmarshaling);

    checkEqual_VkSpecializationInfo(
        &forMarshaling, &forUnmarshaling, [](const char* errMsg) {
        EXPECT_TRUE(false) << errMsg;
    });
}

} // namespace goldfish_vk
