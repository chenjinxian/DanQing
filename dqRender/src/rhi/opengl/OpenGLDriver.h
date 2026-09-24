// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — OpenGL RHI driver implementation
// Ported from: filament backend/src/opengl/OpenGLDriver.h
//
// Concrete implementation of the Driver interface using OpenGL.
// Phase 0: minimal subset for a single triangle.
#pragma once

#include "OpenGLContext.h"
#include "OpenGLState.h"
#include "dqRender/rhi/Driver.h"
#include "dqRender/rhi/OpenGLPlatform.h"
#include "dqRender/RenderMemory.h"

#include "rhi/DriverBase.h"
#include "rhi/HandleAllocator.h"

#include <array>
#include <memory>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE
namespace rhi {

// Forward declarations
class OpenGLProgram;

// ---------------------------------------------------------------------------
// OpenGLDriver — OpenGL implementation of Driver
// ---------------------------------------------------------------------------
class OpenGLDriver final : public Driver {
public:
    OpenGLDriver(OpenGLPlatform& platform, void* sharedContext,
                 DriverConfig const& config);
    ~OpenGLDriver() override;

    // --- Driver interface ---
    void beginFrame(int64_t monotonicClockNs, int64_t refreshIntervalNs,
                    uint32_t frameId) noexcept override;
    void endFrame(uint32_t frameId) noexcept override;
    void flush() noexcept override;
    void terminate() noexcept override;

    SwapChainHandle createSwapChain(void* nativeWindow, uint64_t flags) noexcept override;
    SwapChainHandle createSwapChainHeadless(uint32_t width, uint32_t height,
                                            uint64_t flags) noexcept override;
    void destroySwapChain(SwapChainHandle sch) noexcept override;
    void makeCurrent(SwapChainHandle schDraw, SwapChainHandle schRead) noexcept override;
    void commit(SwapChainHandle sch) noexcept override;

    ProgramHandle createProgram(Program&& program) noexcept override;
    void destroyProgram(ProgramHandle ph) noexcept override;

    VertexBufferInfoHandle createVertexBufferInfo(
        uint8_t bufferCount, uint8_t attributeCount,
        AttributeArray const& attributes) noexcept override;
    void destroyVertexBufferInfo(VertexBufferInfoHandle vbih) noexcept override;

    VertexBufferHandle createVertexBuffer(uint32_t vertexCount,
                                          VertexBufferInfoHandle vbih) noexcept override;
    void destroyVertexBuffer(VertexBufferHandle vbh) noexcept override;

    IndexBufferHandle createIndexBuffer(ElementType elementType, uint32_t indexCount,
                                        BufferUsage usage) noexcept override;
    void destroyIndexBuffer(IndexBufferHandle ibh) noexcept override;

    BufferObjectHandle createBufferObject(uint32_t byteCount,
                                          BufferObjectBinding bindingType,
                                          BufferUsage usage) noexcept override;
    void destroyBufferObject(BufferObjectHandle boh) noexcept override;
    void updateBufferObject(BufferObjectHandle boh, BufferDescriptor&& data,
                            uint32_t byteOffset) noexcept override;
    void setVertexBufferObject(VertexBufferHandle vbh, uint32_t index,
                               BufferObjectHandle bufferObject) noexcept override;

    RenderPrimitiveHandle createRenderPrimitive(VertexBufferHandle vbh,
                                                IndexBufferHandle ibh,
                                                PrimitiveType pt) noexcept override;
    void destroyRenderPrimitive(RenderPrimitiveHandle rph) noexcept override;
    void bindRenderPrimitive(RenderPrimitiveHandle rph) noexcept override;

    RenderTargetHandle createDefaultRenderTarget() noexcept override;
    RenderTargetHandle createRenderTarget(TargetBufferFlags flags, uint32_t width,
                                         uint32_t height, uint8_t samples,
                                         uint8_t layerCount) noexcept override;
    void destroyRenderTarget(RenderTargetHandle rth) noexcept override;

    // --- Render target attachment access (for OIT MRT) ---
    uint8_t getRenderTargetColorAttachmentCount(RenderTargetHandle rth) noexcept override;
    TextureHandle getRenderTargetColorAttachment(RenderTargetHandle rth,
                                                 uint8_t index) noexcept override;

    void beginRenderPass(RenderTargetHandle rth,
                         RenderPassParams const& params) noexcept override;
    void endRenderPass() noexcept override;

