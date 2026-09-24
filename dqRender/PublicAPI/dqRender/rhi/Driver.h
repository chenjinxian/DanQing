// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — RHI Driver abstract base class
// Ported from: filament backend/include/private/backend/Driver.h
//
// The Driver is the abstract RHI interface.  All GPU operations go through
// this interface.  Concrete backends (OpenGL, Vulkan, Metal) implement it.
//
// Phase 0: minimal method set (~20 methods) for a single triangle.
// Phase 1+: full 144-command surface.
#pragma once

#include "BufferDescriptor.h"
#include "DriverEnums.h"
#include "Handle.h"
#include "PipelineState.h"
#include "dqRender/RenderMemory.h"
#include "Program.h"

#include <cstdint>
#include <functional>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

// Forward declarations
struct HwBase;
struct HwVertexBufferInfo;
struct HwVertexBuffer;
struct HwBufferObject;
struct HwIndexBuffer;
struct HwRenderPrimitive;
struct HwProgram;
struct HwTexture;
struct HwRenderTarget;
struct HwSwapChain;
struct HwFence;

// ---------------------------------------------------------------------------
// Driver — abstract RHI interface (Phase 0 minimal subset)
// ---------------------------------------------------------------------------
class Driver {
public:
    virtual ~Driver() = default;

    // --- Frame lifecycle ---
    virtual void beginFrame(int64_t monotonicClockNs, int64_t refreshIntervalNs,
                            uint32_t frameId) noexcept = 0;
    virtual void endFrame(uint32_t frameId) noexcept = 0;
    virtual void flush() noexcept = 0;
    virtual void terminate() noexcept = 0;

    // --- Swap chain ---
    virtual SwapChainHandle createSwapChain(void* nativeWindow, uint64_t flags) noexcept = 0;
    virtual SwapChainHandle createSwapChainHeadless(uint32_t width, uint32_t height,
                                                    uint64_t flags) noexcept = 0;
    virtual void destroySwapChain(SwapChainHandle sch) noexcept = 0;
    virtual void makeCurrent(SwapChainHandle schDraw, SwapChainHandle schRead) noexcept = 0;
    virtual void commit(SwapChainHandle sch) noexcept = 0;

    // --- Program ---
    virtual ProgramHandle createProgram(Program&& program) noexcept = 0;
    virtual void destroyProgram(ProgramHandle ph) noexcept = 0;

    // --- Vertex / Index buffers ---
    virtual VertexBufferInfoHandle createVertexBufferInfo(
        uint8_t bufferCount, uint8_t attributeCount,
        AttributeArray const& attributes) noexcept = 0;
    virtual void destroyVertexBufferInfo(VertexBufferInfoHandle vbih) noexcept = 0;

    virtual VertexBufferHandle createVertexBuffer(
        uint32_t vertexCount, VertexBufferInfoHandle vbih) noexcept = 0;
    virtual void destroyVertexBuffer(VertexBufferHandle vbh) noexcept = 0;

    virtual IndexBufferHandle createIndexBuffer(
        ElementType elementType, uint32_t indexCount,
        BufferUsage usage) noexcept = 0;
    virtual void destroyIndexBuffer(IndexBufferHandle ibh) noexcept = 0;

    // --- Buffer objects ---
    virtual BufferObjectHandle createBufferObject(
        uint32_t byteCount, BufferObjectBinding bindingType,
        BufferUsage usage) noexcept = 0;
    virtual void destroyBufferObject(BufferObjectHandle boh) noexcept = 0;
    virtual void updateBufferObject(BufferObjectHandle boh,
                                    BufferDescriptor&& data,
                                    uint32_t byteOffset) noexcept = 0;
    virtual void setVertexBufferObject(VertexBufferHandle vbh, uint32_t index,
                                       BufferObjectHandle bufferObject) noexcept = 0;

    // --- Render primitive ---
    virtual RenderPrimitiveHandle createRenderPrimitive(
        VertexBufferHandle vbh, IndexBufferHandle ibh,
        PrimitiveType pt) noexcept = 0;
    virtual void destroyRenderPrimitive(RenderPrimitiveHandle rph) noexcept = 0;
    virtual void bindRenderPrimitive(RenderPrimitiveHandle rph) noexcept = 0;

