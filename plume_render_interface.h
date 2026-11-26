//
// plume
//
// Copyright (c) 2024 renderbag and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE file for details.
//

#pragma once

#include <climits>

#include "plume_render_interface_types.h"

namespace plume {
    // Interfaces.

    struct RenderBufferFormattedView {
        virtual ~RenderBufferFormattedView() { }
    };

    struct RenderBuffer {
        virtual ~RenderBuffer() { }
        virtual void *map(uint32_t subresource = 0, const RenderRange *readRange = nullptr) = 0;
        virtual void unmap(uint32_t subresource = 0, const RenderRange *writtenRange = nullptr) = 0;
        virtual std::unique_ptr<RenderBufferFormattedView> createBufferFormattedView(RenderFormat format) = 0;
        virtual void setName(const std::string &name) = 0;
        virtual uint64_t getDeviceAddress() const = 0;

        // Concrete implementation shortcuts.
        inline RenderBufferReference at(uint64_t offset) const {
            return RenderBufferReference(this, offset);
        }
    };

    struct RenderTextureView {
        virtual ~RenderTextureView() { }
    };

    struct RenderTexture {
        virtual ~RenderTexture() { }
        virtual std::unique_ptr<RenderTextureView> createTextureView(const RenderTextureViewDesc &desc) const = 0;
        virtual void setName(const std::string &name) = 0;
    };

    struct RenderAccelerationStructure {
        virtual ~RenderAccelerationStructure() { }
    };

    struct RenderShader {
        virtual ~RenderShader() { }
        virtual void setName(const std::string &name) = 0;
    };

    struct RenderSampler {
        virtual ~RenderSampler() { }
    };

    struct RenderPipeline {
        virtual ~RenderPipeline() { }
        virtual void setName(const std::string &name) = 0;
        virtual RenderPipelineProgram getProgram(const std::string &name) const = 0;
        // The driver's statistics of each of the pipeline's executables (registers, scratch,
        // instructions), one line each; empty where not captured or not supported.
        virtual std::string getStatistics() const { return {}; }
    };

    struct RenderPipelineLayout {
        virtual ~RenderPipelineLayout() { }
    };

    struct RenderCommandFence {
        virtual ~RenderCommandFence() { }
    };

    struct RenderCommandSemaphore {
        virtual ~RenderCommandSemaphore() { }
    };

    struct RenderDescriptorSet {
        // Descriptor indices correspond to the index assuming the descriptor set is one contiguous array. They DO NOT correspond to the bindings, which can be sparse.
        // User code should derive these indices on its own by looking at the order the bindings were assigned during set creation along with the descriptor count and
        // assume it was all allocated in one contiguous array. This allows efficient mapping between Vulkan and D3D12's descriptor models.

        virtual ~RenderDescriptorSet() { }
        virtual void setBuffer(uint32_t descriptorIndex, const RenderBuffer *buffer, uint64_t bufferSize = 0, const RenderBufferStructuredView *bufferStructuredView = nullptr, const RenderBufferFormattedView *bufferFormattedView = nullptr) = 0;
        virtual void setTexture(uint32_t descriptorIndex, const RenderTexture *texture, RenderTextureLayout textureLayout, const RenderTextureView *textureView = nullptr) = 0;
        virtual void setSampler(uint32_t descriptorIndex, const RenderSampler *sampler) = 0;
        virtual void setAccelerationStructure(uint32_t descriptorIndex, const RenderAccelerationStructure *accelerationStructure) = 0;
    };