    void bindPipeline(PipelineState const& state) noexcept override;
    void useProgram(ProgramHandle ph) noexcept override;
    void bindDescriptorSet(DescriptorSetHandle dsh, descriptor_set_t set,
                           uint32_t const* offsets,
                           uint32_t offsetCount) noexcept override;

    void draw2(uint32_t indexOffset, uint32_t indexCount,
               uint32_t instanceCount) noexcept override;
    void drawArrays(uint32_t vertexOffset, uint32_t vertexCount,
                    uint32_t instanceCount) noexcept override;
    void setVertexAttribDivisor(uint32_t location, uint32_t divisor) noexcept override;
    void bindInstanceBuffer(BufferObjectHandle boh,
                            uint32_t location, uint32_t components,
                            uint32_t stride, uint32_t offset) noexcept override;

    void readPixels(RenderTargetHandle src, uint32_t x, uint32_t y,
                    uint32_t width, uint32_t height,
                    PixelBufferDescriptor&& data,
                    uint32_t colorAttachment = 0) noexcept override;

    // --- Fence/Sync (Phase 4) ---
    FenceHandle createFence() noexcept override;
    FenceStatus getFenceStatus(FenceHandle fh) noexcept override;
    void destroyFence(FenceHandle fh) noexcept override;

    // --- MRT Render target (Phase 4) ---
    RenderTargetHandle createRenderTargetMRT(
        TargetBufferFlags flags, uint32_t width, uint32_t height,
        uint8_t samples, uint8_t layerCount,
        uint8_t colorAttachmentCount,
        TextureFormat const* colorFormats,
        TextureFormat depthFormat) noexcept override;

    // --- Blit/Resolve (Phase 4) ---
    void blit(TargetBufferFlags buffers,
              RenderTargetHandle dst, Viewport const& dstViewport,
              RenderTargetHandle src, Viewport const& srcViewport) noexcept override;
    void resolve(RenderTargetHandle dst, RenderTargetHandle src) noexcept override;
    void blitToDefaultFramebuffer(RenderTargetHandle src,
                                  Viewport const& /*srcViewport*/) noexcept override;
    void resetGlState() noexcept override;
    uint32_t getGlProgramId(ProgramHandle ph) noexcept override;

    // --- Read texture (Phase 4) ---
    void readTexture(TextureHandle th, uint32_t level,
                     PixelBufferDescriptor&& data) noexcept override;

    // --- Queries (Phase 4) ---
    bool isTextureFormatSupported(TextureFormat format) const noexcept override;
    size_t getMaxTextureSize(SamplerType target) const noexcept override;
    uint8_t getMaxDrawBuffers() const noexcept override;
    size_t getMaxUniformBufferSize() const noexcept override;
    size_t getMaxArrayTextureLayers() const noexcept override;
    bool isRenderTargetFormatSupported(TextureFormat format) const noexcept override;
    bool isFrameBufferFetchSupported() const noexcept override;
    bool isDepthClampSupported() const noexcept override;
    bool isSRGBSwapChainSupported() const noexcept override;
    uint32_t getUniformBufferOffsetAlignment() const noexcept override;

    // --- Debug markers (Phase 4) ---
    void pushGroupMarker(char const* label) noexcept override;
    void popGroupMarker() noexcept override;

