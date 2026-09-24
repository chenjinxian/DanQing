// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — OpenGL Driver implementation
// Ported from: filament backend/src/opengl/OpenGLDriver.cpp
//
// Phase 0: minimal implementation for a single triangle.
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "OpenGLDriver.h"
#include "OpenGLProgram.h"
#include "OpenGLAttribute.h"

#include <cassert>
#include <cstring>
#include <functional>
#include <iostream>
#include <type_traits>
#include <vector>

// glTextureView declaration for platforms with GL 4.3+ (not on macOS where GL caps at 4.1).
// Windows 由 GlLoader 运行时装载提供；此处仅 Linux（系统 gl.h/glext.h 无原型时）需要。
#if !defined(__APPLE__) && !defined(_WIN32)
extern "C" void glTextureView(GLuint texture, GLenum target, GLuint origtexture,
                               GLenum internalformat, GLint minlevel, GLsizei numlevels,
                               GLint minlayer, GLsizei numlayers);
#endif

BEGIN_DQ_RENDER_NAMESPACE

namespace rhi {

static std::pair<GLenum, GLenum> toGLTextureFormat(TextureFormat format) noexcept;  // 前向声明（MRT 颜色附件格式映射用）
static GLenum toGLPixelType(TextureFormat format) noexcept;  // 前向声明（MRT 颜色附件 glTexImage2D 的 type 匹配用）
static GLenum toGLBlendFunction(BlendFunction f) noexcept;  // 前向声明（bindPipeline 用）
static GLenum toGLBlendEquation(BlendEquation e) noexcept;  // 前向声明（bindPipeline 用）

// Helper for enum class flag testing
template<typename E>
constexpr bool hasFlag(E value, E flag) noexcept
{
    return (static_cast<std::underlying_type_t<E>>(value) &
            static_cast<std::underlying_type_t<E>>(flag)) != 0;
}

// ---------------------------------------------------------------------------
// Framebuffer discard/invalidate helpers
// ---------------------------------------------------------------------------
// Convert TargetBufferFlags to GL attachment enums.
// Ported from: filament backend/src/opengl/OpenGLDriver.cpp getAttachments()
[[maybe_unused]] static GLsizei getGlAttachments(GLenum* outAttachments, TargetBufferFlags buffers,
                                bool isDefaultFramebuffer) noexcept
{
    GLsizei count = 0;
    if (hasFlag(buffers, TargetBufferFlags::COLOR0))
        outAttachments[count++] = isDefaultFramebuffer ? GL_COLOR : GL_COLOR_ATTACHMENT0;
    if (hasFlag(buffers, TargetBufferFlags::COLOR1))
        outAttachments[count++] = GL_COLOR_ATTACHMENT1;
    if (hasFlag(buffers, TargetBufferFlags::COLOR2))
        outAttachments[count++] = GL_COLOR_ATTACHMENT2;
    if (hasFlag(buffers, TargetBufferFlags::COLOR3))
        outAttachments[count++] = GL_COLOR_ATTACHMENT3;
    if (hasFlag(buffers, TargetBufferFlags::COLOR4))
        outAttachments[count++] = GL_COLOR_ATTACHMENT4;
    if (hasFlag(buffers, TargetBufferFlags::COLOR5))
        outAttachments[count++] = GL_COLOR_ATTACHMENT5;
    if (hasFlag(buffers, TargetBufferFlags::COLOR6))
        outAttachments[count++] = GL_COLOR_ATTACHMENT6;
    if (hasFlag(buffers, TargetBufferFlags::COLOR7))
        outAttachments[count++] = GL_COLOR_ATTACHMENT7;
    if (hasFlag(buffers, TargetBufferFlags::DEPTH))
        outAttachments[count++] = isDefaultFramebuffer ? GL_DEPTH : GL_DEPTH_ATTACHMENT;
    if (hasFlag(buffers, TargetBufferFlags::STENCIL))
        outAttachments[count++] = isDefaultFramebuffer ? GL_STENCIL : GL_STENCIL_ATTACHMENT;
    return count;
}

// Wrapper for glInvalidateFramebuffer — no-op on macOS where GL 4.1 lacks this entry point.
// Ported from: filament backend/src/opengl/OpenGLDriver.cpp (invalidateFramebuffer path)
static void invalidateFramebuffer(GLenum target, GLsizei count,
                                  const GLenum* attachments) noexcept
{
#if !defined(__APPLE__)
    glInvalidateFramebuffer(target, count, attachments);
#else
    (void)target;
    (void)count;
    (void)attachments;
#endif
}

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------
OpenGLDriver::OpenGLDriver(OpenGLPlatform& platform, void* sharedContext,
                           DriverConfig const& config)
    : m_platform(platform)
{
    (void)sharedContext;
    (void)config;
}

OpenGLDriver::~OpenGLDriver()
{
    terminate();
}

// ---------------------------------------------------------------------------
// Frame lifecycle
// ---------------------------------------------------------------------------
void OpenGLDriver::beginFrame(int64_t, int64_t, uint32_t) noexcept
{
    // Nothing to do for Phase 0
}

void OpenGLDriver::endFrame(uint32_t) noexcept
{
    // Nothing to do for Phase 0
}

void OpenGLDriver::flush() noexcept
{
    glFlush();
}

void OpenGLDriver::terminate() noexcept
{
    // TODO: destroy all allocated resources
}

// ---------------------------------------------------------------------------
// Swap chain
// ---------------------------------------------------------------------------
SwapChainHandle OpenGLDriver::createSwapChain(void* nativeWindow, uint64_t) noexcept
{
    auto handle = m_handleAllocator.allocate<GLSwapChain>();
    auto* sc = m_handleAllocator.handle_cast<GLSwapChain, HwSwapChain>(handle);
    if (sc) {
        sc->nativeWindow = nativeWindow;
        sc->platformSwapChain = m_platform.createSwapChain(nativeWindow, 0);
    }
    return handle;
}

SwapChainHandle OpenGLDriver::createSwapChainHeadless(uint32_t width, uint32_t height,
                                                      uint64_t) noexcept
{
    auto handle = m_handleAllocator.allocate<GLSwapChain>();
    auto* sc = m_handleAllocator.handle_cast<GLSwapChain, HwSwapChain>(handle);
    if (sc) {
        sc->platformSwapChain = m_platform.createSwapChain(width, height, 0);
    }
    return handle;
}

void OpenGLDriver::destroySwapChain(SwapChainHandle sch) noexcept
{
    auto* sc = m_handleAllocator.handle_cast<GLSwapChain, HwSwapChain>(sch);
    if (sc) {
        m_platform.destroySwapChain(sc->platformSwapChain);
        m_handleAllocator.deallocate(sch);
    }
}

void OpenGLDriver::makeCurrent(SwapChainHandle schDraw, SwapChainHandle schRead) noexcept
{
    auto* draw = m_handleAllocator.handle_cast<GLSwapChain, HwSwapChain>(schDraw);
    auto* read = m_handleAllocator.handle_cast<GLSwapChain, HwSwapChain>(schRead);
    if (draw && read) {
        m_platform.makeCurrent(draw->platformSwapChain, read->platformSwapChain);
    }
}

void OpenGLDriver::commit(SwapChainHandle sch) noexcept
{
    auto* sc = m_handleAllocator.handle_cast<GLSwapChain, HwSwapChain>(sch);
    if (sc) {
        m_platform.commit(sc->platformSwapChain);
    }
}

// ---------------------------------------------------------------------------
// Program
// ---------------------------------------------------------------------------
ProgramHandle OpenGLDriver::createProgram(Program&& program) noexcept
{
    auto handle = m_handleAllocator.allocate<OpenGLProgram>();
    auto* prog = m_handleAllocator.handle_cast<OpenGLProgram, HwProgram>(handle);
    if (prog) {
        auto const& vertSrc = program.getShaderSource(ShaderStage::VERTEX);
        auto const& fragSrc = program.getShaderSource(ShaderStage::FRAGMENT);

        std::string vertStr(vertSrc.begin(), vertSrc.end());
        std::string fragStr(fragSrc.begin(), fragSrc.end());

        if (!prog->compile(vertStr.c_str(), fragStr.c_str(), program.getName(),
                           program.getAttributeLocations())) {
            m_handleAllocator.deallocate(handle);
            return ProgramHandle{};
        }
    }
    return handle;
}

void OpenGLDriver::destroyProgram(ProgramHandle ph) noexcept
{
    m_handleAllocator.deallocate(ph);
}

// ---------------------------------------------------------------------------
// Vertex / Index buffers
// ---------------------------------------------------------------------------
VertexBufferInfoHandle OpenGLDriver::createVertexBufferInfo(
    uint8_t bufferCount, uint8_t attributeCount,
    AttributeArray const& attributes) noexcept
{
    auto handle = m_handleAllocator.allocate<GLVertexBufferInfo>();
    auto* info = m_handleAllocator.handle_cast<GLVertexBufferInfo, HwVertexBufferInfo>(handle);
    if (info) {
        info->bufferCount = bufferCount;
        info->attributeCount = attributeCount;
        for (uint8_t i = 0; i < attributeCount; ++i) {
            info->attributes[i] = attributes[i];
        }
    }
    return handle;
}

void OpenGLDriver::destroyVertexBufferInfo(VertexBufferInfoHandle vbih) noexcept
{
    m_handleAllocator.deallocate(vbih);
}

VertexBufferHandle OpenGLDriver::createVertexBuffer(uint32_t vertexCount,
                                                    VertexBufferInfoHandle vbih) noexcept
{
    auto handle = m_handleAllocator.allocate<GLVertexBuffer>();
    auto* vb = m_handleAllocator.handle_cast<GLVertexBuffer, HwVertexBuffer>(handle);
    if (vb) {
        vb->vertexCount = vertexCount;
        vb->vbih = vbih;
    }
    return handle;
}

void OpenGLDriver::destroyVertexBuffer(VertexBufferHandle vbh) noexcept
{
    auto* vb = m_handleAllocator.handle_cast<GLVertexBuffer, HwVertexBuffer>(vbh);
    if (vb) {
        for (uint32_t i = 0; i < MAX_VERTEX_BUFFER_COUNT; ++i) {
            if (vb->buffers[i]) {
                glDeleteBuffers(1, &vb->buffers[i]);
            }
        }
        m_handleAllocator.deallocate(vbh);
    }
}

IndexBufferHandle OpenGLDriver::createIndexBuffer(ElementType elementType, uint32_t indexCount,
                                                  BufferUsage) noexcept
{
    auto handle = m_handleAllocator.allocate<GLIndexBuffer>();
    auto* ib = m_handleAllocator.handle_cast<GLIndexBuffer, HwIndexBuffer>(handle);
    if (ib) {
        ib->count = indexCount;
        ib->elementType = elementType;
        glGenBuffers(1, &ib->buffer);
    }
    return handle;
}

void OpenGLDriver::destroyIndexBuffer(IndexBufferHandle ibh) noexcept
{
    auto* ib = m_handleAllocator.handle_cast<GLIndexBuffer, HwIndexBuffer>(ibh);
    if (ib) {
        if (ib->buffer) {
            glDeleteBuffers(1, &ib->buffer);
        }
        m_handleAllocator.deallocate(ibh);
    }
}

// ---------------------------------------------------------------------------
// Buffer objects
// ---------------------------------------------------------------------------
BufferObjectHandle OpenGLDriver::createBufferObject(uint32_t byteCount,
                                                    BufferObjectBinding bindingType,
                                                    BufferUsage) noexcept
{
    auto handle = m_handleAllocator.allocate<GLBufferObject>();
    auto* bo = m_handleAllocator.handle_cast<GLBufferObject, HwBufferObject>(handle);
    if (bo) {
        bo->byteCount = byteCount;
        bo->bindingType = bindingType;
        glGenBuffers(1, &bo->id);
    }
    return handle;
}

void OpenGLDriver::destroyBufferObject(BufferObjectHandle boh) noexcept
{
    auto* bo = m_handleAllocator.handle_cast<GLBufferObject, HwBufferObject>(boh);
    if (bo) {
        if (bo->id) {
            glDeleteBuffers(1, &bo->id);
        }
        m_handleAllocator.deallocate(boh);
    }
}

void OpenGLDriver::updateBufferObject(BufferObjectHandle boh, BufferDescriptor&& data,
                                      uint32_t byteOffset) noexcept
{
    auto* bo = m_handleAllocator.handle_cast<GLBufferObject, HwBufferObject>(boh);
    if (bo && bo->id && data.buffer()) {
        GLenum target = GL_ARRAY_BUFFER;
        if (bo->bindingType == BufferObjectBinding::UNIFORM) {
            target = GL_UNIFORM_BUFFER;
        }
        glBindBuffer(target, bo->id);
        if (byteOffset == 0) {
            glBufferData(target, static_cast<GLsizeiptr>(data.size()),
                         data.buffer(), GL_STATIC_DRAW);
        } else {
            glBufferSubData(target, static_cast<GLintptr>(byteOffset),
                            static_cast<GLsizeiptr>(data.size()), data.buffer());
        }
    }
}

void OpenGLDriver::setVertexBufferObject(VertexBufferHandle vbh, uint32_t index,
                                         BufferObjectHandle bufferObject) noexcept
{
    auto* vb = m_handleAllocator.handle_cast<GLVertexBuffer, HwVertexBuffer>(vbh);
    auto* bo = m_handleAllocator.handle_cast<GLBufferObject, HwBufferObject>(bufferObject);
    if (vb && bo && index < MAX_VERTEX_BUFFER_COUNT) {
        vb->buffers[index] = bo->id;
    }
}

// ---------------------------------------------------------------------------
// Render primitive
// ---------------------------------------------------------------------------
RenderPrimitiveHandle OpenGLDriver::createRenderPrimitive(VertexBufferHandle vbh,
                                                          IndexBufferHandle ibh,
                                                          PrimitiveType pt) noexcept
{
    auto handle = m_handleAllocator.allocate<GLRenderPrimitive>();
    auto* rp = m_handleAllocator.handle_cast<GLRenderPrimitive, HwRenderPrimitive>(handle);
    if (rp) {
        rp->type = pt;
        rp->vbh = vbh;
        rp->ibh = ibh;
    }
    return handle;
}

void OpenGLDriver::destroyRenderPrimitive(RenderPrimitiveHandle rph) noexcept
{
    auto* rp = m_handleAllocator.handle_cast<GLRenderPrimitive, HwRenderPrimitive>(rph);
    if (rp && rp->gl.vao) {
        glDeleteVertexArrays(1, &rp->gl.vao);
    }
    m_handleAllocator.deallocate(rph);
}

void OpenGLDriver::bindRenderPrimitive(RenderPrimitiveHandle rph) noexcept
{
    auto* rp = m_handleAllocator.handle_cast<GLRenderPrimitive, HwRenderPrimitive>(rph);
    if (!rp) return;

    // Create VAO if needed
    if (!rp->gl.vao) {
        glGenVertexArrays(1, &rp->gl.vao);
    }

    glBindVertexArray(rp->gl.vao);

    // Bind vertex buffers
    auto* vb = m_handleAllocator.handle_cast<GLVertexBuffer, HwVertexBuffer>(rp->vbh);
    auto* ib = m_handleAllocator.handle_cast<GLIndexBuffer, HwIndexBuffer>(rp->ibh);

    // TEMP-DIAG (U-flip saga, env gated): read back first 3 vertices (pos +
    // texCoord at offset 40) + attrib3 GL state — ground truth of draw input.
    if (getenv("DANQING_VAO_TRACE") && vb && vb->buffers[0]) {
        static int n = 0;
        GLint bsz = 0;
        glBindBuffer(GL_ARRAY_BUFFER, vb->buffers[0]);
        zoglGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &bsz);
        if (n < 40 && bsz >= 24 * 52) {
            ++n;
            unsigned char vbuf[3 * 52];
            zoglGetBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vbuf), vbuf);
            printf("[VAO] glBufferId=%u size=%d\n", vb->buffers[0], bsz);
            for (int vi = 0; vi < 3; ++vi) {
                float* pos = reinterpret_cast<float*>(vbuf + vi * 52);
                float* uv = reinterpret_cast<float*>(vbuf + vi * 52 + 40);
                printf("[VAO] v%d pos=(%.2f,%.2f,%.2f) uv=(%.3f,%.3f)\n",
                       vi, pos[0], pos[1], pos[2], uv[0], uv[1]);
            }
        }
    }

    if (vb) {
        auto* info = m_handleAllocator.handle_cast<GLVertexBufferInfo, HwVertexBufferInfo>(vb->vbih);
        if (info) {
            // Compute the stride for interleaved vertex data.
            // All attributes in the same buffer share the same stride.
            // Sum up the byte sizes of all attributes that belong to the same buffer.
            uint32_t strides[MAX_VERTEX_BUFFER_COUNT] = {};
            for (uint8_t i = 0; i < info->attributeCount; ++i) {
                auto const& a = info->attributes[i];
                // Size switch extracted to gl::elementSize() (OpenGLAttribute);
                // UBYTE/UBYTE2/UBYTE3 added for Polyline corner buffer attrs.
                uint32_t sz = static_cast<uint32_t>(gl::elementSize(a.type));
                strides[a.buffer] += sz;
            }

            for (uint8_t i = 0; i < info->attributeCount; ++i) {
                auto const& attr = info->attributes[i];
                GLuint buffer = vb->buffers[attr.buffer];
                if (buffer) {
                    glBindBuffer(GL_ARRAY_BUFFER, buffer);
                    glEnableVertexAttribArray(i);

                    // Determine GL type and size.
                    // Type switch extracted to gl::elementFormat() (OpenGLAttribute);
                    // UBYTE/UBYTE2/UBYTE3 added for Polyline corner buffer attrs.
                    // normalized stays GL_FALSE (faithful value for these byte attrs —
                    // the itwinjs WebGL driver does not normalize a_pos/a_param either).
                    int glTypeInt = 0;
                    int glSizeInt = 0;
                    gl::elementFormat(attr.type, glTypeInt, glSizeInt);
                    GLenum glType = static_cast<GLenum>(glTypeInt);
                    GLint glSize = static_cast<GLint>(glSizeInt);
                    GLboolean normalized = GL_FALSE;

                    glVertexAttribPointer(i, glSize, glType, normalized,
                                          static_cast<GLsizei>(strides[attr.buffer]),
                                          reinterpret_cast<void*>(static_cast<uintptr_t>(attr.offset)));
                }
            }
        }
    }

    // Bind index buffer
    if (ib && ib->buffer) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib->buffer);
        rp->gl.elementArray = ib->buffer;
        rp->gl.indicesType = (ib->elementType == ElementType::UINT) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
        rp->gl.indicesShift = (ib->elementType == ElementType::UINT) ? 2 : 1;
    }

    m_currentPrimitive = rp;
}