    // --- Render target ---
    virtual RenderTargetHandle createDefaultRenderTarget() noexcept = 0;
    virtual RenderTargetHandle createRenderTarget(
        TargetBufferFlags flags, uint32_t width, uint32_t height,
        uint8_t samples, uint8_t layerCount) noexcept = 0;
    virtual void destroyRenderTarget(RenderTargetHandle rth) noexcept = 0;

    // --- Render target attachment access (for OIT MRT) ---
    virtual uint8_t getRenderTargetColorAttachmentCount(RenderTargetHandle rth) noexcept = 0;
    virtual TextureHandle getRenderTargetColorAttachment(RenderTargetHandle rth,
                                                         uint8_t index) noexcept = 0;

    // --- Render pass ---
    virtual void beginRenderPass(RenderTargetHandle rth,
                                 RenderPassParams const& params) noexcept = 0;
    virtual void endRenderPass() noexcept = 0;

    // --- Pipeline state ---
    virtual void bindPipeline(PipelineState const& state) noexcept = 0;
    // Bind ONLY the program (WebGL gl.useProgram) — no raster/blend/stencil state.
    // Ported from: itwinjs-core ShaderProgram.use() which calls gl.useProgram without
    // touching render state; render state stays under RenderState::apply's control.
    // bindPipeline's filament-default RasterState (culling=BACK, depth on) must NOT
    // stomp the itwinjs RenderState applied by the compositor.
    virtual void useProgram(ProgramHandle ph) noexcept = 0;
    virtual void bindDescriptorSet(DescriptorSetHandle dsh, descriptor_set_t set,
                                   uint32_t const* offsets,
                                   uint32_t offsetCount) noexcept = 0;

    // --- Drawing ---
    virtual void draw2(uint32_t indexOffset, uint32_t indexCount,
                       uint32_t instanceCount) noexcept = 0;
    virtual void drawArrays(uint32_t vertexOffset, uint32_t vertexCount,
                            uint32_t instanceCount) noexcept = 0;

    // --- Vertex attribute divisor (for instanced rendering) ---
    // Set the divisor for a vertex attribute (0 = per-vertex, 1 = per-instance).
    // Ported from: glVertexAttribDivisor
    virtual void setVertexAttribDivisor(uint32_t location, uint32_t divisor) noexcept = 0;

    // Bind a buffer object as a vertex attribute (for instanced rendering).
    // Binds the buffer and sets up the vertex attribute pointer.
    // Ported from: glBindBuffer + glVertexAttribPointer + glVertexAttribDivisor
    virtual void bindInstanceBuffer(BufferObjectHandle boh,
                                    uint32_t location, uint32_t components,
                                    uint32_t stride, uint32_t offset) noexcept = 0;

    // --- Texture ---
    virtual TextureHandle createTexture(SamplerType target, uint8_t levels,
                                        TextureFormat format, uint32_t width,
                                        uint32_t height, uint32_t depth,
                                        TextureUsage usage) noexcept = 0;
    virtual void destroyTexture(TextureHandle th) noexcept = 0;
    virtual void setTextureData(TextureHandle th, uint32_t level,
                                uint32_t x, uint32_t y, uint32_t z,
                                uint32_t width, uint32_t height, uint32_t depth,
                                PixelBufferDescriptor&& data) noexcept = 0;

    /// Set a texture's wrap mode (wrapS/wrapT as raw GL values: GL_REPEAT / GL_CLAMP_TO_EDGE).
    /// Ported from: itwinjs-core Texture.ts:87-88 — gl.texParameteri(TEXTURE_WRAP_S/T, params.wrapMode).
    /// Default no-op: the OpenGL backend clamps at creation; callers needing glTF
    /// semantics (spec default wrapS/T = REPEAT) apply it explicitly after upload.
    virtual void setTextureWrapMode(TextureHandle th, uint32_t wrapS, uint32_t wrapT) noexcept
    {
        (void)th; (void)wrapS; (void)wrapT;
    }
    virtual TextureHandle createTextureView(TextureHandle src, uint8_t baseLevel,
                                            uint8_t levelCount) noexcept = 0;
    virtual void generateMipmaps(TextureHandle th) noexcept = 0;

