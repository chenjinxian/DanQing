// SPDX-License-Identifier: Apache-2.0
// Authored: DanQing test infrastructure — a no-op concrete rhi::Driver for Driver-based unit tests.
// No equivalent in itwinjs-core / filament (filament uses an X-macro DriverAPI.inc; DanQing declares
// Driver methods explicitly, so this stub list is transcribed 1:1 from rhi/Driver.h).
//
// NullDriver implements every pure-virtual of rhi::Driver as a no-op: void methods empty; value/handle
// returns default-constructed. It is a REUSABLE base: any test that needs to drive code that takes a
// rhi::Driver& derives NullDriver and overrides only the methods it cares about (see MockDriver). This
// unlocks runtime verification of GPU-upload logic WITHOUT a real GL context (portable; runs in ctest).
//
// Header-only with compact unnamed-parameter signatures to keep the 154 stubs scannable. If rhi::Driver
// gains pure-virtuals, this fails to compile until they are stubbed — a desirable forcing function.
#pragma once

#include "dqRender/rhi/Driver.h"

#include <functional>

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

class NullDriver : public Driver {
public:
    // --- Frame lifecycle ---
    void beginFrame(int64_t, int64_t, uint32_t) noexcept override {}
    void endFrame(uint32_t) noexcept override {}
    void flush() noexcept override {}
    void terminate() noexcept override {}

    // --- Swap chain ---
    SwapChainHandle createSwapChain(void*, uint64_t) noexcept override { return {}; }
    SwapChainHandle createSwapChainHeadless(uint32_t, uint32_t, uint64_t) noexcept override { return {}; }
    void destroySwapChain(SwapChainHandle) noexcept override {}
    void makeCurrent(SwapChainHandle, SwapChainHandle) noexcept override {}
    void commit(SwapChainHandle) noexcept override {}

    // --- Program ---
    ProgramHandle createProgram(Program&&) noexcept override { return {}; }
    void destroyProgram(ProgramHandle) noexcept override {}

    // --- Vertex / Index buffers ---
    VertexBufferInfoHandle createVertexBufferInfo(uint8_t, uint8_t, AttributeArray const&) noexcept override { return {}; }
    void destroyVertexBufferInfo(VertexBufferInfoHandle) noexcept override {}
    VertexBufferHandle createVertexBuffer(uint32_t, VertexBufferInfoHandle) noexcept override { return {}; }
    void destroyVertexBuffer(VertexBufferHandle) noexcept override {}
    IndexBufferHandle createIndexBuffer(ElementType, uint32_t, BufferUsage) noexcept override { return {}; }
    void destroyIndexBuffer(IndexBufferHandle) noexcept override {}

    // --- Buffer objects ---
    BufferObjectHandle createBufferObject(uint32_t, BufferObjectBinding, BufferUsage) noexcept override { return {}; }
    void destroyBufferObject(BufferObjectHandle) noexcept override {}
    void updateBufferObject(BufferObjectHandle, BufferDescriptor&&, uint32_t) noexcept override {}
    void setVertexBufferObject(VertexBufferHandle, uint32_t, BufferObjectHandle) noexcept override {}

    // --- Render primitive ---
    RenderPrimitiveHandle createRenderPrimitive(VertexBufferHandle, IndexBufferHandle, PrimitiveType) noexcept override { return {}; }
    void destroyRenderPrimitive(RenderPrimitiveHandle) noexcept override {}
    void bindRenderPrimitive(RenderPrimitiveHandle) noexcept override {}

    // --- Render target ---
    RenderTargetHandle createDefaultRenderTarget() noexcept override { return {}; }
    RenderTargetHandle createRenderTarget(TargetBufferFlags, uint32_t, uint32_t, uint8_t, uint8_t) noexcept override { return {}; }
    void destroyRenderTarget(RenderTargetHandle) noexcept override {}

    // --- Render target attachment access ---
    uint8_t getRenderTargetColorAttachmentCount(RenderTargetHandle) noexcept override { return 0; }
    TextureHandle getRenderTargetColorAttachment(RenderTargetHandle, uint8_t) noexcept override { return {}; }

    // --- Render pass ---
    void beginRenderPass(RenderTargetHandle, RenderPassParams const&) noexcept override {}
    void endRenderPass() noexcept override {}