    // --- Phase 5: Complete Filament RHI ---
    void resetState() noexcept override;
    void setPresentationTime(int64_t monotonicClockNs) noexcept override;
    FenceStatus fenceWait(FenceHandle fh, uint64_t timeoutNs) noexcept override;
    void fenceCancel(FenceHandle fh) noexcept override;
    SyncHandle createSync() noexcept override;
    void destroySync(SyncHandle sh) noexcept override;
    TimerQueryHandle createTimerQuery() noexcept override;
    void destroyTimerQuery(TimerQueryHandle tqh) noexcept override;
    void beginTimerQuery(TimerQueryHandle tqh) noexcept override;
    void endTimerQuery(TimerQueryHandle tqh) noexcept override;
    TimerQueryResult getTimerQueryValue(TimerQueryHandle tqh,
                                        uint64_t* elapsedTime) noexcept override;
    DescriptorSetLayoutHandle createDescriptorSetLayout(
        DescriptorSetLayout&& info) noexcept override;
    void destroyDescriptorSetLayout(DescriptorSetLayoutHandle dslh) noexcept override;
    DescriptorSetHandle createDescriptorSet(DescriptorSetLayoutHandle dslh) noexcept override;
    void destroyDescriptorSet(DescriptorSetHandle dsh) noexcept override;
    void updateDescriptorSetBuffer(DescriptorSetHandle dsh, descriptor_binding_t binding,
                                   BufferObjectHandle boh, uint32_t offset,
                                   uint32_t size) noexcept override;
    void updateDescriptorSetTexture(DescriptorSetHandle dsh, descriptor_binding_t binding,
                                    TextureHandle th,
                                    SamplerParams const& params) noexcept override;
    TextureHandle createTextureViewSwizzle(TextureHandle texture, TextureSwizzle r,
                                           TextureSwizzle g, TextureSwizzle b,
                                           TextureSwizzle a) noexcept override;
    TextureHandle createTextureExternalImage(SamplerType target, TextureFormat format,
                                             uint32_t width, uint32_t height,
                                             TextureUsage usage,
                                             void* image) noexcept override;
    TextureHandle createTextureExternalImage2(SamplerType target, TextureFormat format,
                                              uint32_t width, uint32_t height,
                                              TextureUsage usage,
                                              void* image) noexcept override;
    TextureHandle createTextureExternalImagePlane(TextureFormat format, uint32_t width,
                                                  uint32_t height, TextureUsage usage,
                                                  void* image,
                                                  uint32_t plane) noexcept override;
    TextureHandle importTexture(intptr_t id, SamplerType target, uint8_t levels,
                                TextureFormat format, uint8_t samples, uint32_t width,
                                uint32_t height, uint32_t depth,
                                TextureUsage usage) noexcept override;
    void setExternalStream(TextureHandle th, StreamHandle sh) noexcept override;
    void updateBufferObjectUnsynchronized(BufferObjectHandle boh, BufferDescriptor&& data,
                                          uint32_t byteOffset) noexcept override;
    void resetBufferObject(BufferObjectHandle boh) noexcept override;
    void nextSubpass() noexcept override;
    void draw(PipelineState const& state, RenderPrimitiveHandle rph, uint32_t indexOffset,
              uint32_t indexCount, uint32_t instanceCount) noexcept override;
    void dispatchCompute(ProgramHandle program, uint32_t groupsX, uint32_t groupsY,
                         uint32_t groupsZ) noexcept override;
    void readBufferSubData(BufferObjectHandle src, uint32_t offset, uint32_t size,
                           BufferDescriptor&& data) noexcept override;
    void* mapBuffer(BufferObjectHandle boh, size_t offset, size_t size,
                    uint32_t accessFlags) noexcept override;
    void unmapBuffer(BufferObjectHandle boh) noexcept override;
    void setPushConstant(ShaderStage stage, uint8_t index, int32_t value) noexcept override;
    void insertEventMarker(char const* label) noexcept override;
    void startCapture() noexcept override;
    void stopCapture() noexcept override;
    bool isTextureSwizzleSupported() const noexcept override;
    bool isTextureFormatMipmappable(TextureFormat format) const noexcept override;
    bool isTextureFormatFilterable(TextureFormat format) const noexcept override;
    bool isFrameBufferFetchMultiSampleSupported() const noexcept override;
    bool isFrameTimeSupported() const noexcept override;
    bool isAutoDepthResolveSupported() const noexcept override;
    bool isMSAASwapChainSupported(uint32_t samples) const noexcept override;
    bool isProtectedContentSupported() const noexcept override;
    bool isStereoSupported() const noexcept override;
    bool isParallelShaderCompileSupported() const noexcept override;
    bool isDepthStencilResolveSupported() const noexcept override;
    bool isDepthStencilBlitSupported(TextureFormat format) const noexcept override;
    bool isProtectedTexturesSupported() const noexcept override;
    bool isAsynchronousModeEnabled() const noexcept override;
    bool isWorkaroundNeeded(uint32_t workaround) const noexcept override;
    ShaderModel getShaderModel() const noexcept override;
    StreamHandle createStreamNative(void* stream) noexcept override;
    StreamHandle createStreamAcquired() noexcept override;
    void destroyStream(StreamHandle sh) noexcept override;
    void setAcquiredImage(StreamHandle stream, void* image, void* transform) noexcept override;
    void setStreamDimensions(StreamHandle stream, uint32_t width,
                             uint32_t height) noexcept override;
    int64_t getStreamTimestamp(StreamHandle stream) noexcept override;
    void updateStreams() noexcept override;
    void setupExternalImage(void* image) noexcept override;
    void setupExternalImage2(void* image) noexcept override;
    void setFrameRate(SwapChainHandle sch, float frameRate) noexcept override;
    bool queryFrameTimestamps(SwapChainHandle sch, uint64_t frameId,
                              void* outTimestamps) noexcept override;
    bool isCompositorTimingSupported() const noexcept override;
    bool queryCompositorTiming(SwapChainHandle sch, void* outTiming) noexcept override;