    struct RenderSwapChain {
        virtual ~RenderSwapChain() { }
        virtual bool present(uint32_t textureIndex, RenderCommandSemaphore **waitSemaphores, uint32_t waitSemaphoreCount) = 0;
        virtual void wait() = 0;
        // Frames the wait leaves queued (1: the last present on screen before the next frame).
        virtual void setMaxFrameLatency(uint32_t maxFrameLatency) { }
        // VK_KHR_present_wait from another thread: the ID of the last present
        // (0 without the extension), and whether that one is on screen yet:
        // 1 yes, 0 not yet, -1 never (an earlier swapchain's, or an error).
        // It doesn't wait: the swapchain's calls are serialized (Vulkan
        // synchronizes them externally), so a wait would hold up presents.
        virtual uint64_t getPresentId() const { return 0; }
        virtual int pollPresent(uint64_t presentId) { return -1; }
        // VK_EXT_present_timing, asked for from the next resize: wait and
        // pollPresent then go by when frames reached the screen as the
        // compositor reports it (gamescope's present_wait reports when it took
        // the frame) where the surface offers it. hasPresentTiming: in use now.
        // getPresentTime: when that present's first pixel went out (or became
        // visible), CLOCK_MONOTONIC ns, 0 not known (yet). getRefreshDuration:
        // the display's refresh, ns (0 not known).
        virtual void setPresentTiming(bool on) { (void)on; }
        virtual bool hasPresentTiming() const { return false; }
        virtual int64_t getPresentTime(uint64_t presentId) { (void)presentId; return 0; }
        virtual uint64_t getRefreshDuration() { return 0; }
        virtual bool resize() = 0;
        virtual bool needsResize() const = 0;
        virtual void setVsyncEnabled(bool vsyncEnabled) = 0;
        virtual bool isVsyncEnabled() const = 0;

        // How frames are presented (used from the next resize); false if the backend or
        // surface lacks the mode, which then falls back to FIFO.
        virtual bool setPresentMode(RenderPresentMode mode) {
            setVsyncEnabled(mode != RenderPresentMode::IMMEDIATE);
            return (mode == RenderPresentMode::IMMEDIATE) || (mode == RenderPresentMode::FIFO);
        }
        virtual uint32_t getWidth() const = 0;
        virtual uint32_t getHeight() const = 0;
        virtual RenderTexture *getTexture(uint32_t textureIndex) = 0;
        virtual uint32_t getTextureCount() const = 0;
        // The surface's minImageCount: an acquire without a timeout may wait for
        // ever once more than getTextureCount() minus it are held.
        virtual uint32_t getMinTextureCount() const { return 1; }
        // Images asked for from the next resize (the surface's limits still apply).
        virtual void setTextureCount(uint32_t count) { (void)count; }
        // Full screen exclusive (Windows, VK_EXT_full_screen_exclusive), from the
        // next resize: 0 not asked (the driver's default), 1 taken by the
        // application, 2 left to the driver, 3 never (a window covering the
        // screen stays composed, as screen capture needs).
        virtual void setFullScreenExclusive(int mode) { (void)mode; }
        // Whether the swapchain holds the display now (taken, and not lost since).
        virtual bool isFullScreenExclusive() const { return false; }
        virtual bool acquireTexture(RenderCommandSemaphore *signalSemaphore, uint32_t *textureIndex) = 0;
        // Acquires in short waits, the swapchain's lock let go between them (for a
        // caller that presents from another thread); off: one wait, as before.
        virtual void setAcquireInShortWaits(bool shortWaits) { (void)shortWaits; }
        virtual RenderWindow getWindow() const = 0;
        virtual bool isEmpty() const = 0;

        // Only valid if displayTiming is enabled in capabilities.
        virtual uint32_t getRefreshRate() const = 0;
    };

    struct RenderFramebuffer {
        virtual ~RenderFramebuffer() { }
        virtual uint32_t getWidth() const = 0;
        virtual uint32_t getHeight() const = 0;
    };