// ---------------------------------------------------------------------------
// Render target
// ---------------------------------------------------------------------------
RenderTargetHandle OpenGLDriver::createDefaultRenderTarget() noexcept
{
    auto handle = m_handleAllocator.allocate<GLRenderTarget>();
    auto* rt = m_handleAllocator.handle_cast<GLRenderTarget, HwRenderTarget>(handle);
    if (rt) {
        rt->isDefault = true;
        rt->fbo = 0;  // default framebuffer
    }
    return handle;
}

RenderTargetHandle OpenGLDriver::createRenderTarget(TargetBufferFlags flags, uint32_t width,
                                                    uint32_t height, uint8_t /*samples*/,
                                                    uint8_t /*layerCount*/) noexcept
{
    // Ported from: filament backend/src/opengl/OpenGLDriver.cpp (createRenderTargetR)
    // Legacy path: creates textures internally. For MRT with pre-created textures,
    // use createRenderTargetMRT instead.
    auto handle = m_handleAllocator.allocate<GLRenderTarget>();
    auto* rt = m_handleAllocator.handle_cast<GLRenderTarget, HwRenderTarget>(handle);
    if (!rt) return handle;

    rt->width = width;
    rt->height = height;
    rt->targets = flags;
    glGenFramebuffers(1, &rt->fbo);
    m_state.bindFramebuffer(GL_FRAMEBUFFER, rt->fbo);

    // Create color texture attachment (single color buffer, legacy path)
    if (static_cast<uint16_t>(flags) & static_cast<uint16_t>(TargetBufferFlags::COLOR0)) {
        GLuint colorTex = 0;
        glGenTextures(1, &colorTex);
        m_state.bindTexture(0, GL_TEXTURE_2D, colorTex);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               colorTex, 0);
        m_state.bindTexture(0, GL_TEXTURE_2D, 0);
        rt->colorTextures[0] = colorTex;
        rt->colorAttachmentCount = 1;
        // 登记附件纹理句柄（MRT 路径同款）：getRenderTargetColorAttachment 读
        // colorTextureHandles——此前 legacy 路径只填 GLuint 不建句柄，快照类
        // 消费者（OIT 合成的 opaque 快照）拿到空句柄 → 采样无效纹理 → 合成
        // 输出丢背景（应用实测：亮背景勾 Grid 后整帧变暗灰）。
        {
            auto texHandle = m_handleAllocator.allocate<GLTexture>();
            auto* tex = m_handleAllocator.handle_cast<GLTexture, HwTexture>(texHandle);
            if (tex) {
                tex->id = colorTex;
                tex->width = width;
                tex->height = height;
                tex->depth = 1;
                tex->target = SamplerType::SAMPLER_2D;
                tex->levels = 1;
                tex->format = TextureFormat::RGBA8;
                tex->usage = TextureUsage::COLOR_ATTACHMENT;
                tex->glTarget = GL_TEXTURE_2D;
            }
            rt->colorTextureHandles[0] = texHandle;
        }
    }

    // Depth+stencil packed format: use texture for GL_DEPTH_STENCIL_ATTACHMENT
    if (hasFlag(flags, TargetBufferFlags::DEPTH) &&
        hasFlag(flags, TargetBufferFlags::STENCIL)) {
        GLuint dsTex = 0;
        glGenTextures(1, &dsTex);
        m_state.bindTexture(0, GL_TEXTURE_2D, dsTex);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, width, height);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                               GL_TEXTURE_2D, dsTex, 0);
        m_state.bindTexture(0, GL_TEXTURE_2D, 0);
        rt->depthTexture = dsTex;
    } else if (hasFlag(flags, TargetBufferFlags::DEPTH)) {
        // Depth-only: use texture for consistency with MRT path
        GLuint depthTex = 0;
        glGenTextures(1, &depthTex);
        m_state.bindTexture(0, GL_TEXTURE_2D, depthTex);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT24, width, height);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                               depthTex, 0);
        m_state.bindTexture(0, GL_TEXTURE_2D, 0);
        rt->depthTexture = depthTex;
    } else if (hasFlag(flags, TargetBufferFlags::STENCIL)) {
        // Stencil-only
        GLuint stencilTex = 0;
        glGenTextures(1, &stencilTex);
        m_state.bindTexture(0, GL_TEXTURE_2D, stencilTex);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_STENCIL_INDEX8, width, height);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                               stencilTex, 0);
        m_state.bindTexture(0, GL_TEXTURE_2D, 0);
        rt->stencilTexture = stencilTex;
    }

    m_state.bindFramebuffer(GL_FRAMEBUFFER, 0);
    return handle;
}

void OpenGLDriver::destroyRenderTarget(RenderTargetHandle rth) noexcept
{
    auto* rt = m_handleAllocator.handle_cast<GLRenderTarget, HwRenderTarget>(rth);
    if (rt && !rt->isDefault) {
        for (uint8_t i = 0; i < rt->colorAttachmentCount; ++i) {
            // Deallocate TextureHandle (but don't delete GL texture - it's managed by FBO)
            if (rt->colorTextureHandles[i]) {
                m_handleAllocator.deallocate(rt->colorTextureHandles[i]);
            }
            if (rt->colorTextures[i]) {
                glDeleteTextures(1, &rt->colorTextures[i]);
            }
        }
        if (rt->depthTexture) {
            glDeleteTextures(1, &rt->depthTexture);
        }
        if (rt->stencilTexture) {
            glDeleteTextures(1, &rt->stencilTexture);
        }
        if (rt->fbo) {
            glDeleteFramebuffers(1, &rt->fbo);
        }
        m_handleAllocator.deallocate(rth);
    }
    // isDefault（Swapchain default target）的 handle 永不 deallocate——
    // OpenGLSwapchain::getRenderTarget() 返回同一 m_defaultTarget 成员，
    // deallocate 后 handle_cast 失败（beginRenderPass 早退 → endRenderPass
    // 断言 m_currentRenderTarget=null，最大化崩溃根因）。
}