    // --- Pipeline state ---
    void bindPipeline(PipelineState const&) noexcept override {}
    void useProgram(ProgramHandle) noexcept override {}
    void bindDescriptorSet(DescriptorSetHandle, descriptor_set_t, uint32_t const*, uint32_t) noexcept override {}

    // --- Drawing ---
    void draw2(uint32_t, uint32_t, uint32_t) noexcept override {}
    void drawArrays(uint32_t, uint32_t, uint32_t) noexcept override {}
    void setVertexAttribDivisor(uint32_t, uint32_t) noexcept override {}
    void bindInstanceBuffer(BufferObjectHandle, uint32_t, uint32_t, uint32_t, uint32_t) noexcept override {}

    // --- Texture ---
    TextureHandle createTexture(SamplerType, uint8_t, TextureFormat, uint32_t, uint32_t, uint32_t, TextureUsage) noexcept override { return {}; }
    void destroyTexture(TextureHandle) noexcept override {}
    void setTextureData(TextureHandle, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, PixelBufferDescriptor&&) noexcept override {}
    TextureHandle createTextureView(TextureHandle, uint8_t, uint8_t) noexcept override { return {}; }
    void generateMipmaps(TextureHandle) noexcept override {}
    void bindTexture(uint32_t, TextureHandle) noexcept override {}

    // --- Index buffer / 3D texture update ---
    void updateIndexBuffer(IndexBufferHandle, BufferDescriptor&&, uint32_t) noexcept override {}
    void update3DImage(TextureHandle, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, PixelBufferDescriptor&&) noexcept override {}

    // --- Finish / tick / compile ---
    void finish() noexcept override {}
    void tick() noexcept override {}
    void compilePrograms() noexcept override {}

    // --- Fence/Sync ---
    FenceHandle createFence() noexcept override { return {}; }
    FenceStatus getFenceStatus(FenceHandle) noexcept override { return FenceStatus{}; }
    void destroyFence(FenceHandle) noexcept override {}

    // --- MRT render target ---
    RenderTargetHandle createRenderTargetMRT(TargetBufferFlags, uint32_t, uint32_t, uint8_t, uint8_t, uint8_t, TextureFormat const*, TextureFormat) noexcept override { return {}; }

    // --- Blit/Resolve ---
    void blit(TargetBufferFlags, RenderTargetHandle, Viewport const&, RenderTargetHandle, Viewport const&) noexcept override {}
    void resolve(RenderTargetHandle, RenderTargetHandle) noexcept override {}
    void blitToDefaultFramebuffer(RenderTargetHandle, Viewport const&) noexcept override {}

    // --- GL state / program id ---
    void resetGlState() noexcept override {}
    uint32_t getGlProgramId(ProgramHandle) noexcept override { return 0; }

    // --- Read-back ---
    void readPixels(RenderTargetHandle, uint32_t, uint32_t, uint32_t, uint32_t, PixelBufferDescriptor&&, uint32_t) noexcept override {}
    void readTexture(TextureHandle, uint32_t, PixelBufferDescriptor&&) noexcept override {}

    // --- Capability queries (const) ---
    bool isTextureFormatSupported(TextureFormat) const noexcept override { return false; }
    size_t getMaxTextureSize(SamplerType) const noexcept override { return 0; }
    uint8_t getMaxDrawBuffers() const noexcept override { return 0; }
    size_t getMaxUniformBufferSize() const noexcept override { return 0; }
    size_t getMaxArrayTextureLayers() const noexcept override { return 0; }
    bool isRenderTargetFormatSupported(TextureFormat) const noexcept override { return false; }
    bool isFrameBufferFetchSupported() const noexcept override { return false; }
    bool isDepthClampSupported() const noexcept override { return false; }
    bool isSRGBSwapChainSupported() const noexcept override { return false; }
    uint32_t getUniformBufferOffsetAlignment() const noexcept override { return 0; }

    // --- Scissor / debug markers ---
    void scissor(Viewport const&) noexcept override {}
    void pushGroupMarker(char const*) noexcept override {}
    void popGroupMarker() noexcept override {}