    /// Bind a texture to a texture unit for sampler access.
    /// Ported from: filament Driver::bindTexture()
    virtual void bindTexture(uint32_t unit, TextureHandle th) noexcept = 0;

    /// Aggregate texture-memory statistics owned by this driver.
    /// Ported from: itwinjs-core RenderSystem.collectStatistics (the
    /// MemoryTracker "Textures" consumer slice). Default: no statistics
    /// (backends without a tracking hook report zero — registered gap).
    virtual void collectTextureStatistics(RenderMemory::Statistics& /*stats*/) const noexcept {}

    // --- Index buffer update ---
    virtual void updateIndexBuffer(IndexBufferHandle ibh, BufferDescriptor&& data,
                                   uint32_t byteOffset) noexcept = 0;

    // --- 3D texture update ---
    virtual void update3DImage(TextureHandle th, uint32_t level,
                               uint32_t x, uint32_t y, uint32_t z,
                               uint32_t width, uint32_t height, uint32_t depth,
                               PixelBufferDescriptor&& data) noexcept = 0;

    // --- Finish / tick ---
    virtual void finish() noexcept = 0;
    virtual void tick() noexcept = 0;

    // --- Program compilation ---
    virtual void compilePrograms() noexcept = 0;

    // --- Fence/Sync (Phase 4) ---
    virtual FenceHandle createFence() noexcept = 0;
    virtual FenceStatus getFenceStatus(FenceHandle fh) noexcept = 0;
    virtual void destroyFence(FenceHandle fh) noexcept = 0;

    // --- MRT Render target (Phase 4) ---
    virtual RenderTargetHandle createRenderTargetMRT(
        TargetBufferFlags flags, uint32_t width, uint32_t height,
        uint8_t samples, uint8_t layerCount,
        uint8_t colorAttachmentCount,
        TextureFormat const* colorFormats,
        TextureFormat depthFormat) noexcept = 0;

    // --- Blit/Resolve (Phase 4) ---
    virtual void blit(TargetBufferFlags buffers,
                      RenderTargetHandle dst, Viewport const& dstViewport,
                      RenderTargetHandle src, Viewport const& srcViewport) noexcept = 0;
    virtual void resolve(RenderTargetHandle dst, RenderTargetHandle src) noexcept = 0;

    // Blit from a render target to the default framebuffer (FBO 0 / screen).
    // Used to display the compositor's off-screen rendering result on screen.
    virtual void blitToDefaultFramebuffer(RenderTargetHandle src,
                                          Viewport const& srcViewport) noexcept = 0;

    // Reset the internal GL state cache. Call this when external code
    // (e.g., an external GL context owner) may have changed GL state behind the driver's back.
    virtual void resetGlState() noexcept = 0;

    // Get the native GL program ID from a ProgramHandle.
    // Returns 0 if the handle is invalid.
    virtual uint32_t getGlProgramId(ProgramHandle ph) noexcept = 0;

    // --- Read-back ---
    // colorAttachment：MRT 读附件下标（默认 0；参考 WebGL 的 glReadBuffer 语义——
    // SceneCompositor 回读 featureId/depthAndOrder 附件即按附件序读）。
    virtual void readPixels(RenderTargetHandle src, uint32_t x, uint32_t y,
                            uint32_t width, uint32_t height,
                            PixelBufferDescriptor&& data,
                            uint32_t colorAttachment = 0) noexcept = 0;
    virtual void readTexture(TextureHandle th, uint32_t level,
                             PixelBufferDescriptor&& data) noexcept = 0;

    // --- Queries ---
    virtual bool isTextureFormatSupported(TextureFormat format) const noexcept = 0;
    virtual size_t getMaxTextureSize(SamplerType target) const noexcept = 0;
    virtual uint8_t getMaxDrawBuffers() const noexcept = 0;
    virtual size_t getMaxUniformBufferSize() const noexcept = 0;
    virtual size_t getMaxArrayTextureLayers() const noexcept = 0;
    virtual bool isRenderTargetFormatSupported(TextureFormat format) const noexcept = 0;
    virtual bool isFrameBufferFetchSupported() const noexcept = 0;
    virtual bool isDepthClampSupported() const noexcept = 0;
    virtual bool isSRGBSwapChainSupported() const noexcept = 0;
    virtual uint32_t getUniformBufferOffsetAlignment() const noexcept = 0;