    // --- Phase 6: Async + callbacks ---
    VertexBufferHandle createVertexBufferAsync(uint32_t vertexCount,
                                               VertexBufferInfoHandle vbih,
                                               CompletionCallback callback,
                                               void* user) noexcept override;
    IndexBufferHandle createIndexBufferAsync(ElementType elementType, uint32_t indexCount,
                                             BufferUsage usage, CompletionCallback callback,
                                             void* user) noexcept override;
    BufferObjectHandle createBufferObjectAsync(uint32_t byteCount,
                                               BufferObjectBinding bindingType,
                                               BufferUsage usage,
                                               CompletionCallback callback,
                                               void* user) noexcept override;
    TextureHandle createTextureAsync(SamplerType target, uint8_t levels,
                                     TextureFormat format, uint8_t samples, uint32_t width,
                                     uint32_t height, uint32_t depth, TextureUsage usage,
                                     CompletionCallback callback,
                                     void* user) noexcept override;
    TextureHandle createTextureViewSwizzleAsync(TextureHandle texture, TextureSwizzle r,
                                                TextureSwizzle g, TextureSwizzle b,
                                                TextureSwizzle a, CompletionCallback callback,
                                                void* user) noexcept override;
    TextureHandle importTextureAsync(intptr_t id, SamplerType target, uint8_t levels,
                                     TextureFormat format, uint8_t samples, uint32_t width,
                                     uint32_t height, uint32_t depth, TextureUsage usage,
                                     CompletionCallback callback,
                                     void* user) noexcept override;
    void updateIndexBufferAsync(IndexBufferHandle ibh, BufferDescriptor&& data,
                                uint32_t byteOffset, CompletionCallback callback,
                                void* user) noexcept override;
    void updateBufferObjectAsync(BufferObjectHandle boh, BufferDescriptor&& data,
                                 uint32_t byteOffset, CompletionCallback callback,
                                 void* user) noexcept override;
    void update3DImageAsync(TextureHandle th, uint32_t level, uint32_t x, uint32_t y,
                            uint32_t z, uint32_t width, uint32_t height, uint32_t depth,
                            PixelBufferDescriptor&& data, CompletionCallback callback,
                            void* user) noexcept override;
    void setFrameScheduledCallback(SwapChainHandle sch, FrameScheduledCallback callback,
                                   void* user) noexcept override;
    void setFrameCompletedCallback(SwapChainHandle sch, CompletionCallback callback,
                                   void* user) noexcept override;
    void getPlatformSync(SyncHandle sh, SyncCallback callback, void* user) noexcept override;
    void copyToMemoryMappedBuffer(void* mappedPtr, size_t offset,
                                  BufferDescriptor&& data) noexcept override;
    void queueCommandAsync(CompletionCallback callback, void* user) noexcept override;
    bool cancelAsyncJob(uint64_t jobId) noexcept override;
    void purge() noexcept override;
    void scheduleCallback(CompletionCallback callback, void* user) noexcept override;
    void setUnrecoverableError() noexcept override;
    void execute(std::function<void()> const& fn) override;
    void debugCommandBegin(char const* methodName, bool synchronous) noexcept override;
    void debugCommandEnd(char const* methodName, bool synchronous) noexcept override;

    void scissor(Viewport const& viewport) noexcept override;

    // --- Texture ---
    TextureHandle createTexture(SamplerType target, uint8_t levels,
                                TextureFormat format, uint32_t width,
                                uint32_t height, uint32_t depth,
                                TextureUsage usage) noexcept override;
    void destroyTexture(TextureHandle th) noexcept override;
    void setTextureData(TextureHandle th, uint32_t level,
                        uint32_t x, uint32_t y, uint32_t z,
                        uint32_t width, uint32_t height, uint32_t depth,
                        PixelBufferDescriptor&& data) noexcept override;
    // Ported from: itwinjs-core Texture.ts:87-88 (texParameteri TEXTURE_WRAP_S/T).
    void setTextureWrapMode(TextureHandle th, uint32_t wrapS, uint32_t wrapT) noexcept override;
    TextureHandle createTextureView(TextureHandle src, uint8_t baseLevel,
                                    uint8_t levelCount) noexcept override;
    void generateMipmaps(TextureHandle th) noexcept override;
    void bindTexture(uint32_t unit, TextureHandle th) noexcept override;