    // === Phase 5 ===
    void resetState() noexcept override {}
    void setPresentationTime(int64_t) noexcept override {}
    FenceStatus fenceWait(FenceHandle, uint64_t) noexcept override { return FenceStatus{}; }
    void fenceCancel(FenceHandle) noexcept override {}
    SyncHandle createSync() noexcept override { return {}; }
    void destroySync(SyncHandle) noexcept override {}
    TimerQueryHandle createTimerQuery() noexcept override { return {}; }
    void destroyTimerQuery(TimerQueryHandle) noexcept override {}
    void beginTimerQuery(TimerQueryHandle) noexcept override {}
    void endTimerQuery(TimerQueryHandle) noexcept override {}
    TimerQueryResult getTimerQueryValue(TimerQueryHandle, uint64_t*) noexcept override { return TimerQueryResult{}; }

    DescriptorSetLayoutHandle createDescriptorSetLayout(DescriptorSetLayout&&) noexcept override { return {}; }
    void destroyDescriptorSetLayout(DescriptorSetLayoutHandle) noexcept override {}
    DescriptorSetHandle createDescriptorSet(DescriptorSetLayoutHandle) noexcept override { return {}; }
    void destroyDescriptorSet(DescriptorSetHandle) noexcept override {}
    void updateDescriptorSetBuffer(DescriptorSetHandle, descriptor_binding_t, BufferObjectHandle, uint32_t, uint32_t) noexcept override {}
    void updateDescriptorSetTexture(DescriptorSetHandle, descriptor_binding_t, TextureHandle, SamplerParams const&) noexcept override {}

    TextureHandle createTextureViewSwizzle(TextureHandle, TextureSwizzle, TextureSwizzle, TextureSwizzle, TextureSwizzle) noexcept override { return {}; }
    TextureHandle createTextureExternalImage(SamplerType, TextureFormat, uint32_t, uint32_t, TextureUsage, void*) noexcept override { return {}; }
    TextureHandle createTextureExternalImage2(SamplerType, TextureFormat, uint32_t, uint32_t, TextureUsage, void*) noexcept override { return {}; }
    TextureHandle createTextureExternalImagePlane(TextureFormat, uint32_t, uint32_t, TextureUsage, void*, uint32_t) noexcept override { return {}; }
    TextureHandle importTexture(intptr_t, SamplerType, uint8_t, TextureFormat, uint8_t, uint32_t, uint32_t, uint32_t, TextureUsage) noexcept override { return {}; }
    void setExternalStream(TextureHandle, StreamHandle) noexcept override {}

    void updateBufferObjectUnsynchronized(BufferObjectHandle, BufferDescriptor&&, uint32_t) noexcept override {}
    void resetBufferObject(BufferObjectHandle) noexcept override {}
    void nextSubpass() noexcept override {}
    void draw(PipelineState const&, RenderPrimitiveHandle, uint32_t, uint32_t, uint32_t) noexcept override {}
    void dispatchCompute(ProgramHandle, uint32_t, uint32_t, uint32_t) noexcept override {}
    void readBufferSubData(BufferObjectHandle, uint32_t, uint32_t, BufferDescriptor&&) noexcept override {}
    void* mapBuffer(BufferObjectHandle, size_t, size_t, uint32_t) noexcept override { return nullptr; }
    void unmapBuffer(BufferObjectHandle) noexcept override {}
    void setPushConstant(ShaderStage, uint8_t, int32_t) noexcept override {}
    void insertEventMarker(char const*) noexcept override {}
    void startCapture() noexcept override {}
    void stopCapture() noexcept override {}

    bool isTextureSwizzleSupported() const noexcept override { return false; }
    bool isTextureFormatMipmappable(TextureFormat) const noexcept override { return false; }
    bool isTextureFormatFilterable(TextureFormat) const noexcept override { return false; }
    bool isFrameBufferFetchMultiSampleSupported() const noexcept override { return false; }
    bool isFrameTimeSupported() const noexcept override { return false; }
    bool isAutoDepthResolveSupported() const noexcept override { return false; }
    bool isMSAASwapChainSupported(uint32_t) const noexcept override { return false; }
    bool isProtectedContentSupported() const noexcept override { return false; }
    bool isStereoSupported() const noexcept override { return false; }
    bool isParallelShaderCompileSupported() const noexcept override { return false; }
    bool isDepthStencilResolveSupported() const noexcept override { return false; }
    bool isDepthStencilBlitSupported(TextureFormat) const noexcept override { return false; }
    bool isProtectedTexturesSupported() const noexcept override { return false; }
    bool isAsynchronousModeEnabled() const noexcept override { return false; }
    bool isWorkaroundNeeded(uint32_t) const noexcept override { return false; }
    ShaderModel getShaderModel() const noexcept override { return ShaderModel{}; }