    // --- Scissor ---
    virtual void scissor(Viewport const& viewport) noexcept = 0;

    // --- Debug markers (Phase 4) ---
    virtual void pushGroupMarker(char const* label) noexcept = 0;
    virtual void popGroupMarker() noexcept = 0;

    // =======================================================================
    // Phase 5: Complete Filament RHI surface
    // =======================================================================

    // --- Frame lifecycle (extended) ---
    virtual void resetState() noexcept = 0;
    virtual void setPresentationTime(int64_t monotonicClockNs) noexcept = 0;

    // --- Fence/Sync (extended) ---
    virtual FenceStatus fenceWait(FenceHandle fh, uint64_t timeoutNs) noexcept = 0;
    virtual void fenceCancel(FenceHandle fh) noexcept = 0;

    // --- Sync primitives ---
    virtual SyncHandle createSync() noexcept = 0;
    virtual void destroySync(SyncHandle sh) noexcept = 0;

    // --- Timer queries ---
    virtual TimerQueryHandle createTimerQuery() noexcept = 0;
    virtual void destroyTimerQuery(TimerQueryHandle tqh) noexcept = 0;
    virtual void beginTimerQuery(TimerQueryHandle tqh) noexcept = 0;
    virtual void endTimerQuery(TimerQueryHandle tqh) noexcept = 0;
    virtual TimerQueryResult getTimerQueryValue(TimerQueryHandle tqh,
                                                uint64_t* elapsedTime) noexcept = 0;

    // --- Descriptor sets ---
    virtual DescriptorSetLayoutHandle createDescriptorSetLayout(
        DescriptorSetLayout&& info) noexcept = 0;
    virtual void destroyDescriptorSetLayout(DescriptorSetLayoutHandle dslh) noexcept = 0;
    virtual DescriptorSetHandle createDescriptorSet(
        DescriptorSetLayoutHandle dslh) noexcept = 0;
    virtual void destroyDescriptorSet(DescriptorSetHandle dsh) noexcept = 0;
    virtual void updateDescriptorSetBuffer(DescriptorSetHandle dsh,
                                           descriptor_binding_t binding,
                                           BufferObjectHandle boh,
                                           uint32_t offset,
                                           uint32_t size) noexcept = 0;
    virtual void updateDescriptorSetTexture(DescriptorSetHandle dsh,
                                            descriptor_binding_t binding,
                                            TextureHandle th,
                                            SamplerParams const& params) noexcept = 0;

    // --- Texture variants ---
    virtual TextureHandle createTextureViewSwizzle(TextureHandle texture,
                                                   TextureSwizzle r, TextureSwizzle g,
                                                   TextureSwizzle b,
                                                   TextureSwizzle a) noexcept = 0;
    virtual TextureHandle createTextureExternalImage(SamplerType target,
                                                     TextureFormat format,
                                                     uint32_t width, uint32_t height,
                                                     TextureUsage usage,
                                                     void* image) noexcept = 0;
    virtual TextureHandle createTextureExternalImage2(SamplerType target,
                                                      TextureFormat format,
                                                      uint32_t width, uint32_t height,
                                                      TextureUsage usage,
                                                      void* image) noexcept = 0;
    virtual TextureHandle createTextureExternalImagePlane(TextureFormat format,
                                                          uint32_t width, uint32_t height,
                                                          TextureUsage usage, void* image,
                                                          uint32_t plane) noexcept = 0;
    virtual TextureHandle importTexture(intptr_t id, SamplerType target, uint8_t levels,
                                        TextureFormat format, uint8_t samples,
                                        uint32_t width, uint32_t height, uint32_t depth,
                                        TextureUsage usage) noexcept = 0;
    virtual void setExternalStream(TextureHandle th, StreamHandle sh) noexcept = 0;