uint8_t OpenGLDriver::getRenderTargetColorAttachmentCount(RenderTargetHandle rth) noexcept
{
    auto* rt = m_handleAllocator.handle_cast<GLRenderTarget, HwRenderTarget>(rth);
    if (!rt) return 0;
    return rt->colorAttachmentCount;
}

TextureHandle OpenGLDriver::getRenderTargetColorAttachment(RenderTargetHandle rth,
                                                           uint8_t index) noexcept
{
    auto* rt = m_handleAllocator.handle_cast<GLRenderTarget, HwRenderTarget>(rth);
    if (!rt || index >= rt->colorAttachmentCount) return TextureHandle();
    return rt->colorTextureHandles[index];
}

// ---------------------------------------------------------------------------
// Render pass
// ---------------------------------------------------------------------------
void OpenGLDriver::beginRenderPass(RenderTargetHandle rth,
                                   RenderPassParams const& params) noexcept
{
    // Ported from: filament backend/src/opengl/OpenGLDriver.cpp beginRenderPass()
    auto* rt = m_handleAllocator.handle_cast<GLRenderTarget, HwRenderTarget>(rth);
    if (!rt) return;

    m_currentRenderTarget = rt;
    m_renderPassParams = params;

    // Bind framebuffer
    m_state.bindFramebuffer(GL_FRAMEBUFFER, rt->fbo);

    // Set up draw buffers for MRT
    if (rt->colorAttachmentCount > 1) {
        GLenum bufs[GLRenderTarget::MAX_COLOR_ATTACHMENTS] = {GL_NONE};
        uint8_t count = rt->colorAttachmentCount;
        if (count > GLRenderTarget::MAX_COLOR_ATTACHMENTS) {
            count = GLRenderTarget::MAX_COLOR_ATTACHMENTS;
        }
        for (uint8_t i = 0; i < count; ++i) {
            if (rt->colorTextures[i]) {
                bufs[i] = GL_COLOR_ATTACHMENT0 + i;
            }
        }
        glDrawBuffers(static_cast<GLsizei>(count), bufs);
    }

    // Determine effective flags - only invalidate/clear attachments that exist on the target
    TargetBufferFlags const rtAttachments = rt->isDefault ? TargetBufferFlags::ALL : rt->targets;
    TargetBufferFlags const clearFlags = params.flags.clear & rtAttachments;
    TargetBufferFlags discardFlags = params.flags.discardStart & rtAttachments;

    // Clear is an implicit discard - don't invalidate attachments that will be cleared
    TargetBufferFlags const discardOnlyFlags = static_cast<TargetBufferFlags>(
        static_cast<uint32_t>(discardFlags) & ~static_cast<uint32_t>(clearFlags));

    // discardStart: invalidate tile memory before drawing (tile-based GPU optimization)
    if (static_cast<uint32_t>(discardOnlyFlags) != 0) {
        GLenum attachments[12];  // max: 8 color + depth + stencil
        GLsizei const count = getGlAttachments(attachments, discardOnlyFlags, rt->isDefault);
        if (count > 0) {
            invalidateFramebuffer(GL_FRAMEBUFFER, count, attachments);
        }
    }

    // Set viewport
    if (params.viewport.width > 0 && params.viewport.height > 0) {
        m_state.viewport(params.viewport.left, params.viewport.bottom,
                        params.viewport.width, params.viewport.height);
    } else if (rt->width > 0 && rt->height > 0) {
        m_state.viewport(0, 0, rt->width, rt->height);
    }

    // Depth range
    glDepthRange(static_cast<GLdouble>(params.depthRange.near),
                 static_cast<GLdouble>(params.depthRange.far));

    // Clear if requested
    GLbitfield clearMask = 0;
    if (hasFlag(clearFlags, TargetBufferFlags::COLOR_ALL)) {
        // Per-attachment clear (for OIT MRT)
        // Ported from: itwinjs-core SceneCompositor.ts per-attachment clear
        if (params.perAttachmentClearCount > 0 && rt->colorAttachmentCount > 1) {
            for (uint8_t i = 0; i < params.perAttachmentClearCount; ++i) {
                glDrawBuffer(GL_COLOR_ATTACHMENT0 + i);
                auto const& cc = params.perAttachmentClearColors[i];
                glClearColor(cc.f[0], cc.f[1], cc.f[2], cc.f[3]);
                glClear(GL_COLOR_BUFFER_BIT);
            }
            // Restore MRT draw buffers
            GLenum bufs[GLRenderTarget::MAX_COLOR_ATTACHMENTS] = {GL_NONE};
            for (uint8_t i = 0; i < rt->colorAttachmentCount; ++i) {
                bufs[i] = GL_COLOR_ATTACHMENT0 + i;
            }
            glDrawBuffers(static_cast<GLsizei>(rt->colorAttachmentCount), bufs);
        } else {
            glClearColor(params.clearColor.f[0], params.clearColor.f[1],
                         params.clearColor.f[2], params.clearColor.f[3]);
            clearMask |= GL_COLOR_BUFFER_BIT;
        }
    }
    if (hasFlag(clearFlags, TargetBufferFlags::DEPTH)) {
        m_state.depthMask(GL_TRUE);
        glClearDepth(params.clearDepth);
        clearMask |= GL_DEPTH_BUFFER_BIT;
    }
    if (hasFlag(clearFlags, TargetBufferFlags::STENCIL)) {
        glClearStencil(static_cast<GLint>(params.clearStencil));
        clearMask |= GL_STENCIL_BUFFER_BIT;
    }
    if (clearMask) {
        glClear(clearMask);
    }

    // Track what was written (used by endRenderPass to decide discard eligibility)
    m_renderPassColorWrite = hasFlag(clearFlags, TargetBufferFlags::COLOR_ALL);
    m_renderPassDepthWrite = hasFlag(clearFlags, TargetBufferFlags::DEPTH);
    m_renderPassStencilWrite = hasFlag(clearFlags, TargetBufferFlags::STENCIL);
}

void OpenGLDriver::endRenderPass() noexcept
{
    // Ported from: filament backend/src/opengl/OpenGLDriver.cpp endRenderPass()
    assert(m_currentRenderTarget);

    GLRenderTarget const* const rt = m_currentRenderTarget;

    // Determine effective discard flags
    TargetBufferFlags const rtAttachments = rt->isDefault ? TargetBufferFlags::ALL : rt->targets;
    TargetBufferFlags discardFlags = m_renderPassParams.flags.discardEnd & rtAttachments;

    // Ignore discard flags for buffers that weren't written during this pass
    if (!m_renderPassColorWrite) {
        discardFlags = static_cast<TargetBufferFlags>(
            static_cast<uint32_t>(discardFlags) &
            ~static_cast<uint32_t>(TargetBufferFlags::COLOR_ALL));
    }
    if (!m_renderPassDepthWrite) {
        discardFlags = static_cast<TargetBufferFlags>(
            static_cast<uint32_t>(discardFlags) &
            ~static_cast<uint32_t>(TargetBufferFlags::DEPTH));
    }
    if (!m_renderPassStencilWrite) {
        discardFlags = static_cast<TargetBufferFlags>(
            static_cast<uint32_t>(discardFlags) &
            ~static_cast<uint32_t>(TargetBufferFlags::STENCIL));
    }

    // discardEnd: invalidate tile memory after drawing (tile-based GPU optimization)
    if (static_cast<uint32_t>(discardFlags) != 0) {
        m_state.bindFramebuffer(GL_FRAMEBUFFER, rt->fbo);
        GLenum attachments[12];
        GLsizei const count = getGlAttachments(attachments, discardFlags, rt->isDefault);
        if (count > 0) {
            invalidateFramebuffer(GL_FRAMEBUFFER, count, attachments);
        }
    }

    m_currentRenderTarget = nullptr;
}

// ---------------------------------------------------------------------------
// Pipeline state
// ---------------------------------------------------------------------------
void OpenGLDriver::bindPipeline(PipelineState const& state) noexcept
{
    // Raster state
    setRasterState(state.rasterState);

    // Per-attachment blend (for OIT MRT)
    // Ported from: itwinjs-core SceneCompositor.ts per-attachment blend
    if (state.perAttachmentBlend && state.perAttachmentBlendCount > 0) {
        m_state.enable(GL_BLEND);
        for (uint8_t i = 0; i < state.perAttachmentBlendCount; ++i) {
            auto const& ab = state.attachmentBlends[i];
            glBlendFuncSeparatei(i,
                toGLBlendFunction(ab.srcRGB),
                toGLBlendFunction(ab.dstRGB),
                toGLBlendFunction(ab.srcAlpha),
                toGLBlendFunction(ab.dstAlpha));
            glBlendEquationSeparatei(i,
                toGLBlendEquation(ab.equationRGB),
                toGLBlendEquation(ab.equationAlpha));
        }
    }

    // Stencil state
    auto const& ss = state.stencilState;
    m_state.stencilFunc(
        static_cast<GLenum>(ss.front.function),
        ss.ref, ss.readMask, true);
    m_state.stencilOp(
        static_cast<GLenum>(ss.front.stencilFail),
        static_cast<GLenum>(ss.front.depthFail),
        static_cast<GLenum>(ss.front.stencilDepthPass), true);

    // Polygon offset
    m_state.polygonOffset(state.polygonOffset.slope, state.polygonOffset.constant);

    // Clip distances
    for (uint8_t i = 0; i < 6; ++i) {
        GLenum cap = static_cast<GLenum>(GL_CLIP_DISTANCE0 + i);
        if (i < state.numClipPlanes) {
            m_state.enable(cap);
        } else {
            m_state.disable(cap);
        }
    }

    // Program
    auto* prog = m_handleAllocator.handle_cast<OpenGLProgram>(state.program);
    if (prog && prog->isValid()) {
        m_state.useProgram(prog->getProgram());
        m_currentProgram = state.program;
    }
}

void OpenGLDriver::bindDescriptorSet(DescriptorSetHandle, descriptor_set_t,
                                     uint32_t const*, uint32_t) noexcept
{
    // Phase 0: not implemented.  Phase 1: full descriptor set binding.
}