    StreamHandle createStreamNative(void*) noexcept override { return {}; }
    StreamHandle createStreamAcquired() noexcept override { return {}; }
    void destroyStream(StreamHandle) noexcept override {}
    void setAcquiredImage(StreamHandle, void*, void*) noexcept override {}
    void setStreamDimensions(StreamHandle, uint32_t, uint32_t) noexcept override {}
    int64_t getStreamTimestamp(StreamHandle) noexcept override { return 0; }
    void updateStreams() noexcept override {}
    void setupExternalImage(void*) noexcept override {}
    void setupExternalImage2(void*) noexcept override {}
    void setFrameRate(SwapChainHandle, float) noexcept override {}
    bool queryFrameTimestamps(SwapChainHandle, uint64_t, void*) noexcept override { return false; }
    bool isCompositorTimingSupported() const noexcept override { return false; }
    bool queryCompositorTiming(SwapChainHandle, void*) noexcept override { return false; }

    // === Phase 6: async + callbacks ===
    VertexBufferHandle createVertexBufferAsync(uint32_t, VertexBufferInfoHandle, CompletionCallback, void*) noexcept override { return {}; }
    IndexBufferHandle createIndexBufferAsync(ElementType, uint32_t, BufferUsage, CompletionCallback, void*) noexcept override { return {}; }
    BufferObjectHandle createBufferObjectAsync(uint32_t, BufferObjectBinding, BufferUsage, CompletionCallback, void*) noexcept override { return {}; }
    TextureHandle createTextureAsync(SamplerType, uint8_t, TextureFormat, uint8_t, uint32_t, uint32_t, uint32_t, TextureUsage, CompletionCallback, void*) noexcept override { return {}; }
    TextureHandle createTextureViewSwizzleAsync(TextureHandle, TextureSwizzle, TextureSwizzle, TextureSwizzle, TextureSwizzle, CompletionCallback, void*) noexcept override { return {}; }
    TextureHandle importTextureAsync(intptr_t, SamplerType, uint8_t, TextureFormat, uint8_t, uint32_t, uint32_t, uint32_t, TextureUsage, CompletionCallback, void*) noexcept override { return {}; }
    void updateIndexBufferAsync(IndexBufferHandle, BufferDescriptor&&, uint32_t, CompletionCallback, void*) noexcept override {}
    void updateBufferObjectAsync(BufferObjectHandle, BufferDescriptor&&, uint32_t, CompletionCallback, void*) noexcept override {}
    void update3DImageAsync(TextureHandle, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, PixelBufferDescriptor&&, CompletionCallback, void*) noexcept override {}
    void setFrameScheduledCallback(SwapChainHandle, FrameScheduledCallback, void*) noexcept override {}
    void setFrameCompletedCallback(SwapChainHandle, CompletionCallback, void*) noexcept override {}
    void getPlatformSync(SyncHandle, SyncCallback, void*) noexcept override {}
    void copyToMemoryMappedBuffer(void*, size_t, BufferDescriptor&&) noexcept override {}
    void queueCommandAsync(CompletionCallback, void*) noexcept override {}
    bool cancelAsyncJob(uint64_t) noexcept override { return false; }

    // === Phase 6: base class virtuals ===
    void purge() noexcept override {}
    void scheduleCallback(CompletionCallback, void*) noexcept override {}
    void setUnrecoverableError() noexcept override {}
    void execute(std::function<void()> const&) override {}
    void debugCommandBegin(char const*, bool) noexcept override {}
    void debugCommandEnd(char const*, bool) noexcept override {}
};

}  // namespace rhi
END_DQ_RENDER_NAMESPACE