    // --- Update operations (extended) ---
    virtual void updateBufferObjectUnsynchronized(BufferObjectHandle boh,
                                                  BufferDescriptor&& data,
                                                  uint32_t byteOffset) noexcept = 0;
    virtual void resetBufferObject(BufferObjectHandle boh) noexcept = 0;

    // --- Render operations (extended) ---
    virtual void nextSubpass() noexcept = 0;
    virtual void draw(PipelineState const& state, RenderPrimitiveHandle rph,
                      uint32_t indexOffset, uint32_t indexCount,
                      uint32_t instanceCount) noexcept = 0;
    virtual void dispatchCompute(ProgramHandle program, uint32_t groupsX,
                                 uint32_t groupsY, uint32_t groupsZ) noexcept = 0;

    // --- Read-back (extended) ---
    virtual void readBufferSubData(BufferObjectHandle src, uint32_t offset, uint32_t size,
                                   BufferDescriptor&& data) noexcept = 0;

    // --- Memory-mapped buffers ---
    virtual void* mapBuffer(BufferObjectHandle boh, size_t offset, size_t size,
                            uint32_t accessFlags) noexcept = 0;
    virtual void unmapBuffer(BufferObjectHandle boh) noexcept = 0;

    // --- Push constants ---
    virtual void setPushConstant(ShaderStage stage, uint8_t index,
                                 int32_t value) noexcept = 0;

    // --- Debug / capture (extended) ---
    virtual void insertEventMarker(char const* label) noexcept = 0;
    virtual void startCapture() noexcept = 0;
    virtual void stopCapture() noexcept = 0;

    // --- Capability queries (extended) ---
    virtual bool isTextureSwizzleSupported() const noexcept = 0;
    virtual bool isTextureFormatMipmappable(TextureFormat format) const noexcept = 0;
    virtual bool isTextureFormatFilterable(TextureFormat format) const noexcept = 0;
    virtual bool isFrameBufferFetchMultiSampleSupported() const noexcept = 0;
    virtual bool isFrameTimeSupported() const noexcept = 0;
    virtual bool isAutoDepthResolveSupported() const noexcept = 0;
    virtual bool isMSAASwapChainSupported(uint32_t samples) const noexcept = 0;
    virtual bool isProtectedContentSupported() const noexcept = 0;
    virtual bool isStereoSupported() const noexcept = 0;
    virtual bool isParallelShaderCompileSupported() const noexcept = 0;
    virtual bool isDepthStencilResolveSupported() const noexcept = 0;
    virtual bool isDepthStencilBlitSupported(TextureFormat format) const noexcept = 0;
    virtual bool isProtectedTexturesSupported() const noexcept = 0;
    virtual bool isAsynchronousModeEnabled() const noexcept = 0;
    virtual bool isWorkaroundNeeded(uint32_t workaround) const noexcept = 0;

    // --- Shader model ---
    virtual ShaderModel getShaderModel() const noexcept = 0;

    // --- Streams ---
    virtual StreamHandle createStreamNative(void* stream) noexcept = 0;
    virtual StreamHandle createStreamAcquired() noexcept = 0;
    virtual void destroyStream(StreamHandle sh) noexcept = 0;
    virtual void setAcquiredImage(StreamHandle stream, void* image,
                                  void* transform) noexcept = 0;
    virtual void setStreamDimensions(StreamHandle stream, uint32_t width,
                                     uint32_t height) noexcept = 0;
    virtual int64_t getStreamTimestamp(StreamHandle stream) noexcept = 0;
    virtual void updateStreams() noexcept = 0;

    // --- External image setup ---
    virtual void setupExternalImage(void* image) noexcept = 0;
    virtual void setupExternalImage2(void* image) noexcept = 0;

    // --- Swap chain (extended) ---
    virtual void setFrameRate(SwapChainHandle sch, float frameRate) noexcept = 0;

    // --- Frame timing ---
    virtual bool queryFrameTimestamps(SwapChainHandle sch, uint64_t frameId,
                                      void* outTimestamps) noexcept = 0;
    virtual bool isCompositorTimingSupported() const noexcept = 0;
    virtual bool queryCompositorTiming(SwapChainHandle sch,
                                       void* outTiming) noexcept = 0;