    struct RenderCommandList {
        virtual ~RenderCommandList() { }
        virtual void begin() = 0;
        virtual void end() = 0;
        virtual void barriers(RenderBarrierStages stages, const RenderBufferBarrier *bufferBarriers, uint32_t bufferBarriersCount, const RenderTextureBarrier *textureBarriers, uint32_t textureBarriersCount) = 0;
        virtual void dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ) = 0;
        virtual void traceRays(uint32_t width, uint32_t height, uint32_t depth, RenderBufferReference shaderBindingTable, const RenderShaderBindingGroupsInfo &shaderBindingGroupsInfo) = 0;
        virtual void drawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation) = 0;
        virtual void drawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation) = 0;
        virtual void setPipeline(const RenderPipeline *pipeline) = 0;
        virtual void setComputePipelineLayout(const RenderPipelineLayout *pipelineLayout) = 0;
        virtual void setComputePushConstants(uint32_t rangeIndex, const void *data, uint32_t offset = 0, uint32_t size = 0) = 0;
        virtual void setComputeDescriptorSet(RenderDescriptorSet *descriptorSet, uint32_t setIndex) = 0;
        virtual void setGraphicsPipelineLayout(const RenderPipelineLayout *pipelineLayout) = 0;
        virtual void setGraphicsPushConstants(uint32_t rangeIndex, const void *data, uint32_t offset = 0, uint32_t size = 0) = 0;
        virtual void setGraphicsDescriptorSet(RenderDescriptorSet *descriptorSet, uint32_t setIndex) = 0;
        virtual void setGraphicsRootDescriptor(RenderBufferReference bufferReference, uint32_t rootDescriptorIndex) = 0;
        virtual void setRaytracingPipelineLayout(const RenderPipelineLayout *pipelineLayout) = 0;
        virtual void setRaytracingPushConstants(uint32_t rangeIndex, const void *data, uint32_t offset = 0, uint32_t size = 0) = 0;
        virtual void setRaytracingDescriptorSet(RenderDescriptorSet *descriptorSet, uint32_t setIndex) = 0;
        virtual void setIndexBuffer(const RenderIndexBufferView *view) = 0;
        virtual void setVertexBuffers(uint32_t startSlot, const RenderVertexBufferView *views, uint32_t viewCount, const RenderInputSlot *inputSlots) = 0;
        virtual void setViewports(const RenderViewport *viewports, uint32_t count) = 0;
        virtual void setScissors(const RenderRect *scissorRects, uint32_t count) = 0;
        virtual void setFramebuffer(const RenderFramebuffer *framebuffer) = 0;
        virtual void setDepthBias(float depthBias, float depthBiasClamp, float slopeScaledDepthBias) = 0;
        // For pipelines made with dynamicBlendConstantsEnabled (Vulkan only).
        virtual void setBlendConstants(const float constants[4]) { (void)constants; }
        virtual void clearColor(uint32_t attachmentIndex = 0, RenderColor colorValue = RenderColor(), const RenderRect *clearRects = nullptr, uint32_t clearRectsCount = 0) = 0;
        virtual void clearDepthStencil(bool clearDepth = true, bool clearStencil = true, float depthValue = 1.0f, uint32_t stencilValue = 0, const RenderRect *clearRects = nullptr, uint32_t clearRectsCount = 0) = 0;
        virtual void copyBufferRegion(RenderBufferReference dstBuffer, RenderBufferReference srcBuffer, uint64_t size) = 0;
        virtual void copyTextureRegion(const RenderTextureCopyLocation &dstLocation, const RenderTextureCopyLocation &srcLocation, uint32_t dstX = 0, uint32_t dstY = 0, uint32_t dstZ = 0, const RenderBox *srcBox = nullptr) = 0;
        virtual void copyBuffer(const RenderBuffer *dstBuffer, const RenderBuffer *srcBuffer) = 0;
        virtual void copyTexture(const RenderTexture *dstTexture, const RenderTexture *srcTexture) = 0;
        virtual void resolveTexture(const RenderTexture *dstTexture, const RenderTexture *srcTexture) = 0;
        virtual void resolveTextureRegion(const RenderTexture *dstTexture, uint32_t dstX, uint32_t dstY, const RenderTexture *srcTexture, const RenderRect *srcRect = nullptr, RenderResolveMode resolveMode = RenderResolveMode::AVERAGE) = 0;
        virtual void buildBottomLevelAS(const RenderAccelerationStructure *dstAccelerationStructure, RenderBufferReference scratchBuffer, const RenderBottomLevelASBuildInfo &buildInfo) = 0;
        virtual void buildTopLevelAS(const RenderAccelerationStructure *dstAccelerationStructure, RenderBufferReference scratchBuffer, RenderBufferReference instancesBuffer, const RenderTopLevelASBuildInfo &buildInfo) = 0;
        virtual void discardTexture(const RenderTexture* texture) = 0; // D3D12 only.
        virtual void resetQueryPool(const RenderQueryPool *queryPool, uint32_t queryFirstIndex, uint32_t queryCount) = 0;
        virtual void writeTimestamp(const RenderQueryPool *queryPool, uint32_t queryIndex) = 0;

        // Concrete implementation shortcuts.
        inline void barriers(RenderBarrierStages stages, const RenderBufferBarrier &barrier) {
            barriers(stages, &barrier, 1, nullptr, 0);
        }

        inline void barriers(RenderBarrierStages stages, const RenderTextureBarrier &barrier) {
            barriers(stages, nullptr, 0, &barrier, 1);
        }

        inline void barriers(RenderBarrierStages stages, const RenderBufferBarrier &bufferBarrier, const RenderTextureBarrier &textureBarrier) {
            barriers(stages, &bufferBarrier, 1, &textureBarrier, 1);
        }

        inline void barriers(RenderBarrierStages stages, const RenderBufferBarrier *bufferBarriers, uint32_t bufferBarriersCount) {
            barriers(stages, bufferBarriers, bufferBarriersCount, nullptr, 0);
        }

        inline void barriers(RenderBarrierStages stages, const std::vector<RenderBufferBarrier> &bufferBarriers) {
            barriers(stages, bufferBarriers.data(), uint32_t(bufferBarriers.size()), nullptr, 0);
        }

        inline void barriers(RenderBarrierStages stages, const RenderTextureBarrier *textureBarriers, uint32_t textureBarriersCount) {
            barriers(stages, nullptr, 0, textureBarriers, textureBarriersCount);
        }

        inline void barriers(RenderBarrierStages stages, const std::vector<RenderTextureBarrier> &textureBarriers) {
            barriers(stages, nullptr, 0, textureBarriers.data(), uint32_t(textureBarriers.size()));
        }

        inline void barriers(RenderBarrierStages stages, const std::vector<RenderBufferBarrier> &bufferBarriers, const std::vector<RenderTextureBarrier> &textureBarriers) {
            barriers(stages, bufferBarriers.data(), uint32_t(bufferBarriers.size()), textureBarriers.data(), uint32_t(textureBarriers.size()));
        }

        inline void setViewports(const RenderViewport &viewport) {
            setViewports(&viewport, 1);
        }

        inline void setScissors(const RenderRect &scissorRect) {
            setScissors(&scissorRect, 1);
        }

        inline void clearDepth(bool clearDepth = true, float depthValue = 1.0f, const RenderRect *clearRects = nullptr, uint32_t clearRectsCount = 0) {
            clearDepthStencil(clearDepth, false, depthValue, 0, clearRects, clearRectsCount);
        }
    };

    struct RenderCommandQueue {
        virtual ~RenderCommandQueue() { }
        virtual std::unique_ptr<RenderCommandList> createCommandList() = 0;
        virtual std::unique_ptr<RenderSwapChain> createSwapChain(RenderWindow renderWindow, uint32_t textureCount, RenderFormat format, uint32_t maxFrameLatency) = 0;
        virtual void executeCommandLists(const RenderCommandList **commandLists, uint32_t commandListCount, RenderCommandSemaphore **waitSemaphores = nullptr, uint32_t waitSemaphoreCount = 0, RenderCommandSemaphore **signalSemaphores = nullptr, uint32_t signalSemaphoreCount = 0, RenderCommandFence *signalFence = nullptr) = 0;
        virtual void waitForCommandFence(RenderCommandFence *fence) = 0;

        // Concrete implementation shortcuts.
        inline void executeCommandLists(const RenderCommandList *commandList, RenderCommandFence *signalFence = nullptr) {
            executeCommandLists(&commandList, 1, nullptr, 0, nullptr, 0, signalFence);
        }
    };

    struct RenderPool {
        virtual ~RenderPool() { }
        virtual std::unique_ptr<RenderBuffer> createBuffer(const RenderBufferDesc &desc) = 0;
        virtual std::unique_ptr<RenderTexture> createTexture(const RenderTextureDesc &desc) = 0;
    };

    struct RenderQueryPool {
        virtual ~RenderQueryPool() { }
        // count: only the first queries (0: all), so unwritten ones at the
        // end don't make the call fail.
        virtual void queryResults(uint32_t count = 0) = 0;
        virtual const uint64_t *getResults() const = 0;
        virtual uint32_t getCount() const = 0;
    };

    struct RenderDevice {
        virtual ~RenderDevice() { }
        virtual std::unique_ptr<RenderDescriptorSet> createDescriptorSet(const RenderDescriptorSetDesc &desc) = 0;
        virtual std::unique_ptr<RenderShader> createShader(const void *data, uint64_t size, const char *entryPointName, RenderShaderFormat format) = 0;
        virtual std::unique_ptr<RenderSampler> createSampler(const RenderSamplerDesc &desc) = 0;
        virtual std::unique_ptr<RenderPipeline> createComputePipeline(const RenderComputePipelineDesc &desc) = 0;
        virtual std::unique_ptr<RenderPipeline> createGraphicsPipeline(const RenderGraphicsPipelineDesc &desc) = 0;
        virtual std::unique_ptr<RenderPipeline> createRaytracingPipeline(const RenderRaytracingPipelineDesc &desc, const RenderPipeline *previousPipeline = nullptr) = 0;
        virtual std::unique_ptr<RenderCommandQueue> createCommandQueue(RenderCommandListType type) = 0;
        virtual std::unique_ptr<RenderBuffer> createBuffer(const RenderBufferDesc &desc) = 0;
        virtual std::unique_ptr<RenderTexture> createTexture(const RenderTextureDesc &desc) = 0;
        virtual std::unique_ptr<RenderAccelerationStructure> createAccelerationStructure(const RenderAccelerationStructureDesc &desc) = 0;
        virtual std::unique_ptr<RenderPool> createPool(const RenderPoolDesc &desc) = 0;
        virtual std::unique_ptr<RenderPipelineLayout> createPipelineLayout(const RenderPipelineLayoutDesc &desc) = 0;
        virtual std::unique_ptr<RenderCommandFence> createCommandFence() = 0;
        virtual std::unique_ptr<RenderCommandSemaphore> createCommandSemaphore() = 0;
        virtual std::unique_ptr<RenderFramebuffer> createFramebuffer(const RenderFramebufferDesc &desc) = 0;
        virtual std::unique_ptr<RenderQueryPool> createQueryPool(uint32_t queryCount) = 0;
        virtual void setBottomLevelASBuildInfo(RenderBottomLevelASBuildInfo &buildInfo, const RenderBottomLevelASMesh *meshes, uint32_t meshCount, bool preferFastBuild = true, bool preferFastTrace = false) = 0;
        virtual void setTopLevelASBuildInfo(RenderTopLevelASBuildInfo &buildInfo, const RenderTopLevelASInstance *instances, uint32_t instanceCount, bool preferFastBuild = true, bool preferFastTrace = false) = 0;
        virtual void setShaderBindingTableInfo(RenderShaderBindingTableInfo &tableInfo, const RenderShaderBindingGroups &groups, const RenderPipeline *pipeline, RenderDescriptorSet **descriptorSets, uint32_t descriptorSetCount) = 0;
        virtual const RenderDeviceCapabilities &getCapabilities() const = 0;
        virtual const RenderDeviceDescription &getDescription() const = 0;
        virtual RenderSampleCounts getSampleCountsSupported(RenderFormat format) const = 0;
        virtual bool beginCapture() = 0;
        virtual bool endCapture() = 0;
    };

    struct RenderInterface {
        virtual ~RenderInterface() { }
        virtual std::unique_ptr<RenderDevice> createDevice(const std::string &preferredDeviceName = "") = 0;
        virtual const std::vector<std::string> &getDeviceNames() const = 0;
        virtual const RenderInterfaceCapabilities &getCapabilities() const = 0;
    };

    extern void RenderInterfaceTest(RenderInterface *renderInterface);
    extern void TestInitialize(RenderInterface* renderInterface, RenderWindow window);
    extern void TestDraw();
    extern void TestResize();
    extern void TestShutdown();
};

#include "plume_render_interface_builders.h"