// Bind ONLY the program — the WebGL gl.useProgram equivalent used by
// ShaderProgram::use(). Deliberately applies NO raster/blend/stencil state:
// render state remains owned by RenderState::apply (itwinjs semantics).
// bindPipeline's filament-default RasterState (culling=BACK!) must not stomp it.
void OpenGLDriver::useProgram(ProgramHandle ph) noexcept
{
    auto* prog = m_handleAllocator.handle_cast<OpenGLProgram>(ph);
    if (prog && prog->isValid()) {
        m_state.useProgram(prog->getProgram());
        m_currentProgram = ph;
    }
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

// Map PrimitiveType to the GL draw-mode enum. PrimitiveType values do NOT
// match GL constants (GL has LINE_LOOP=2 between LINES=1 and LINE_STRIP=3,
// so PrimitiveType::TRIANGLES=3 would static_cast to GL_LINE_STRIP=3, not
// GL_TRIANGLES=4). This explicit map prevents the wrong draw mode.
static GLenum toGlPrimitiveMode(PrimitiveType type) noexcept
{
    switch (type) {
        case PrimitiveType::POINTS:         return GL_POINTS;
        case PrimitiveType::LINES:          return GL_LINES;
        case PrimitiveType::LINE_STRIP:     return GL_LINE_STRIP;
        case PrimitiveType::TRIANGLES:      return GL_TRIANGLES;
        case PrimitiveType::TRIANGLE_STRIP: return GL_TRIANGLE_STRIP;
    }
    return GL_TRIANGLES;
}

void OpenGLDriver::draw2(uint32_t indexOffset, uint32_t indexCount,
                         uint32_t instanceCount) noexcept
{
    if (!m_currentPrimitive) return;

    GLenum mode = toGlPrimitiveMode(m_currentPrimitive->type);
    GLenum type = m_currentPrimitive->gl.indicesType;
    auto offset = static_cast<intptr_t>(indexOffset) << m_currentPrimitive->gl.indicesShift;

    if (instanceCount > 1) {
        glDrawElementsInstanced(mode, static_cast<GLsizei>(indexCount), type,
                                reinterpret_cast<void*>(offset),
                                static_cast<GLsizei>(instanceCount));
    } else {
        glDrawElements(mode, static_cast<GLsizei>(indexCount), type,
                       reinterpret_cast<void*>(offset));
    }
}

void OpenGLDriver::drawArrays(uint32_t vertexOffset, uint32_t vertexCount,
                              uint32_t instanceCount) noexcept
{
    // Use the current primitive's type if available, otherwise default to GL_TRIANGLES
    GLenum mode = GL_TRIANGLES;
    if (m_currentPrimitive) {
        mode = toGlPrimitiveMode(m_currentPrimitive->type);
    }

    if (instanceCount > 1) {
        glDrawArraysInstanced(mode, static_cast<GLint>(vertexOffset),
                              static_cast<GLsizei>(vertexCount),
                              static_cast<GLsizei>(instanceCount));
    } else {
        glDrawArrays(mode, static_cast<GLint>(vertexOffset),
                     static_cast<GLsizei>(vertexCount));
    }
}

void OpenGLDriver::setVertexAttribDivisor(uint32_t location, uint32_t divisor) noexcept
{
    glVertexAttribDivisor(static_cast<GLuint>(location), static_cast<GLuint>(divisor));
}

void OpenGLDriver::bindInstanceBuffer(BufferObjectHandle boh,
                                      uint32_t location, uint32_t components,
                                      uint32_t stride, uint32_t offset) noexcept
{
    auto* bo = m_handleAllocator.handle_cast<GLBufferObject, HwBufferObject>(boh);
    if (!bo) return;

    glBindBuffer(GL_ARRAY_BUFFER, bo->id);
    glEnableVertexAttribArray(static_cast<GLuint>(location));
    glVertexAttribPointer(
        static_cast<GLuint>(location),
        static_cast<GLint>(components),
        GL_FLOAT, GL_FALSE,
        static_cast<GLsizei>(stride),
        reinterpret_cast<const void*>(static_cast<uintptr_t>(offset)));
}

// ---------------------------------------------------------------------------
// Read-back
// ---------------------------------------------------------------------------
void OpenGLDriver::readPixels(RenderTargetHandle src, uint32_t x, uint32_t y,
                              uint32_t width, uint32_t height,
                              PixelBufferDescriptor&& data, uint32_t colorAttachment) noexcept
{
    if (!data.buffer())
        return;
    // 绑定 src 的 FBO：glReadPixels 读"当前绑定"的帧缓冲——此前实现忽略 src，
    // drawFrame 结束后当前绑定已非渲染目标，回读到黑（渲染独立验证测试实测）。
    auto* rt = m_handleAllocator.handle_cast<GLRenderTarget, HwRenderTarget>(src);
    if (!rt)
        return;
    GLint prevFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
    if (static_cast<GLuint>(prevFbo) != rt->fbo)
        m_state.bindFramebuffer(GL_FRAMEBUFFER, rt->fbo);
    // MRT FBO：显式指定读附件。GL_READ_BUFFER 是 FBO 状态，默认 attachment0，
    // 但若此前被其他路径改动过（drawBuffers 设置不影响 readBuffer），回读会拿到
    // 错误的附件 → 显式固定（Pixel.Selector.Color 语义 = color0；pick 的
    // depthAndOrder 回读传 1）。
    glReadBuffer(rt->isDefault ? GL_BACK
                               : static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + colorAttachment));
    glReadPixels(static_cast<GLint>(x), static_cast<GLint>(y),
                 static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                 data.format(), data.type(), const_cast<void*>(data.buffer()));
    if (static_cast<GLuint>(prevFbo) != rt->fbo)
        m_state.bindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFbo));
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------
bool OpenGLDriver::isTextureFormatSupported(TextureFormat) const noexcept
{
    return true;  // Phase 0: assume all formats supported
}

size_t OpenGLDriver::getMaxTextureSize(SamplerType) const noexcept
{
    return static_cast<size_t>(m_context.gets.maxTextureSize);
}

uint8_t OpenGLDriver::getMaxDrawBuffers() const noexcept
{
    return static_cast<uint8_t>(m_context.gets.maxDrawBuffers);
}

size_t OpenGLDriver::getMaxUniformBufferSize() const noexcept
{
    GLint maxSize = 0;
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &maxSize);
    return static_cast<size_t>(maxSize);
}

size_t OpenGLDriver::getMaxArrayTextureLayers() const noexcept
{
    GLint maxLayers = 0;
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxLayers);
    return static_cast<size_t>(maxLayers);
}

bool OpenGLDriver::isRenderTargetFormatSupported(TextureFormat) const noexcept
{
    return true;  // Phase 4: assume all formats supported
}

bool OpenGLDriver::isFrameBufferFetchSupported() const noexcept
{
    // EXT_shader_framebuffer_fetch / ARM_shader_framebuffer_fetch
    // Phase 4: check via GL extension string at runtime
    return false;  // Conservative default
}

bool OpenGLDriver::isDepthClampSupported() const noexcept
{
    // GL_ARB_depth_clamp or GL_EXT_depth_clamp
    return m_context.isAtLeastGL(3, 2);  // Core in GL 3.2+
}

bool OpenGLDriver::isSRGBSwapChainSupported() const noexcept
{
    return true;  // Assume supported on desktop GL
}

uint32_t OpenGLDriver::getUniformBufferOffsetAlignment() const noexcept
{
    GLint alignment = 0;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &alignment);
    return static_cast<uint32_t>(alignment);
}

// ---------------------------------------------------------------------------
// Fence/Sync (Phase 4)
// ---------------------------------------------------------------------------
FenceHandle OpenGLDriver::createFence() noexcept
{
    FenceHandle fh = m_handleAllocator.allocate<GLFence>();
    GLFence* fence = m_handleAllocator.handle_cast<GLFence>(fh);
    fence->sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    return fh;
}

FenceStatus OpenGLDriver::getFenceStatus(FenceHandle fh) noexcept
{
    auto* fence = m_handleAllocator.handle_cast<GLFence>(fh);
    if (!fence || !fence->sync)
        return FenceStatus::ERROR;

    GLint status = 0;
    glGetSynciv(fence->sync, GL_SYNC_STATUS, sizeof(GLint), nullptr, &status);
    if (status == GL_SIGNALED)
        return FenceStatus::CONDITION_SATISFIED;
    return FenceStatus::TIMEOUT_EXPIRED;
}

void OpenGLDriver::destroyFence(FenceHandle fh) noexcept
{
    auto* fence = m_handleAllocator.handle_cast<GLFence>(fh);
    if (fence && fence->sync) {
        glDeleteSync(fence->sync);
        fence->sync = nullptr;
    }
    m_handleAllocator.deallocate(fh);
}