    // =======================================================================
    // Phase 6: Async operations + callbacks for multi-threaded rendering
    // =======================================================================

    // --- Callback types ---
    using CompletionCallback = void (*)(void* user);
    using FrameScheduledCallback = void (*)(void* user);
    using SyncCallback = void (*)(void* user, int32_t status);

    // --- Async resource creation ---
    virtual VertexBufferHandle createVertexBufferAsync(
        uint32_t vertexCount, VertexBufferInfoHandle vbih,
        CompletionCallback callback, void* user) noexcept = 0;
    virtual IndexBufferHandle createIndexBufferAsync(
        ElementType elementType, uint32_t indexCount, BufferUsage usage,
        CompletionCallback callback, void* user) noexcept = 0;
    virtual BufferObjectHandle createBufferObjectAsync(
        uint32_t byteCount, BufferObjectBinding bindingType, BufferUsage usage,
        CompletionCallback callback, void* user) noexcept = 0;
    virtual TextureHandle createTextureAsync(
        SamplerType target, uint8_t levels, TextureFormat format, uint8_t samples,
        uint32_t width, uint32_t height, uint32_t depth, TextureUsage usage,
        CompletionCallback callback, void* user) noexcept = 0;
    virtual TextureHandle createTextureViewSwizzleAsync(
        TextureHandle texture, TextureSwizzle r, TextureSwizzle g,
        TextureSwizzle b, TextureSwizzle a,
        CompletionCallback callback, void* user) noexcept = 0;
    virtual TextureHandle importTextureAsync(
        intptr_t id, SamplerType target, uint8_t levels, TextureFormat format,
        uint8_t samples, uint32_t width, uint32_t height, uint32_t depth,
        TextureUsage usage, CompletionCallback callback, void* user) noexcept = 0;

    // --- Async update operations ---
    virtual void updateIndexBufferAsync(IndexBufferHandle ibh, BufferDescriptor&& data,
                                        uint32_t byteOffset,
                                        CompletionCallback callback,
                                        void* user) noexcept = 0;
    virtual void updateBufferObjectAsync(BufferObjectHandle boh, BufferDescriptor&& data,
                                         uint32_t byteOffset,
                                         CompletionCallback callback,
                                         void* user) noexcept = 0;
    virtual void update3DImageAsync(TextureHandle th, uint32_t level,
                                    uint32_t x, uint32_t y, uint32_t z,
                                    uint32_t width, uint32_t height, uint32_t depth,
                                    PixelBufferDescriptor&& data,
                                    CompletionCallback callback,
                                    void* user) noexcept = 0;

    // --- Frame callbacks ---
    virtual void setFrameScheduledCallback(SwapChainHandle sch,
                                           FrameScheduledCallback callback,
                                           void* user) noexcept = 0;
    virtual void setFrameCompletedCallback(SwapChainHandle sch,
                                           CompletionCallback callback,
                                           void* user) noexcept = 0;

    // --- Sync queries (extended) ---
    virtual void getPlatformSync(SyncHandle sh, SyncCallback callback,
                                 void* user) noexcept = 0;

    // --- Memory-mapped buffer (extended) ---
    virtual void copyToMemoryMappedBuffer(void* mappedPtr, size_t offset,
                                          BufferDescriptor&& data) noexcept = 0;

    // --- Async command execution ---
    virtual void queueCommandAsync(CompletionCallback callback,
                                   void* user) noexcept = 0;
    virtual bool cancelAsyncJob(uint64_t jobId) noexcept = 0;

    // =======================================================================
    // Phase 6: Base class virtuals (Filament Driver.h outside DriverAPI.inc)
    // =======================================================================

    virtual void purge() noexcept = 0;
    virtual void scheduleCallback(CompletionCallback callback, void* user) noexcept = 0;
    virtual void setUnrecoverableError() noexcept = 0;
    virtual void execute(std::function<void()> const& fn) = 0;
    virtual void debugCommandBegin(char const* methodName,
                                   bool synchronous) noexcept = 0;
    virtual void debugCommandEnd(char const* methodName,
                                 bool synchronous) noexcept = 0;
};

}  // namespace rhi
END_DQ_RENDER_NAMESPACE