    // --- Diagnostics ---
    // Aggregate bytes of all live textures owned by this driver (GPU texture
    // memory — MemoryTracker "Textures" consumer equivalent; estimates
    // width×height×depth×levels×bytesPerTexel from each live GLTexture).
    void collectTextureStatistics(RenderMemory::Statistics& stats) const noexcept override;

    // --- Index buffer update ---
    void updateIndexBuffer(IndexBufferHandle ibh, BufferDescriptor&& data,
                           uint32_t byteOffset) noexcept override;

    // --- 3D texture update ---
    void update3DImage(TextureHandle th, uint32_t level,
                       uint32_t x, uint32_t y, uint32_t z,
                       uint32_t width, uint32_t height, uint32_t depth,
                       PixelBufferDescriptor&& data) noexcept override;

    // --- Finish / tick ---
    void finish() noexcept override;
    void tick() noexcept override;

    // --- Program compilation ---
    void compilePrograms() noexcept override;

    // --- Internal ---
    OpenGLContext& getContext() noexcept { return m_context; }
    OpenGLState& getState() noexcept { return m_state; }

    /// Resolve a ProgramHandle to the underlying OpenGLProgram.
    /// Used by ShaderProgram for name-based uniform upload (Phase 0-1 bridge).
    OpenGLProgram* resolveProgram(ProgramHandle ph) noexcept;

    /// Initialize GL state (called by platform after context creation).
    void initializeGL();

    // --- GL-extended resource types ---
    struct GLSwapChain : public HwSwapChain {
        void* platformSwapChain = nullptr;
    };

    struct GLVertexBufferInfo : public HwVertexBufferInfo {
        AttributeArray attributes = {};
    };

    struct GLVertexBuffer : public HwVertexBuffer {
        Handle<HwVertexBufferInfo> vbih;
        std::array<GLuint, MAX_VERTEX_BUFFER_COUNT> buffers = {};
    };

    struct GLIndexBuffer : public HwIndexBuffer {
        GLuint buffer = 0;
    };

    struct GLBufferObject : public HwBufferObject {
        GLuint id = 0;
    };

    struct GLRenderPrimitive : public HwRenderPrimitive {
        OpenGLContext::RenderPrimitive gl;
        Handle<HwVertexBufferInfo> vbih;
        Handle<HwVertexBuffer> vbh;
        Handle<HwIndexBuffer> ibh;
    };

    struct GLTexture : public HwTexture {
        GLuint id = 0;
        GLenum glTarget = GL_TEXTURE_2D;
    };

    struct GLFence : public HwFence {
        GLsync sync = nullptr;
    };

    struct GLRenderTarget : public HwRenderTarget {
        GLuint fbo = 0;
        bool isDefault = false;
        TargetBufferFlags targets = TargetBufferFlags::NONE;
        GLuint colorTexture = 0;
        GLuint depthRenderbuffer = 0;
        static constexpr uint8_t MAX_COLOR_ATTACHMENTS = 8;
        GLuint colorTextures[MAX_COLOR_ATTACHMENTS] = {};
        TextureHandle colorTextureHandles[MAX_COLOR_ATTACHMENTS] = {};
        GLuint depthTexture = 0;
        GLuint stencilTexture = 0;
        uint8_t colorAttachmentCount = 0;
    };

private:
    void setRasterState(RasterState const& rs) noexcept;

    OpenGLPlatform& m_platform;
    OpenGLContext m_context;
    OpenGLState m_state;
    HandleAllocator m_handleAllocator;

    // Current state
    GLRenderTarget* m_currentRenderTarget = nullptr;
    GLRenderPrimitive* m_currentPrimitive = nullptr;
    ProgramHandle m_currentProgram;
    RenderPassParams m_renderPassParams = {};
    bool m_renderPassColorWrite = false;
    bool m_renderPassDepthWrite = false;
    bool m_renderPassStencilWrite = false;
};

}  // namespace rhi
END_DQ_RENDER_NAMESPACE