// ---------------------------------------------------------------------------
// MRT Render target (Phase 4)
// ---------------------------------------------------------------------------
RenderTargetHandle OpenGLDriver::createRenderTargetMRT(
    TargetBufferFlags flags, uint32_t width, uint32_t height,
    uint8_t samples, uint8_t /*layerCount*/,
    uint8_t colorAttachmentCount,
    TextureFormat const* colorFormats,
    TextureFormat /*depthFormat*/) noexcept
{
    // Ported from: filament backend/src/opengl/OpenGLDriver.cpp (createRenderTargetR)
    // MRT path: creates color and depth textures internally, stores them in
    // GLRenderTarget for later binding and cleanup.
    RenderTargetHandle rth = m_handleAllocator.allocate<GLRenderTarget>();
    GLRenderTarget* rt = m_handleAllocator.handle_cast<GLRenderTarget>(rth);
    if (!rth) return rth;

    rt->targets = flags;
    rt->width = width;
    rt->height = height;

    // clamp color attachment count to max
    if (colorAttachmentCount > GLRenderTarget::MAX_COLOR_ATTACHMENTS) {
        colorAttachmentCount = GLRenderTarget::MAX_COLOR_ATTACHMENTS;
    }

    glGenFramebuffers(1, &rt->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);

    // Create color attachments
    GLenum bufs[GLRenderTarget::MAX_COLOR_ATTACHMENTS] = {GL_NONE};
    for (uint8_t i = 0; i < colorAttachmentCount; ++i) {
        if (!hasFlag(flags, static_cast<TargetBufferFlags>(
                               1u << static_cast<uint32_t>(i)))) {
            bufs[i] = GL_NONE;
            continue;
        }
        GLenum glTarget = (samples > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
        GLuint colorTex = 0;
        glGenTextures(1, &colorTex);
        glBindTexture(glTarget, colorTex);
        if (samples > 1) {
            glTexImage2DMultisample(glTarget, samples, GL_RGBA8,
                                    width, height, GL_TRUE);
        } else {
            // 尊重调用方格式（此前硬编码 RGBA8——OIT 请求 RGBA16F 却得到 8bit：
            // accum/revealage 的加权值 (Ci·wzi, ai·wzi ≈ 数十) 被 clamp 到 1，
            // 合成除法后整帧爆白。参考 WebGL HalfFloat 目标同配置不 scale 输出）。
            TextureFormat const fmt = colorFormats ? colorFormats[i] : TextureFormat::RGBA8;
            auto const [internalFormat, pixelFormat] = toGLTextureFormat(fmt);
            // type 必须与格式族匹配：整数附件（R32UI 等）配 UNSIGNED_BYTE 是无效
            // glTexImage2D 组合 → 纹理创建失败 → FBO 不完整 → 绘制静默无效
            // （拾取 saga：pick target 全零的直接根因）。
            GLenum const pixelType = toGLPixelType(fmt);
            glTexImage2D(glTarget, 0, internalFormat, width, height, 0,
                         pixelFormat, pixelType, nullptr);
            glTexParameteri(glTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(glTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(glTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(glTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                               glTarget, colorTex, 0);
        rt->colorTextures[i] = colorTex;

        // Create TextureHandle for attachment access
        auto texHandle = m_handleAllocator.allocate<GLTexture>();
        auto* tex = m_handleAllocator.handle_cast<GLTexture, HwTexture>(texHandle);
        if (tex) {
            tex->id = colorTex;
            tex->width = width;
            tex->height = height;
            tex->depth = 1;
            tex->target = SamplerType::SAMPLER_2D;
            tex->levels = 1;
            tex->format = colorFormats ? colorFormats[i] : TextureFormat::RGBA8;
            tex->usage = TextureUsage::COLOR_ATTACHMENT;
            tex->glTarget = glTarget;
        }
        rt->colorTextureHandles[i] = texHandle;

        bufs[i] = GL_COLOR_ATTACHMENT0 + i;
    }
    rt->colorAttachmentCount = colorAttachmentCount;

    // Set draw buffers
    glDrawBuffers(static_cast<GLsizei>(colorAttachmentCount), bufs);

    // Depth+stencil packed attachment
    bool depthStencilPacked = hasFlag(flags, TargetBufferFlags::DEPTH) &&
                              hasFlag(flags, TargetBufferFlags::STENCIL);

    if (depthStencilPacked) {
        GLuint dsTex = 0;
        glGenTextures(1, &dsTex);
        GLenum dsTarget = (samples > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
        glBindTexture(dsTarget, dsTex);
        if (samples > 1) {
            glTexImage2DMultisample(dsTarget, samples, GL_DEPTH24_STENCIL8,
                                    width, height, GL_TRUE);
        } else {
            glTexImage2D(dsTarget, 0, GL_DEPTH24_STENCIL8, width, height, 0,
                         GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
            glTexParameteri(dsTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(dsTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                               dsTarget, dsTex, 0);
        rt->depthTexture = dsTex;
    } else {
        // Depth-only attachment
        if (hasFlag(flags, TargetBufferFlags::DEPTH)) {
            GLuint depthTex = 0;
            glGenTextures(1, &depthTex);
            GLenum depthTarget = (samples > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
            glBindTexture(depthTarget, depthTex);
            if (samples > 1) {
                glTexImage2DMultisample(depthTarget, samples, GL_DEPTH_COMPONENT24,
                                        width, height, GL_TRUE);
            } else {
                glTexImage2D(depthTarget, 0, GL_DEPTH_COMPONENT24, width, height, 0,
                             GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
                glTexParameteri(depthTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(depthTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            }
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                   depthTarget, depthTex, 0);
            rt->depthTexture = depthTex;
        }

        // Stencil-only attachment
        if (hasFlag(flags, TargetBufferFlags::STENCIL)) {
            GLuint stencilTex = 0;
            glGenTextures(1, &stencilTex);
            GLenum stencilTarget = (samples > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
            glBindTexture(stencilTarget, stencilTex);
            if (samples > 1) {
                glTexImage2DMultisample(stencilTarget, samples, GL_STENCIL_INDEX8,
                                        width, height, GL_TRUE);
            } else {
                glTexImage2D(stencilTarget, 0, GL_STENCIL_INDEX8, width, height, 0,
                             GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, nullptr);
                glTexParameteri(stencilTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(stencilTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            }
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                   stencilTarget, stencilTex, 0);
            rt->stencilTexture = stencilTex;
        }
    }

    // FBO 完整性诊断：不完整的 FBO 让后续绘制/回读全部静默无效（拾取 saga
    // 的教训——R32UI 附件 + UNSIGNED_BYTE type 使纹理创建失败）。
    GLenum const fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
        printf("[RHI] createRenderTargetMRT: INCOMPLETE FBO status=0x%x (%ux%u, colors=%u)\n",
               fboStatus, width, height, colorAttachmentCount);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return rth;
}

// ---------------------------------------------------------------------------
// Blit/Resolve (Phase 4)
// ---------------------------------------------------------------------------
void OpenGLDriver::blit(TargetBufferFlags buffers,
                        RenderTargetHandle dst, Viewport const& dstViewport,
                        RenderTargetHandle src, Viewport const& srcViewport) noexcept
{
    auto* dstRt = m_handleAllocator.handle_cast<GLRenderTarget>(dst);
    auto* srcRt = m_handleAllocator.handle_cast<GLRenderTarget>(src);
    if (!dstRt || !srcRt) return;

    GLbitfield mask = 0;
    if (hasFlag(buffers, TargetBufferFlags::COLOR_ALL))
        mask |= GL_COLOR_BUFFER_BIT;
    if (hasFlag(buffers, TargetBufferFlags::DEPTH))
        mask |= GL_DEPTH_BUFFER_BIT;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, srcRt->fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstRt->fbo);
    glBlitFramebuffer(
        srcViewport.left, srcViewport.bottom,
        srcViewport.left + srcViewport.width, srcViewport.bottom + srcViewport.height,
        dstViewport.left, dstViewport.bottom,
        dstViewport.left + dstViewport.width, dstViewport.bottom + dstViewport.height,
        mask, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // Re-sync the state cache: the raw binds above bypassed OpenGLState, so its
    // cached draw/read FBO no longer matches GL (both are now 0). Without this,
    // the next beginRenderPass(fbo) would see a stale cache hit and skip the bind.
    m_state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    m_state.bindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

void OpenGLDriver::resolve(RenderTargetHandle dst, RenderTargetHandle src) noexcept
{
    // For OpenGL, resolve is the same as blit with COLOR buffer
    Viewport fullViewport;
    fullViewport.left = 0;
    fullViewport.bottom = 0;
    auto* srcRt = m_handleAllocator.handle_cast<GLRenderTarget>(src);
    if (srcRt) {
        fullViewport.width = srcRt->width;
        fullViewport.height = srcRt->height;
    }
    blit(TargetBufferFlags::COLOR_ALL, dst, fullViewport, src, fullViewport);
}

void OpenGLDriver::resetGlState() noexcept
{
    m_state.reset();
}

uint32_t OpenGLDriver::getGlProgramId(ProgramHandle ph) noexcept
{
    auto* prog = m_handleAllocator.handle_cast<OpenGLProgram, HwProgram>(ph);
    return prog ? static_cast<uint32_t>(prog->getProgram()) : 0;
}

void OpenGLDriver::blitToDefaultFramebuffer(RenderTargetHandle src,
                                             Viewport const& /*srcViewport*/) noexcept
{
    auto* srcRt = m_handleAllocator.handle_cast<GLRenderTarget>(src);
    if (!srcRt) return;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, srcRt->fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    // Source: entire FBO
    GLint srcX0 = 0;
    GLint srcY0 = 0;
    GLint srcX1 = static_cast<GLint>(srcRt->width);
    GLint srcY1 = static_cast<GLint>(srcRt->height);

    // Destination: entire default framebuffer (same size as FBO)
    glBlitFramebuffer(
        srcX0, srcY0, srcX1, srcY1,
        srcX0, srcY0, srcX1, srcY1,
        GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // Re-sync the state cache (raw binds above bypassed OpenGLState). Both GL
    // read/draw are now 0; without this the next beginRenderPass skips its bind.
    m_state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    m_state.bindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

// ---------------------------------------------------------------------------
// Read texture (Phase 4)
// ---------------------------------------------------------------------------
void OpenGLDriver::readTexture(TextureHandle th, uint32_t level,
                               PixelBufferDescriptor&& data) noexcept
{
    auto* tex = m_handleAllocator.handle_cast<GLTexture>(th);
    if (!tex) return;

    // Read texture data via FBO attachment (avoids glGetTexImage deprecation on macOS)
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex->glTarget, tex->id, static_cast<GLint>(level));
    // 尊重 descriptor 的 left/top 原点与 format/type（此前硬编码 (0,0)+RGBA/UBYTE：
    // 带 left/top 的读取拿错区域；16F 附件用 UBYTE 写 float 缓冲 → 字节模式被按
    // float 解释成天文垃圾值——OIT 排查时数值探针全失真的根源）。
    glReadPixels(static_cast<GLint>(data.left()), static_cast<GLint>(data.top()),
                 static_cast<GLsizei>(data.width()), static_cast<GLsizei>(data.height()),
                 data.format(), data.type(), const_cast<void*>(data.buffer()));
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    // Re-sync the state cache: the temp FBO binds above bypassed OpenGLState.
    m_state.bindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    m_state.bindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

// ---------------------------------------------------------------------------
// Debug markers (Phase 4)
// ---------------------------------------------------------------------------
void OpenGLDriver::pushGroupMarker(char const* label) noexcept
{
    // KHR_debug support — available in GL 4.3+ or via extension
    (void)label;
    // Phase 4: glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, label);
}

void OpenGLDriver::popGroupMarker() noexcept
{
    // Phase 4: glPopDebugGroup();
}

// ---------------------------------------------------------------------------
// Scissor
// ---------------------------------------------------------------------------
void OpenGLDriver::scissor(Viewport const& viewport) noexcept
{
    m_state.scissor(viewport.left, viewport.bottom, viewport.width, viewport.height);
}

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------
void OpenGLDriver::initializeGL()
{
    m_context.initialize();
    m_state.initialize();
}

OpenGLProgram* OpenGLDriver::resolveProgram(ProgramHandle ph) noexcept
{
    return m_handleAllocator.handle_cast<OpenGLProgram>(ph);
}

// ---------------------------------------------------------------------------
// DepthFunc -> GL comparison function
// SamplerCompareFunc values (LEQUAL=0..NEVER=7) do NOT match GL constants,
// so we need an explicit mapping.
// ---------------------------------------------------------------------------
static GLenum toGLDepthFunc(DepthFunc func) noexcept
{
    switch (func) {
        case DepthFunc::LEQUAL:   return GL_LEQUAL;
        case DepthFunc::GEQUAL:   return GL_GEQUAL;
        case DepthFunc::LESS:     return GL_LESS;
        case DepthFunc::GREATER:  return GL_GREATER;
        case DepthFunc::EQUAL:    return GL_EQUAL;
        case DepthFunc::NOTEQUAL: return GL_NOTEQUAL;
        case DepthFunc::ALWAYS:   return GL_ALWAYS;
        case DepthFunc::NEVER:    return GL_NEVER;
        default:                  return GL_LEQUAL;
    }
}

// ---------------------------------------------------------------------------
// BlendFunction/BlendEquation -> GL enums
// Ordinal values (ZERO=0, ONE=1, SRC_COLOR=2, ... / ADD=0, SUBTRACT=1, ...)
// do NOT match GL constants beyond ZERO/ONE — a raw static_cast turns e.g.
// ONE_MINUS_SRC_ALPHA(7) into invalid enum 7, making glBlendFuncSeparate[i]
// a no-op with GL_INVALID_ENUM (OIT translucent blend silently lost).
// Ported from: filament OpenGLDriver getBlendFunctionMode/getBlendEquationMode.
// ---------------------------------------------------------------------------
static GLenum toGLBlendFunction(BlendFunction f) noexcept
{
    switch (f) {
        case BlendFunction::ZERO:                return GL_ZERO;
        case BlendFunction::ONE:                 return GL_ONE;
        case BlendFunction::SRC_COLOR:           return GL_SRC_COLOR;
        case BlendFunction::ONE_MINUS_SRC_COLOR: return GL_ONE_MINUS_SRC_COLOR;
        case BlendFunction::DST_COLOR:           return GL_DST_COLOR;
        case BlendFunction::ONE_MINUS_DST_COLOR: return GL_ONE_MINUS_DST_COLOR;
        case BlendFunction::SRC_ALPHA:           return GL_SRC_ALPHA;
        case BlendFunction::ONE_MINUS_SRC_ALPHA: return GL_ONE_MINUS_SRC_ALPHA;
        case BlendFunction::DST_ALPHA:           return GL_DST_ALPHA;
        case BlendFunction::ONE_MINUS_DST_ALPHA: return GL_ONE_MINUS_DST_ALPHA;
        case BlendFunction::SRC_ALPHA_SATURATE:  return GL_SRC_ALPHA_SATURATE;
        default:                                 return GL_ONE;
    }
}

static GLenum toGLBlendEquation(BlendEquation e) noexcept
{
    switch (e) {
        case BlendEquation::ADD:              return GL_FUNC_ADD;
        case BlendEquation::SUBTRACT:         return GL_FUNC_SUBTRACT;
        case BlendEquation::REVERSE_SUBTRACT: return GL_FUNC_REVERSE_SUBTRACT;
        case BlendEquation::MIN:              return GL_MIN;
        case BlendEquation::MAX:              return GL_MAX;
        default:                              return GL_FUNC_ADD;
    }
}

void OpenGLDriver::setRasterState(RasterState const& rs) noexcept
{
    // Culling
    if (rs.culling == CullingMode::NONE) {
        m_state.disable(GL_CULL_FACE);
    } else {
        m_state.enable(GL_CULL_FACE);
        m_state.cullFace(static_cast<GLenum>(rs.culling));
    }
    m_state.frontFace(rs.inverseFrontFaces ? GL_CW : GL_CCW);

    // Depth
    if (rs.depthFunc == DepthFunc::NEVER) {
        m_state.disable(GL_DEPTH_TEST);
    } else {
        m_state.enable(GL_DEPTH_TEST);
        m_state.depthFunc(toGLDepthFunc(rs.depthFunc));
    }
    m_state.depthMask(rs.depthWrite ? GL_TRUE : GL_FALSE);

    // Color write
    m_state.colorMask(rs.colorWrite ? GL_TRUE : GL_FALSE);

    // Blend
    if (rs.blendFunctionSrcRGB == BlendFunction::ONE &&
        rs.blendFunctionDstRGB == BlendFunction::ZERO) {
        m_state.disable(GL_BLEND);
    } else {
        m_state.enable(GL_BLEND);
        m_state.blendEquation(toGLBlendEquation(rs.blendEquationRGB),
                              toGLBlendEquation(rs.blendEquationAlpha));
        m_state.blendFunc(toGLBlendFunction(rs.blendFunctionSrcRGB),
                          toGLBlendFunction(rs.blendFunctionDstRGB),
                          toGLBlendFunction(rs.blendFunctionSrcAlpha),
                          toGLBlendFunction(rs.blendFunctionDstAlpha));
    }

    // Alpha to coverage
    if (rs.alphaToCoverage) {
        m_state.enable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    } else {
        m_state.disable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    }
}

// ---------------------------------------------------------------------------
// Texture
// ---------------------------------------------------------------------------
static GLenum toGLTextureTarget(SamplerType target) noexcept
{
    switch (target) {
        case SamplerType::SAMPLER_2D: return GL_TEXTURE_2D;
        case SamplerType::SAMPLER_3D: return GL_TEXTURE_3D;
        case SamplerType::SAMPLER_CUBEMAP: return GL_TEXTURE_CUBE_MAP;
        case SamplerType::SAMPLER_2D_ARRAY: return GL_TEXTURE_2D_ARRAY;
        default: return GL_TEXTURE_2D;
    }
}

// glTexImage2D 的 type 必须与 internalFormat 的数据族匹配（整数附件配
// UNSIGNED_BYTE 无效——附件创建失败 → FBO 不完整）。data=nullptr 时驱动
// 仍做组合校验。
static GLenum toGLPixelType(TextureFormat format) noexcept
{
    switch (format) {
        case TextureFormat::R32UI:
        case TextureFormat::RG8UI:
        case TextureFormat::RGB8UI:
        case TextureFormat::R8UI:
        case TextureFormat::R16UI:
            return GL_UNSIGNED_INT;
        case TextureFormat::R32I:
        case TextureFormat::R8I:
        case TextureFormat::R16I:
        case TextureFormat::RGB8I:
            return GL_INT;
        case TextureFormat::R16F:
        case TextureFormat::RG16F:
        case TextureFormat::RGB16F:
        case TextureFormat::RGBA16F:
        case TextureFormat::R32F:
            return GL_FLOAT;
        default:
            return GL_UNSIGNED_BYTE;
    }
}

static std::pair<GLenum, GLenum> toGLTextureFormat(TextureFormat format) noexcept
{
    switch (format) {
        // 8-bits per element
        case TextureFormat::R8: return {GL_R8, GL_RED};
        // 16-bits per element
        case TextureFormat::R16F: return {GL_R16F, GL_RED};
        case TextureFormat::RG8: return {GL_RG8, GL_RG};
        case TextureFormat::DEPTH16: return {GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT};
        // 24-bits per element
        case TextureFormat::RGB8: return {GL_RGB8, GL_RGB};
        case TextureFormat::DEPTH24: return {GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT};
        // 32-bits per element
        case TextureFormat::R32F: return {GL_R32F, GL_RED};
        case TextureFormat::R32UI: return {GL_R32UI, GL_RED_INTEGER};
        case TextureFormat::R32I: return {GL_R32I, GL_RED_INTEGER};
        case TextureFormat::RG16F: return {GL_RG16F, GL_RG};
        case TextureFormat::RGBA8: return {GL_RGBA8, GL_RGBA};
        case TextureFormat::DEPTH32F: return {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT};
        case TextureFormat::DEPTH24_STENCIL8: return {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL};
        // 48-bits per element
        case TextureFormat::RGB16F: return {GL_RGB16F, GL_RGB};
        // 64-bits per element
        case TextureFormat::RGBA16F: return {GL_RGBA16F, GL_RGBA};
        // BGRA8 — not in Filament, added for Metal/Vulkan compatibility
        case TextureFormat::BGRA8: return {GL_RGBA8, GL_BGRA};
        default: return {GL_RGBA8, GL_RGBA};
    }
}

// Bytes per texel for a TextureFormat (toGLTextureFormat 的分组推导——
// DEPTH24_STENCIL8 = 4 bytes/px packed；其余按注释行 bits-per-element / 8)。
static uint64_t bytesPerTexel(TextureFormat format) noexcept
{
    switch (format) {
        case TextureFormat::R8:        return 1;
        case TextureFormat::R16F:
        case TextureFormat::RG8:
        case TextureFormat::DEPTH16:   return 2;
        case TextureFormat::RGB8:
        case TextureFormat::DEPTH24:   return 3;
        case TextureFormat::R32F:
        case TextureFormat::R32UI:
        case TextureFormat::R32I:
        case TextureFormat::RG16F:
        case TextureFormat::RGBA8:
        case TextureFormat::DEPTH32F:
        case TextureFormat::DEPTH24_STENCIL8:
        case TextureFormat::BGRA8:     return 4;
        case TextureFormat::RGB16F:    return 6;
        case TextureFormat::RGBA16F:   return 8;
        default:                       return 4;
    }
}

void OpenGLDriver::collectTextureStatistics(RenderMemory::Statistics& stats) const noexcept
{
    // MemoryTracker "Textures" consumer walk (MemoryTracker.ts:219) — every
    // live GLTexture owned by this driver's HandleAllocator.
    m_handleAllocator.forEach([&stats](uint32_t, void* ptr) {
        auto const* tex = static_cast<GLTexture const*>(ptr);
        if (!tex || tex->id == 0)
            return;
        uint64_t const bytes = static_cast<uint64_t>(tex->width) * tex->height *
            (tex->depth ? tex->depth : 1) * (tex->levels ? tex->levels : 1) *
            bytesPerTexel(tex->format);
        stats.addTexture(bytes);
    });
}

TextureHandle OpenGLDriver::createTexture(SamplerType target, uint8_t levels,
                                           TextureFormat format, uint32_t width,
                                           uint32_t height, uint32_t depth,
                                           TextureUsage usage) noexcept
{
    auto handle = m_handleAllocator.allocate<GLTexture>();
    auto* tex = m_handleAllocator.handle_cast<GLTexture, HwTexture>(handle);
    if (!tex) return handle;

    tex->width = width;
    tex->height = height;
    tex->depth = depth;
    tex->target = target;
    tex->levels = levels;
    tex->format = format;
    tex->usage = usage;
    tex->glTarget = toGLTextureTarget(target);

    glGenTextures(1, &tex->id);
    m_state.bindTexture(0, tex->glTarget, tex->id);

    // Set texture parameters
    glTexParameteri(tex->glTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(tex->glTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(tex->glTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(tex->glTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (tex->glTarget == GL_TEXTURE_3D || tex->glTarget == GL_TEXTURE_2D_ARRAY) {
        glTexParameteri(tex->glTarget, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }

    // Allocate storage — ported from: filament backend/src/opengl/OpenGLDriver.cpp
    //   textureStorage() dispatches on gl.target
    auto [internalFormat, pixelFormat] = toGLTextureFormat(format);
    switch (tex->glTarget) {
        case GL_TEXTURE_2D:
        case GL_TEXTURE_CUBE_MAP:
            glTexStorage2D(tex->glTarget, static_cast<GLsizei>(levels), internalFormat,
                           static_cast<GLsizei>(width), static_cast<GLsizei>(height));
            break;
        case GL_TEXTURE_3D:
        case GL_TEXTURE_2D_ARRAY:
            glTexStorage3D(tex->glTarget, static_cast<GLsizei>(levels), internalFormat,
                           static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                           static_cast<GLsizei>(depth));
            break;
        default:
            break;
    }

    m_state.bindTexture(0, tex->glTarget, 0);
    return handle;
}

void OpenGLDriver::destroyTexture(TextureHandle th) noexcept
{
    auto* tex = m_handleAllocator.handle_cast<GLTexture, HwTexture>(th);
    if (tex && tex->id) {
        // Invalidate the state-tracker cache BEFORE deleting: glDeleteTextures
        // frees the name, and a subsequent glGenTextures may recycle it. The
        // cache keys on the GL name, so without invalidation the next
        // bindTexture(sameName) is a cache hit → glBindTexture skipped → the new
        // texture's storage/upload lands on the deleted binding → the shader
        // samples an empty texture. This breaks any per-frame-recreated texture
        // (e.g. the Polyline LUT — the ACS-triad invisibility bug).
        m_state.invalidateTexture(tex->id);
        glDeleteTextures(1, &tex->id);
    }
    m_handleAllocator.deallocate(th);
}

void OpenGLDriver::setTextureData(TextureHandle th, uint32_t level,
                                   uint32_t x, uint32_t y, uint32_t z,
                                   uint32_t width, uint32_t height, uint32_t depth,
                                   PixelBufferDescriptor&& data) noexcept
{
    auto* tex = m_handleAllocator.handle_cast<GLTexture, HwTexture>(th);
    if (!tex || !tex->id) return;

    auto [internalFormat, pixelFormat] = toGLTextureFormat(tex->format);
    m_state.bindTexture(0, tex->glTarget, tex->id);
    if (tex->glTarget == GL_TEXTURE_3D || tex->glTarget == GL_TEXTURE_2D_ARRAY) {
        glTexSubImage3D(tex->glTarget, static_cast<GLint>(level),
                        static_cast<GLint>(x), static_cast<GLint>(y), static_cast<GLint>(z),
                        static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                        static_cast<GLsizei>(depth),
                        pixelFormat, GL_UNSIGNED_BYTE, data.buffer());
    } else {
        glTexSubImage2D(tex->glTarget, level, x, y, width, height,
                        pixelFormat, GL_UNSIGNED_BYTE, data.buffer());
    }
    // TEMP-DIAG（贴图 U 翻转 saga）：上传后 glGetTexImage 回读**每个** 256x256
    // 2D 纹理到 build/texdump-N.raw（RGBA），env 门控——验证是否存在第二次
    // （被镜像的）上传。
    if (tex->glTarget == GL_TEXTURE_2D && width == 256 && height == 256 && level == 0) {
        static bool const s_dump = getenv("DANQING_TEX_DUMP") != nullptr;
        if (s_dump) {
            static int s_n = 0;
            std::vector<unsigned char> rb(static_cast<size_t>(width) * height * 4);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, rb.data());
            char path[96];
            snprintf(path, sizeof(path), "build/texdump-%d.raw", s_n);
            FILE* f = fopen(path, "wb");
            if (f) { fwrite(rb.data(), 1, rb.size(), f); fclose(f); }
            printf("[TEXDUMP] texId=%u -> %s (row0 first: %u,%u,%u | row0 last: %u,%u,%u)\n",
                   tex->id, path,
                   rb[0], rb[1], rb[2],
                   rb[static_cast<size_t>(width)*4-4], rb[static_cast<size_t>(width)*4-3], rb[static_cast<size_t>(width)*4-2]);
            ++s_n;
        }
    }
    m_state.bindTexture(0, tex->glTarget, 0);
}

// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/Texture.ts:87-88 —
//   gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, params.wrapMode);
//   gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, params.wrapMode);
// wrapS/wrapT arrive as raw GL values (GL_REPEAT / GL_CLAMP_TO_EDGE).
void OpenGLDriver::setTextureWrapMode(TextureHandle th, uint32_t wrapS, uint32_t wrapT) noexcept
{
    auto* tex = m_handleAllocator.handle_cast<GLTexture, HwTexture>(th);
    if (!tex || !tex->id) return;

    m_state.bindTexture(0, tex->glTarget, tex->id);
    glTexParameteri(tex->glTarget, GL_TEXTURE_WRAP_S, static_cast<GLint>(wrapS));
    glTexParameteri(tex->glTarget, GL_TEXTURE_WRAP_T, static_cast<GLint>(wrapT));
    m_state.bindTexture(0, tex->glTarget, 0);
}

// ---------------------------------------------------------------------------
// Texture view
// ---------------------------------------------------------------------------
TextureHandle OpenGLDriver::createTextureView(TextureHandle src, uint8_t baseLevel,
                                              uint8_t levelCount) noexcept
{
    // Ported from: filament backend/src/opengl/OpenGLDriver.cpp (createTextureView)
    //
    // glTextureView (GL 4.3+ / GL_ARB_texture_view) allows sharing a texture's
    // storage with a different view.  Fallback: return the source handle as-is
    // (no view separation; the caller will sample the full mip range).

    auto* srcTex = m_handleAllocator.handle_cast<GLTexture, HwTexture>(src);
    if (!srcTex || !srcTex->id) return TextureHandle{};

    // On macOS, GL is capped at 4.1 — glTextureView unavailable, suppress unused warnings.
#if defined(__APPLE__)
    (void)baseLevel;
    (void)levelCount;
#endif

    // Try glTextureView if available (GL 4.3+), not on macOS (GL capped at 4.1)
#if !defined(__APPLE__)
    if (m_context.isAtLeastGL(4, 3)) {
        auto handle = m_handleAllocator.allocate<GLTexture>();
        auto* viewTex = m_handleAllocator.handle_cast<GLTexture, HwTexture>(handle);
        if (viewTex) {
            viewTex->width = srcTex->width;
            viewTex->height = srcTex->height;
            viewTex->depth = srcTex->depth;
            viewTex->target = srcTex->target;
            viewTex->levels = levelCount;
            viewTex->format = srcTex->format;
            viewTex->usage = srcTex->usage;
            viewTex->glTarget = srcTex->glTarget;

            auto [internalFormat, pixelFormat] = toGLTextureFormat(srcTex->format);
            (void)pixelFormat;
            glGenTextures(1, &viewTex->id);
            glTextureView(viewTex->id, srcTex->glTarget, srcTex->id,
                          internalFormat, static_cast<GLint>(baseLevel),
                          static_cast<GLsizei>(levelCount), 0, 1);
            return handle;
        }
        return handle;
    }
#endif

    // Fallback: return source handle (caller samples the full mip chain)
    return src;
}

// ---------------------------------------------------------------------------
// Generate mipmaps
// ---------------------------------------------------------------------------
void OpenGLDriver::generateMipmaps(TextureHandle th) noexcept
{
    auto* tex = m_handleAllocator.handle_cast<GLTexture, HwTexture>(th);
    if (!tex || !tex->id) return;

    m_state.bindTexture(0, tex->glTarget, tex->id);
    glGenerateMipmap(tex->glTarget);
    m_state.bindTexture(0, tex->glTarget, 0);
}

// ---------------------------------------------------------------------------
// Bind texture to a texture unit for sampler access
// Ported from: filament Driver::bindTexture()
// ---------------------------------------------------------------------------
void OpenGLDriver::bindTexture(uint32_t unit, TextureHandle th) noexcept
{
    auto* tex = m_handleAllocator.handle_cast<GLTexture, HwTexture>(th);
    if (!tex || !tex->id) {
        // Unbind if invalid handle
        m_state.bindTexture(static_cast<int>(unit), GL_TEXTURE_2D, 0);
        return;
    }
    m_state.bindTexture(static_cast<int>(unit), tex->glTarget, tex->id);
}

// ---------------------------------------------------------------------------
// Index buffer update
// ---------------------------------------------------------------------------
void OpenGLDriver::updateIndexBuffer(IndexBufferHandle ibh, BufferDescriptor&& data,
                                     uint32_t byteOffset) noexcept
{
    auto* ib = m_handleAllocator.handle_cast<GLIndexBuffer, HwIndexBuffer>(ibh);
    if (!ib || !ib->buffer || !data.buffer()) return;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib->buffer);
    if (byteOffset == 0) {
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.size()),
                     data.buffer(), GL_STATIC_DRAW);
    } else {
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLintptr>(byteOffset),
                        static_cast<GLsizeiptr>(data.size()), data.buffer());
    }
}

// ---------------------------------------------------------------------------
// 3D texture update
// ---------------------------------------------------------------------------
void OpenGLDriver::update3DImage(TextureHandle th, uint32_t level,
                                 uint32_t x, uint32_t y, uint32_t z,
                                 uint32_t width, uint32_t height, uint32_t depth,
                                 PixelBufferDescriptor&& data) noexcept
{
    auto* tex = m_handleAllocator.handle_cast<GLTexture, HwTexture>(th);
    if (!tex || !tex->id) return;

    auto [internalFormat, pixelFormat] = toGLTextureFormat(tex->format);
    m_state.bindTexture(0, tex->glTarget, tex->id);
    glTexSubImage3D(tex->glTarget, static_cast<GLint>(level),
                    static_cast<GLint>(x), static_cast<GLint>(y), static_cast<GLint>(z),
                    static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                    static_cast<GLsizei>(depth),
                    pixelFormat, GL_UNSIGNED_BYTE, data.buffer());
    m_state.bindTexture(0, tex->glTarget, 0);
}

// ---------------------------------------------------------------------------
// Finish / tick
// ---------------------------------------------------------------------------
void OpenGLDriver::finish() noexcept
{
    glFinish();
}

void OpenGLDriver::tick() noexcept
{
    // No-op: tick allows the driver to perform deferred cleanup or
    // resource recycling.  No action needed in the OpenGL backend.
}

// ---------------------------------------------------------------------------
// Compile programs
// ---------------------------------------------------------------------------
void OpenGLDriver::compilePrograms() noexcept
{
    // No-op: OpenGL programs compile on first use (lazy compilation).
    // Vulkan/Metal backends may need ahead-of-time compilation.
}

// ---------------------------------------------------------------------------
// Phase 5: Complete Filament RHI — Frame lifecycle (extended)
// ---------------------------------------------------------------------------
void OpenGLDriver::resetState() noexcept
{
    m_state.initialize();
}

void OpenGLDriver::setPresentationTime(int64_t) noexcept
{
    // No-op on desktop GL; used by mobile platforms for vsync alignment.
}

// ---------------------------------------------------------------------------
// Phase 5: Fence/Sync (extended)
// ---------------------------------------------------------------------------
FenceStatus OpenGLDriver::fenceWait(FenceHandle fh, uint64_t timeoutNs) noexcept
{
    auto* fence = m_handleAllocator.handle_cast<GLFence>(fh);
    if (!fence || !fence->sync)
        return FenceStatus::ERROR;

    GLenum result = glClientWaitSync(fence->sync, GL_SYNC_FLUSH_COMMANDS_BIT, timeoutNs);
    if (result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED)
        return FenceStatus::CONDITION_SATISFIED;
    if (result == GL_TIMEOUT_EXPIRED)
        return FenceStatus::TIMEOUT_EXPIRED;
    return FenceStatus::ERROR;
}

void OpenGLDriver::fenceCancel(FenceHandle fh) noexcept
{
    destroyFence(fh);
}

// ---------------------------------------------------------------------------
// Phase 5: Sync primitives
// ---------------------------------------------------------------------------
SyncHandle OpenGLDriver::createSync() noexcept
{
    return m_handleAllocator.allocate<HwSync>();
}

void OpenGLDriver::destroySync(SyncHandle sh) noexcept
{
    m_handleAllocator.deallocate(sh);
}

// ---------------------------------------------------------------------------
// Phase 5: Timer queries
// ---------------------------------------------------------------------------
TimerQueryHandle OpenGLDriver::createTimerQuery() noexcept
{
    return m_handleAllocator.allocate<HwTimerQuery>();
}

void OpenGLDriver::destroyTimerQuery(TimerQueryHandle tqh) noexcept
{
    m_handleAllocator.deallocate(tqh);
}

void OpenGLDriver::beginTimerQuery(TimerQueryHandle) noexcept
{
    // Timer queries require GL_EXT_disjoint_timer_query or GL 3.3+
    // Phase 5: placeholder — full implementation in GLTimer
}

void OpenGLDriver::endTimerQuery(TimerQueryHandle) noexcept
{
    // Phase 5: placeholder
}

TimerQueryResult OpenGLDriver::getTimerQueryValue(TimerQueryHandle, uint64_t*) noexcept
{
    return TimerQueryResult::NOT_READY;
}

// ---------------------------------------------------------------------------
// Phase 5: Descriptor sets
// ---------------------------------------------------------------------------
DescriptorSetLayoutHandle OpenGLDriver::createDescriptorSetLayout(
    DescriptorSetLayout&&) noexcept
{
    return m_handleAllocator.allocate<HwDescriptorSetLayout>();
}

void OpenGLDriver::destroyDescriptorSetLayout(DescriptorSetLayoutHandle dslh) noexcept
{
    m_handleAllocator.deallocate(dslh);
}

DescriptorSetHandle OpenGLDriver::createDescriptorSet(DescriptorSetLayoutHandle) noexcept
{
    return m_handleAllocator.allocate<HwDescriptorSet>();
}

void OpenGLDriver::destroyDescriptorSet(DescriptorSetHandle dsh) noexcept
{
    m_handleAllocator.deallocate(dsh);
}

void OpenGLDriver::updateDescriptorSetBuffer(DescriptorSetHandle, descriptor_binding_t,
                                              BufferObjectHandle, uint32_t,
                                              uint32_t) noexcept
{
    // UBO binding is handled by the rendering layer via glUniformBlockBinding.
    // Descriptor sets are a Vulkan/Metal concept; GL uses direct binding.
}

void OpenGLDriver::updateDescriptorSetTexture(DescriptorSetHandle, descriptor_binding_t,
                                               TextureHandle, SamplerParams const&) noexcept
{
    // Texture binding is handled by the rendering layer via glActiveTexture/glBindTexture.
}

// ---------------------------------------------------------------------------
// Phase 5: Texture variants
// ---------------------------------------------------------------------------
TextureHandle OpenGLDriver::createTextureViewSwizzle(TextureHandle src, TextureSwizzle,
                                                      TextureSwizzle, TextureSwizzle,
                                                      TextureSwizzle) noexcept
{
    // Texture swizzle requires GL 3.3+ or ARB_texture_swizzle.
    // For now, return the source handle (identity swizzle).
    return src;
}

TextureHandle OpenGLDriver::createTextureExternalImage(SamplerType, TextureFormat,
                                                        uint32_t, uint32_t,
                                                        TextureUsage, void*) noexcept
{
    // External textures (e.g., camera frames) require platform-specific handling.
    return TextureHandle();
}

TextureHandle OpenGLDriver::createTextureExternalImage2(SamplerType, TextureFormat,
                                                         uint32_t, uint32_t,
                                                         TextureUsage, void*) noexcept
{
    return TextureHandle();
}

TextureHandle OpenGLDriver::createTextureExternalImagePlane(TextureFormat, uint32_t,
                                                             uint32_t, TextureUsage,
                                                             void*, uint32_t) noexcept
{
    return TextureHandle();
}

TextureHandle OpenGLDriver::importTexture(intptr_t, SamplerType, uint8_t, TextureFormat,
                                           uint8_t, uint32_t, uint32_t, uint32_t,
                                           TextureUsage) noexcept
{
    // Importing external GL textures by ID.
    return TextureHandle();
}

void OpenGLDriver::setExternalStream(TextureHandle, StreamHandle) noexcept
{
    // External stream binding for video/camera textures.
}

// ---------------------------------------------------------------------------
// Phase 5: Update operations (extended)
// ---------------------------------------------------------------------------
void OpenGLDriver::updateBufferObjectUnsynchronized(BufferObjectHandle boh,
                                                     BufferDescriptor&& data,
                                                     uint32_t byteOffset) noexcept
{
    auto* bo = m_handleAllocator.handle_cast<GLBufferObject>(boh);
    if (!bo) return;

    glBindBuffer(GL_ARRAY_BUFFER, bo->id);
    if (byteOffset == 0 && data.size() > 0) {
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.size()),
                     data.buffer(), GL_DYNAMIC_DRAW);
    } else if (data.size() > 0) {
        glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(byteOffset),
                        static_cast<GLsizeiptr>(data.size()), data.buffer());
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void OpenGLDriver::resetBufferObject(BufferObjectHandle boh) noexcept
{
    auto* bo = m_handleAllocator.handle_cast<GLBufferObject>(boh);
    if (!bo) return;

    glBindBuffer(GL_ARRAY_BUFFER, bo->id);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ---------------------------------------------------------------------------
// Phase 5: Render operations (extended)
// ---------------------------------------------------------------------------
void OpenGLDriver::nextSubpass() noexcept
{
    // Subpasses are a Vulkan concept. In GL, each subpass is a separate render pass.
}

void OpenGLDriver::draw(PipelineState const& state, RenderPrimitiveHandle rph,
                         uint32_t indexOffset, uint32_t indexCount,
                         uint32_t instanceCount) noexcept
{
    bindPipeline(state);
    bindRenderPrimitive(rph);
    draw2(indexOffset, indexCount, instanceCount);
}

void OpenGLDriver::dispatchCompute(ProgramHandle, uint32_t, uint32_t, uint32_t) noexcept
{
    // Compute shaders require GL 4.3+ or ES 3.1+.
    // Phase 5: placeholder for future compute support.
}

// ---------------------------------------------------------------------------
// Phase 5: Read-back (extended)
// ---------------------------------------------------------------------------
void OpenGLDriver::readBufferSubData(BufferObjectHandle src, uint32_t offset, uint32_t size,
                                      BufferDescriptor&& data) noexcept
{
    auto* bo = m_handleAllocator.handle_cast<GLBufferObject>(src);
    if (!bo) return;

    glBindBuffer(GL_ARRAY_BUFFER, bo->id);
    glGetBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(offset),
                       static_cast<GLsizeiptr>(size),
                       const_cast<void*>(data.buffer()));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ---------------------------------------------------------------------------
// Phase 5: Memory-mapped buffers
// ---------------------------------------------------------------------------
void* OpenGLDriver::mapBuffer(BufferObjectHandle boh, size_t offset, size_t size,
                               uint32_t accessFlags) noexcept
{
    auto* bo = m_handleAllocator.handle_cast<GLBufferObject>(boh);
    if (!bo) return nullptr;

    GLbitfield access = 0;
    if (accessFlags & static_cast<uint32_t>(MapBufferAccess::READ))
        access |= GL_MAP_READ_BIT;
    if (accessFlags & static_cast<uint32_t>(MapBufferAccess::WRITE))
        access |= GL_MAP_WRITE_BIT;

    glBindBuffer(GL_ARRAY_BUFFER, bo->id);
    void* ptr = glMapBufferRange(GL_ARRAY_BUFFER, static_cast<GLintptr>(offset),
                                  static_cast<GLsizeiptr>(size), access);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return ptr;
}

void OpenGLDriver::unmapBuffer(BufferObjectHandle boh) noexcept
{
    auto* bo = m_handleAllocator.handle_cast<GLBufferObject>(boh);
    if (!bo) return;

    glBindBuffer(GL_ARRAY_BUFFER, bo->id);
    glUnmapBuffer(GL_ARRAY_BUFFER);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ---------------------------------------------------------------------------
// Phase 5: Push constants
// ---------------------------------------------------------------------------
void OpenGLDriver::setPushConstant(ShaderStage, uint8_t, int32_t) noexcept
{
    // Push constants are a Vulkan concept. In GL, uniforms serve this role.
}

// ---------------------------------------------------------------------------
// Phase 5: Debug / capture (extended)
// ---------------------------------------------------------------------------
void OpenGLDriver::insertEventMarker(char const*) noexcept
{
    // GL_KHR_debug: glInsertEventMarker(GL_DEBUG_SOURCE_APPLICATION, label);
}

void OpenGLDriver::startCapture() noexcept
{
    // Platform-specific capture (e.g., RenderDoc, Xcode GPU Capture).
}

void OpenGLDriver::stopCapture() noexcept
{
    // Platform-specific capture.
}

// ---------------------------------------------------------------------------
// Phase 5: Capability queries (extended)
// ---------------------------------------------------------------------------
bool OpenGLDriver::isTextureSwizzleSupported() const noexcept
{
    return m_context.isAtLeastGL(3, 3);  // Core in GL 3.3
}

bool OpenGLDriver::isTextureFormatMipmappable(TextureFormat) const noexcept
{
    return true;  // GL supports mipmaps for most formats
}

bool OpenGLDriver::isTextureFormatFilterable(TextureFormat) const noexcept
{
    return true;  // GL supports filtering for most formats
}

bool OpenGLDriver::isFrameBufferFetchMultiSampleSupported() const noexcept
{
    return false;  // Requires specific extensions
}

bool OpenGLDriver::isFrameTimeSupported() const noexcept
{
    return m_context.ext.EXTDisjointTimerQuery;
}

bool OpenGLDriver::isAutoDepthResolveSupported() const noexcept
{
    return m_context.isAtLeastGL(4, 3);  // GL 4.3+ or ARB_internalformat_query2
}

bool OpenGLDriver::isMSAASwapChainSupported(uint32_t) const noexcept
{
    return true;  // Desktop GL supports MSAA swap chains
}

bool OpenGLDriver::isProtectedContentSupported() const noexcept
{
    return false;  // Not supported in standard OpenGL
}

bool OpenGLDriver::isStereoSupported() const noexcept
{
    return false;  // Stereo rendering requires specific setup
}

bool OpenGLDriver::isParallelShaderCompileSupported() const noexcept
{
    return m_context.ext.KHRParallelShaderCompile;
}

bool OpenGLDriver::isDepthStencilResolveSupported() const noexcept
{
    return m_context.isAtLeastGL(4, 3);
}

bool OpenGLDriver::isDepthStencilBlitSupported(TextureFormat) const noexcept
{
    return m_context.isAtLeastGL(3, 0);
}

bool OpenGLDriver::isProtectedTexturesSupported() const noexcept
{
    return false;
}

bool OpenGLDriver::isAsynchronousModeEnabled() const noexcept
{
    return false;  // GL is synchronous by default
}

bool OpenGLDriver::isWorkaroundNeeded(uint32_t) const noexcept
{
    return false;  // No workarounds needed by default
}

ShaderModel OpenGLDriver::getShaderModel() const noexcept
{
    if (m_context.isAtLeastGL(4, 6)) return ShaderModel::GL_CORE_46;
    if (m_context.isAtLeastGL(4, 5)) return ShaderModel::GL_CORE_45;
    if (m_context.isAtLeastGL(4, 1)) return ShaderModel::GL_CORE_41;
    if (m_context.isAtLeastGL(3, 3)) return ShaderModel::GL_CORE_33;
    return ShaderModel::UNKNOWN;
}

// ---------------------------------------------------------------------------
// Phase 5: Streams
// ---------------------------------------------------------------------------
StreamHandle OpenGLDriver::createStreamNative(void*) noexcept
{
    return m_handleAllocator.allocate<HwStream>();
}

StreamHandle OpenGLDriver::createStreamAcquired() noexcept
{
    return m_handleAllocator.allocate<HwStream>();
}

void OpenGLDriver::destroyStream(StreamHandle sh) noexcept
{
    m_handleAllocator.deallocate(sh);
}

void OpenGLDriver::setAcquiredImage(StreamHandle, void*, void*) noexcept
{
    // External image acquisition (e.g., camera frames).
}

void OpenGLDriver::setStreamDimensions(StreamHandle, uint32_t, uint32_t) noexcept
{
    // Stream dimension update.
}

int64_t OpenGLDriver::getStreamTimestamp(StreamHandle) noexcept
{
    return 0;
}

void OpenGLDriver::updateStreams() noexcept
{
    // Update all external streams.
}

// ---------------------------------------------------------------------------
// Phase 5: External image setup
// ---------------------------------------------------------------------------
void OpenGLDriver::setupExternalImage(void*) noexcept
{
    // Platform-specific external image setup.
}

void OpenGLDriver::setupExternalImage2(void*) noexcept
{
    // Platform-specific external image setup (v2).
}

// ---------------------------------------------------------------------------
// Phase 5: Swap chain (extended)
// ---------------------------------------------------------------------------
void OpenGLDriver::setFrameRate(SwapChainHandle, float) noexcept
{
    // Frame rate control (platform-specific).
}

// ---------------------------------------------------------------------------
// Phase 5: Frame timing
// ---------------------------------------------------------------------------
bool OpenGLDriver::queryFrameTimestamps(SwapChainHandle, uint64_t, void*) noexcept
{
    return false;  // Requires GL_EXT_disjoint_timer_query
}

bool OpenGLDriver::isCompositorTimingSupported() const noexcept
{
    return false;  // Platform-specific
}

bool OpenGLDriver::queryCompositorTiming(SwapChainHandle, void*) noexcept
{
    return false;  // Platform-specific
}

// ---------------------------------------------------------------------------
// Phase 6: Async resource creation
// ---------------------------------------------------------------------------
VertexBufferHandle OpenGLDriver::createVertexBufferAsync(
    uint32_t vertexCount, VertexBufferInfoHandle vbih,
    CompletionCallback callback, void* user) noexcept
{
    // GL is synchronous — create immediately and fire callback.
    VertexBufferHandle h = createVertexBuffer(vertexCount, vbih);
    if (callback) callback(user);
    return h;
}

IndexBufferHandle OpenGLDriver::createIndexBufferAsync(
    ElementType elementType, uint32_t indexCount, BufferUsage usage,
    CompletionCallback callback, void* user) noexcept
{
    IndexBufferHandle h = createIndexBuffer(elementType, indexCount, usage);
    if (callback) callback(user);
    return h;
}

BufferObjectHandle OpenGLDriver::createBufferObjectAsync(
    uint32_t byteCount, BufferObjectBinding bindingType, BufferUsage usage,
    CompletionCallback callback, void* user) noexcept
{
    BufferObjectHandle h = createBufferObject(byteCount, bindingType, usage);
    if (callback) callback(user);
    return h;
}

TextureHandle OpenGLDriver::createTextureAsync(
    SamplerType target, uint8_t levels, TextureFormat format, uint8_t samples,
    uint32_t width, uint32_t height, uint32_t depth, TextureUsage usage,
    CompletionCallback callback, void* user) noexcept
{
    (void)samples;  // GL doesn't use MSAA samples at texture creation time
    TextureHandle h = createTexture(target, levels, format, width, height, depth, usage);
    if (callback) callback(user);
    return h;
}

TextureHandle OpenGLDriver::createTextureViewSwizzleAsync(
    TextureHandle texture, TextureSwizzle r, TextureSwizzle g,
    TextureSwizzle b, TextureSwizzle a,
    CompletionCallback callback, void* user) noexcept
{
    TextureHandle h = createTextureViewSwizzle(texture, r, g, b, a);
    if (callback) callback(user);
    return h;
}

TextureHandle OpenGLDriver::importTextureAsync(
    intptr_t id, SamplerType target, uint8_t levels, TextureFormat format,
    uint8_t samples, uint32_t width, uint32_t height, uint32_t depth,
    TextureUsage usage, CompletionCallback callback, void* user) noexcept
{
    TextureHandle h = importTexture(id, target, levels, format, samples,
                                    width, height, depth, usage);
    if (callback) callback(user);
    return h;
}

// ---------------------------------------------------------------------------
// Phase 6: Async update operations
// ---------------------------------------------------------------------------
void OpenGLDriver::updateIndexBufferAsync(IndexBufferHandle ibh, BufferDescriptor&& data,
                                           uint32_t byteOffset,
                                           CompletionCallback callback,
                                           void* user) noexcept
{
    updateIndexBuffer(ibh, std::move(data), byteOffset);
    if (callback) callback(user);
}

void OpenGLDriver::updateBufferObjectAsync(BufferObjectHandle boh, BufferDescriptor&& data,
                                            uint32_t byteOffset,
                                            CompletionCallback callback,
                                            void* user) noexcept
{
    updateBufferObject(boh, std::move(data), byteOffset);
    if (callback) callback(user);
}

void OpenGLDriver::update3DImageAsync(TextureHandle th, uint32_t level,
                                       uint32_t x, uint32_t y, uint32_t z,
                                       uint32_t width, uint32_t height, uint32_t depth,
                                       PixelBufferDescriptor&& data,
                                       CompletionCallback callback,
                                       void* user) noexcept
{
    update3DImage(th, level, x, y, z, width, height, depth, std::move(data));
    if (callback) callback(user);
}

// ---------------------------------------------------------------------------
// Phase 6: Frame callbacks
// ---------------------------------------------------------------------------
void OpenGLDriver::setFrameScheduledCallback(SwapChainHandle, FrameScheduledCallback,
                                              void*) noexcept
{
    // Frame scheduling is handled by the platform layer (vsync, frame pacing).
}

void OpenGLDriver::setFrameCompletedCallback(SwapChainHandle, CompletionCallback,
                                              void*) noexcept
{
    // Frame completion notifications for multi-threaded rendering.
}

// ---------------------------------------------------------------------------
// Phase 6: Sync queries (extended)
// ---------------------------------------------------------------------------
void OpenGLDriver::getPlatformSync(SyncHandle, SyncCallback callback, void* user) noexcept
{
    // Platform sync objects (e.g., EGLSync, GLFenceSync) for cross-API sharing.
    if (callback) callback(user, 0);
}

// ---------------------------------------------------------------------------
// Phase 6: Memory-mapped buffer (extended)
// ---------------------------------------------------------------------------
void OpenGLDriver::copyToMemoryMappedBuffer(void* mappedPtr, size_t offset,
                                             BufferDescriptor&& data) noexcept
{
    if (mappedPtr && data.buffer() && data.size() > 0) {
        memcpy(static_cast<uint8_t*>(mappedPtr) + offset, data.buffer(), data.size());
    }
}

// ---------------------------------------------------------------------------
// Phase 6: Async command execution
// ---------------------------------------------------------------------------
void OpenGLDriver::queueCommandAsync(CompletionCallback callback, void* user) noexcept
{
    // In GL, commands execute on the calling thread. Fire callback immediately.
    if (callback) callback(user);
}

bool OpenGLDriver::cancelAsyncJob(uint64_t) noexcept
{
    return false;  // GL has no async jobs to cancel
}

// ---------------------------------------------------------------------------
// Phase 6: Base class virtuals
// ---------------------------------------------------------------------------
void OpenGLDriver::purge() noexcept
{
    // Release cached resources (shader programs, textures, etc.).
    // Phase 6: placeholder for resource cache management.
}

void OpenGLDriver::scheduleCallback(CompletionCallback callback, void* user) noexcept
{
    // Schedule a callback to be executed on the render thread.
    // In single-threaded GL, execute immediately.
    if (callback) callback(user);
}

void OpenGLDriver::setUnrecoverableError() noexcept
{
    // Mark the driver as having encountered an unrecoverable error.
    // All subsequent operations will be no-ops.
}

void OpenGLDriver::execute(std::function<void()> const& fn)
{
    fn();
}

void OpenGLDriver::debugCommandBegin(char const*, bool) noexcept
{
    // Debug command tracing begin.
}

void OpenGLDriver::debugCommandEnd(char const*, bool) noexcept
{
    // Debug command tracing end.
}

}  // namespace rhi
END_DQ_RENDER_NAMESPACE
