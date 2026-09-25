// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — Windows GL 运行时符号装载实现（bluegl 机制）
// Authored: 参照 filament third_party/bluegl 的机制实现；WGL bootstrap 为
//           wglGetProcAddress 文档标准流程（与 PlatformWgl 同源）。
#include "GlLoader.h"

#if defined(_WIN32)

#include <windows.h>
#undef near
#undef far

// 指针定义在全局作用域（与 GlLoader.h 的 extern 声明对应；不能放匿名命名空间，
// 否则 init() 内引用产生二义）
PFNGLACTIVETEXTUREPROC dqglActiveTexture = nullptr;
PFNGLATTACHSHADERPROC dqglAttachShader = nullptr;
PFNGLBEGINQUERYPROC dqglBeginQuery = nullptr;
PFNGLBINDATTRIBLOCATIONPROC dqglBindAttribLocation = nullptr;
PFNGLBINDBUFFERPROC dqglBindBuffer = nullptr;
PFNGLBINDFRAMEBUFFERPROC dqglBindFramebuffer = nullptr;
PFNGLBINDRENDERBUFFERPROC dqglBindRenderbuffer = nullptr;
PFNGLBINDTEXTUREPROC dqglBindTexture = nullptr;
PFNGLBINDVERTEXARRAYPROC dqglBindVertexArray = nullptr;
PFNGLBLENDCOLORPROC dqglBlendColor = nullptr;
PFNGLBLENDEQUATIONSEPARATEPROC dqglBlendEquationSeparate = nullptr;
PFNGLBLENDEQUATIONSEPARATEIPROC dqglBlendEquationSeparatei = nullptr;
PFNGLBLENDFUNCPROC dqglBlendFunc = nullptr;
PFNGLBLENDFUNCSEPARATEPROC dqglBlendFuncSeparate = nullptr;
PFNGLBLENDFUNCSEPARATEIPROC dqglBlendFuncSeparatei = nullptr;
PFNGLBLITFRAMEBUFFERPROC dqglBlitFramebuffer = nullptr;
PFNGLBUFFERDATAPROC dqglBufferData = nullptr;
PFNGLBUFFERSUBDATAPROC dqglBufferSubData = nullptr;
PFNGLCHECKFRAMEBUFFERSTATUSPROC dqglCheckFramebufferStatus = nullptr;
PFNGLCLEARPROC dqglClear = nullptr;
PFNGLCLEARCOLORPROC dqglClearColor = nullptr;
PFNGLCLEARDEPTHPROC dqglClearDepth = nullptr;
PFNGLCLEARSTENCILPROC dqglClearStencil = nullptr;
PFNGLCLIENTWAITSYNCPROC dqglClientWaitSync = nullptr;
PFNGLCOLORMASKPROC dqglColorMask = nullptr;
PFNGLCOMPILESHADERPROC dqglCompileShader = nullptr;
PFNGLCREATEPROGRAMPROC dqglCreateProgram = nullptr;
PFNGLCREATESHADERPROC dqglCreateShader = nullptr;
PFNGLCULLFACEPROC dqglCullFace = nullptr;
PFNGLDELETEBUFFERSPROC dqglDeleteBuffers = nullptr;
PFNGLDELETEFRAMEBUFFERSPROC dqglDeleteFramebuffers = nullptr;
PFNGLDELETEPROGRAMPROC dqglDeleteProgram = nullptr;
PFNGLDELETEQUERIESPROC dqglDeleteQueries = nullptr;
PFNGLDELETERENDERBUFFERSPROC dqglDeleteRenderbuffers = nullptr;
PFNGLDELETESHADERPROC dqglDeleteShader = nullptr;
PFNGLDELETESYNCPROC dqglDeleteSync = nullptr;
PFNGLDELETETEXTURESPROC dqglDeleteTextures = nullptr;
PFNGLDELETEVERTEXARRAYSPROC dqglDeleteVertexArrays = nullptr;
PFNGLDEPTHFUNCPROC dqglDepthFunc = nullptr;
PFNGLDEPTHMASKPROC dqglDepthMask = nullptr;
PFNGLDEPTHRANGEPROC dqglDepthRange = nullptr;
PFNGLDISABLEPROC dqglDisable = nullptr;
PFNGLDISABLEVERTEXATTRIBARRAYPROC dqglDisableVertexAttribArray = nullptr;
PFNGLDRAWARRAYSPROC dqglDrawArrays = nullptr;
PFNGLDRAWARRAYSINSTANCEDPROC dqglDrawArraysInstanced = nullptr;
PFNGLDRAWBUFFERPROC dqglDrawBuffer = nullptr;
PFNGLDRAWBUFFERSPROC dqglDrawBuffers = nullptr;
PFNGLDRAWELEMENTSPROC dqglDrawElements = nullptr;
PFNGLDRAWELEMENTSINSTANCEDPROC dqglDrawElementsInstanced = nullptr;
PFNGLENABLEPROC dqglEnable = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC dqglEnableVertexAttribArray = nullptr;
PFNGLENDQUERYPROC dqglEndQuery = nullptr;
PFNGLFENCESYNCPROC dqglFenceSync = nullptr;
PFNGLFINISHPROC dqglFinish = nullptr;
PFNGLFLUSHPROC dqglFlush = nullptr;
PFNGLFRAMEBUFFERRENDERBUFFERPROC dqglFramebufferRenderbuffer = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DPROC dqglFramebufferTexture2D = nullptr;
PFNGLFRONTFACEPROC dqglFrontFace = nullptr;
PFNGLGENBUFFERSPROC dqglGenBuffers = nullptr;
PFNGLGENFRAMEBUFFERSPROC dqglGenFramebuffers = nullptr;
PFNGLGENQUERIESPROC dqglGenQueries = nullptr;
PFNGLGENRENDERBUFFERSPROC dqglGenRenderbuffers = nullptr;
PFNGLGENTEXTURESPROC dqglGenTextures = nullptr;
PFNGLGENVERTEXARRAYSPROC dqglGenVertexArrays = nullptr;
PFNGLGENERATEMIPMAPPROC dqglGenerateMipmap = nullptr;
PFNGLGETACTIVEUNIFORMPROC dqglGetActiveUniform = nullptr;
PFNGLGETATTRIBLOCATIONPROC dqglGetAttribLocation = nullptr;
PFNGLGETBOOLEANVPROC dqglGetBooleanv = nullptr;
PFNGLGETBUFFERSUBDATAPROC dqglGetBufferSubData = nullptr;
PFNGLGETBUFFERPARAMETERIVPROC dqglGetBufferParameteriv = nullptr;
PFNGLGETVERTEXATTRIBIVPROC dqglGetVertexAttribiv = nullptr;
PFNGLGETVERTEXATTRIBPOINTERVPROC dqglGetVertexAttribPointerv = nullptr;
PFNGLGETERRORPROC dqglGetError = nullptr;
PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC dqglGetFramebufferAttachmentParameteriv = nullptr;
PFNGLGETINTEGERVPROC dqglGetIntegerv = nullptr;
PFNGLGETTEXPARAMETERIVPROC dqglGetTexParameteriv = nullptr;
PFNGLGETPROGRAMINFOLOGPROC dqglGetProgramInfoLog = nullptr;
PFNGLGETPROGRAMIVPROC dqglGetProgramiv = nullptr;
PFNGLGETQUERYOBJECTUIVPROC dqglGetQueryObjectuiv = nullptr;
PFNGLGETQUERYOBJECTUI64VPROC dqglGetQueryObjectui64v = nullptr;
PFNGLGETSHADERINFOLOGPROC dqglGetShaderInfoLog = nullptr;
PFNGLGETSHADERIVPROC dqglGetShaderiv = nullptr;
PFNGLGETSTRINGPROC dqglGetString = nullptr;
PFNGLGETSTRINGIPROC dqglGetStringi = nullptr;
PFNGLGETSYNCIVPROC dqglGetSynciv = nullptr;
PFNGLGETTEXIMAGEPROC dqglGetTexImage = nullptr;
PFNGLGETUNIFORMFVPROC dqglGetUniformfv = nullptr;
PFNGLGETUNIFORMLOCATIONPROC dqglGetUniformLocation = nullptr;
PFNGLINVALIDATEFRAMEBUFFERPROC dqglInvalidateFramebuffer = nullptr;
PFNGLISENABLEDPROC dqglIsEnabled = nullptr;
PFNGLLINEWIDTHPROC dqglLineWidth = nullptr;
PFNGLLINKPROGRAMPROC dqglLinkProgram = nullptr;
PFNGLMAPBUFFERRANGEPROC dqglMapBufferRange = nullptr;
PFNGLPIXELSTOREIPROC dqglPixelStorei = nullptr;
PFNGLPOLYGONOFFSETPROC dqglPolygonOffset = nullptr;
PFNGLPOPDEBUGGROUPPROC dqglPopDebugGroup = nullptr;
PFNGLPUSHDEBUGGROUPPROC dqglPushDebugGroup = nullptr;
PFNGLREADBUFFERPROC dqglReadBuffer = nullptr;
PFNGLREADPIXELSPROC dqglReadPixels = nullptr;
PFNGLRENDERBUFFERSTORAGEPROC dqglRenderbufferStorage = nullptr;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC dqglRenderbufferStorageMultisample = nullptr;
PFNGLSCISSORPROC dqglScissor = nullptr;
PFNGLSHADERSOURCEPROC dqglShaderSource = nullptr;
PFNGLSTENCILFUNCSEPARATEPROC dqglStencilFuncSeparate = nullptr;
PFNGLSTENCILMASKPROC dqglStencilMask = nullptr;
PFNGLSTENCILMASKSEPARATEPROC dqglStencilMaskSeparate = nullptr;
PFNGLSTENCILOPSEPARATEPROC dqglStencilOpSeparate = nullptr;
PFNGLTEXIMAGE2DPROC dqglTexImage2D = nullptr;
PFNGLTEXIMAGE2DMULTISAMPLEPROC dqglTexImage2DMultisample = nullptr;
PFNGLTEXPARAMETERIPROC dqglTexParameteri = nullptr;
PFNGLTEXSTORAGE2DPROC dqglTexStorage2D = nullptr;
PFNGLTEXSTORAGE3DPROC dqglTexStorage3D = nullptr;
PFNGLTEXSUBIMAGE2DPROC dqglTexSubImage2D = nullptr;
PFNGLTEXSUBIMAGE3DPROC dqglTexSubImage3D = nullptr;
PFNGLTEXTUREVIEWPROC dqglTextureView = nullptr;
PFNGLUNIFORM1FPROC dqglUniform1f = nullptr;
PFNGLUNIFORM1FVPROC dqglUniform1fv = nullptr;
PFNGLUNIFORM1IPROC dqglUniform1i = nullptr;
PFNGLUNIFORM1IVPROC dqglUniform1iv = nullptr;
PFNGLUNIFORM1UIPROC dqglUniform1ui = nullptr;
PFNGLUNIFORM1UIVPROC dqglUniform1uiv = nullptr;
PFNGLUNIFORM2FVPROC dqglUniform2fv = nullptr;
PFNGLUNIFORM3FPROC dqglUniform3f = nullptr;
PFNGLUNIFORM3FVPROC dqglUniform3fv = nullptr;
PFNGLUNIFORM4FPROC dqglUniform4f = nullptr;
PFNGLUNIFORM4FVPROC dqglUniform4fv = nullptr;
PFNGLUNIFORMBLOCKBINDINGPROC dqglUniformBlockBinding = nullptr;
PFNGLUNIFORMMATRIX3FVPROC dqglUniformMatrix3fv = nullptr;
PFNGLUNIFORMMATRIX4FVPROC dqglUniformMatrix4fv = nullptr;
PFNGLUNMAPBUFFERPROC dqglUnmapBuffer = nullptr;
PFNGLUSEPROGRAMPROC dqglUseProgram = nullptr;
PFNGLVERTEXATTRIBDIVISORPROC dqglVertexAttribDivisor = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC dqglVertexAttribPointer = nullptr;
PFNGLVIEWPORTPROC dqglViewport = nullptr;

namespace {

HMODULE s_opengl32 = nullptr;
bool s_initialized = false;

PROC resolve(char const* name) {
    // wglGetProcAddress 对 GL 1.1 函数返回 nullptr（文档行为）——回退 opengl32.dll
    PROC p = wglGetProcAddress(name);
    if (nullptr == p && nullptr != s_opengl32)
        p = GetProcAddress(s_opengl32, name);
    return p;
}
} // namespace

namespace dqgl {

bool init() {
    if (s_initialized)
        return true;

    s_opengl32 = LoadLibraryA("opengl32.dll");
    if (nullptr == s_opengl32)
        return false;

    // 临时窗口 + 像素格式 + legacy context：wglGetProcAddress 需要当前 context
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "dqgl_bootstrap";
    if (0 == RegisterClassA(&wc))
        return false;

    HWND hwnd = CreateWindowExA(0, "dqgl_bootstrap", "dqgl", WS_OVERLAPPED,
        0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr);
    if (nullptr == hwnd) {
        UnregisterClassA("dqgl_bootstrap", wc.hInstance);
        return false;
    }

    HDC hdc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    int pf = ChoosePixelFormat(hdc, &pfd);
    if (0 == pf || 0 == SetPixelFormat(hdc, pf, &pfd)) {
        ReleaseDC(hwnd, hdc);
        DestroyWindow(hwnd);
        UnregisterClassA("dqgl_bootstrap", wc.hInstance);
        return false;
    }

    HGLRC hglrc = wglCreateContext(hdc);
    if (nullptr == hglrc) {
        ReleaseDC(hwnd, hdc);
        DestroyWindow(hwnd);
        UnregisterClassA("dqgl_bootstrap", wc.hInstance);
        return false;
    }
    wglMakeCurrent(hdc, hglrc);

    bool allResolved = true;
    struct Entry { char const* name; void** slot; };
    Entry const entries[] = {
        { "glActiveTexture", reinterpret_cast<void**>(&dqglActiveTexture) },
        { "glAttachShader", reinterpret_cast<void**>(&dqglAttachShader) },
        { "glBeginQuery", reinterpret_cast<void**>(&dqglBeginQuery) },
        { "glBindAttribLocation", reinterpret_cast<void**>(&dqglBindAttribLocation) },
        { "glBindBuffer", reinterpret_cast<void**>(&dqglBindBuffer) },
        { "glBindFramebuffer", reinterpret_cast<void**>(&dqglBindFramebuffer) },
        { "glBindRenderbuffer", reinterpret_cast<void**>(&dqglBindRenderbuffer) },
        { "glBindTexture", reinterpret_cast<void**>(&dqglBindTexture) },
        { "glBindVertexArray", reinterpret_cast<void**>(&dqglBindVertexArray) },
        { "glBlendColor", reinterpret_cast<void**>(&dqglBlendColor) },
        { "glBlendEquationSeparate", reinterpret_cast<void**>(&dqglBlendEquationSeparate) },
        { "glBlendEquationSeparatei", reinterpret_cast<void**>(&dqglBlendEquationSeparatei) },
        { "glBlendFunc", reinterpret_cast<void**>(&dqglBlendFunc) },
        { "glBlendFuncSeparate", reinterpret_cast<void**>(&dqglBlendFuncSeparate) },
        { "glBlendFuncSeparatei", reinterpret_cast<void**>(&dqglBlendFuncSeparatei) },
        { "glBlitFramebuffer", reinterpret_cast<void**>(&dqglBlitFramebuffer) },
        { "glBufferData", reinterpret_cast<void**>(&dqglBufferData) },
        { "glBufferSubData", reinterpret_cast<void**>(&dqglBufferSubData) },
        { "glCheckFramebufferStatus", reinterpret_cast<void**>(&dqglCheckFramebufferStatus) },
        { "glClear", reinterpret_cast<void**>(&dqglClear) },
        { "glClearColor", reinterpret_cast<void**>(&dqglClearColor) },
        { "glClearDepth", reinterpret_cast<void**>(&dqglClearDepth) },
        { "glClearStencil", reinterpret_cast<void**>(&dqglClearStencil) },
        { "glClientWaitSync", reinterpret_cast<void**>(&dqglClientWaitSync) },
        { "glColorMask", reinterpret_cast<void**>(&dqglColorMask) },
        { "glCompileShader", reinterpret_cast<void**>(&dqglCompileShader) },
        { "glCreateProgram", reinterpret_cast<void**>(&dqglCreateProgram) },
        { "glCreateShader", reinterpret_cast<void**>(&dqglCreateShader) },
        { "glCullFace", reinterpret_cast<void**>(&dqglCullFace) },
        { "glDeleteBuffers", reinterpret_cast<void**>(&dqglDeleteBuffers) },
        { "glDeleteFramebuffers", reinterpret_cast<void**>(&dqglDeleteFramebuffers) },
        { "glDeleteProgram", reinterpret_cast<void**>(&dqglDeleteProgram) },
        { "glDeleteQueries", reinterpret_cast<void**>(&dqglDeleteQueries) },
        { "glDeleteRenderbuffers", reinterpret_cast<void**>(&dqglDeleteRenderbuffers) },
        { "glDeleteShader", reinterpret_cast<void**>(&dqglDeleteShader) },
        { "glDeleteSync", reinterpret_cast<void**>(&dqglDeleteSync) },
        { "glDeleteTextures", reinterpret_cast<void**>(&dqglDeleteTextures) },
        { "glDeleteVertexArrays", reinterpret_cast<void**>(&dqglDeleteVertexArrays) },
        { "glDepthFunc", reinterpret_cast<void**>(&dqglDepthFunc) },
        { "glDepthMask", reinterpret_cast<void**>(&dqglDepthMask) },
        { "glDepthRange", reinterpret_cast<void**>(&dqglDepthRange) },
        { "glDisable", reinterpret_cast<void**>(&dqglDisable) },
        { "glDisableVertexAttribArray", reinterpret_cast<void**>(&dqglDisableVertexAttribArray) },
        { "glDrawArrays", reinterpret_cast<void**>(&dqglDrawArrays) },
        { "glDrawArraysInstanced", reinterpret_cast<void**>(&dqglDrawArraysInstanced) },
        { "glDrawBuffer", reinterpret_cast<void**>(&dqglDrawBuffer) },
        { "glDrawBuffers", reinterpret_cast<void**>(&dqglDrawBuffers) },
        { "glDrawElements", reinterpret_cast<void**>(&dqglDrawElements) },
        { "glDrawElementsInstanced", reinterpret_cast<void**>(&dqglDrawElementsInstanced) },
        { "glEnable", reinterpret_cast<void**>(&dqglEnable) },
        { "glEnableVertexAttribArray", reinterpret_cast<void**>(&dqglEnableVertexAttribArray) },
        { "glEndQuery", reinterpret_cast<void**>(&dqglEndQuery) },
        { "glFenceSync", reinterpret_cast<void**>(&dqglFenceSync) },
        { "glFinish", reinterpret_cast<void**>(&dqglFinish) },
        { "glFlush", reinterpret_cast<void**>(&dqglFlush) },
        { "glFramebufferRenderbuffer", reinterpret_cast<void**>(&dqglFramebufferRenderbuffer) },
        { "glFramebufferTexture2D", reinterpret_cast<void**>(&dqglFramebufferTexture2D) },
        { "glFrontFace", reinterpret_cast<void**>(&dqglFrontFace) },
        { "glGenBuffers", reinterpret_cast<void**>(&dqglGenBuffers) },
        { "glGenFramebuffers", reinterpret_cast<void**>(&dqglGenFramebuffers) },
        { "glGenQueries", reinterpret_cast<void**>(&dqglGenQueries) },
        { "glGenRenderbuffers", reinterpret_cast<void**>(&dqglGenRenderbuffers) },
        { "glGenTextures", reinterpret_cast<void**>(&dqglGenTextures) },
        { "glGenVertexArrays", reinterpret_cast<void**>(&dqglGenVertexArrays) },
        { "glGenerateMipmap", reinterpret_cast<void**>(&dqglGenerateMipmap) },
        { "glGetActiveUniform", reinterpret_cast<void**>(&dqglGetActiveUniform) },
        { "glGetAttribLocation", reinterpret_cast<void**>(&dqglGetAttribLocation) },
        { "glGetBooleanv", reinterpret_cast<void**>(&dqglGetBooleanv) },
        { "glGetBufferSubData", reinterpret_cast<void**>(&dqglGetBufferSubData) },
        { "glGetBufferParameteriv", reinterpret_cast<void**>(&dqglGetBufferParameteriv) },
        { "glGetVertexAttribiv", reinterpret_cast<void**>(&dqglGetVertexAttribiv) },
        { "glGetVertexAttribPointerv", reinterpret_cast<void**>(&dqglGetVertexAttribPointerv) },
        { "glGetAttribLocation", reinterpret_cast<void**>(&dqglGetAttribLocation) },
        { "glGetError", reinterpret_cast<void**>(&dqglGetError) },
        { "glGetFramebufferAttachmentParameteriv", reinterpret_cast<void**>(&dqglGetFramebufferAttachmentParameteriv) },
        { "glGetIntegerv", reinterpret_cast<void**>(&dqglGetIntegerv) },
        { "glGetTexParameteriv", reinterpret_cast<void**>(&dqglGetTexParameteriv) },
        { "glGetProgramInfoLog", reinterpret_cast<void**>(&dqglGetProgramInfoLog) },
        { "glGetProgramiv", reinterpret_cast<void**>(&dqglGetProgramiv) },
        { "glGetQueryObjectuiv", reinterpret_cast<void**>(&dqglGetQueryObjectuiv) },
        { "glGetQueryObjectui64v", reinterpret_cast<void**>(&dqglGetQueryObjectui64v) },
        { "glGetShaderInfoLog", reinterpret_cast<void**>(&dqglGetShaderInfoLog) },
        { "glGetShaderiv", reinterpret_cast<void**>(&dqglGetShaderiv) },
        { "glGetString", reinterpret_cast<void**>(&dqglGetString) },
        { "glGetStringi", reinterpret_cast<void**>(&dqglGetStringi) },
        { "glGetSynciv", reinterpret_cast<void**>(&dqglGetSynciv) },
        { "glGetTexImage", reinterpret_cast<void**>(&dqglGetTexImage) },
        { "glGetUniformfv", reinterpret_cast<void**>(&dqglGetUniformfv) },
        { "glGetUniformLocation", reinterpret_cast<void**>(&dqglGetUniformLocation) },
        { "glInvalidateFramebuffer", reinterpret_cast<void**>(&dqglInvalidateFramebuffer) },
        { "glIsEnabled", reinterpret_cast<void**>(&dqglIsEnabled) },
        { "glLineWidth", reinterpret_cast<void**>(&dqglLineWidth) },
        { "glLinkProgram", reinterpret_cast<void**>(&dqglLinkProgram) },
        { "glMapBufferRange", reinterpret_cast<void**>(&dqglMapBufferRange) },
        { "glPixelStorei", reinterpret_cast<void**>(&dqglPixelStorei) },
        { "glPolygonOffset", reinterpret_cast<void**>(&dqglPolygonOffset) },
        { "glPopDebugGroup", reinterpret_cast<void**>(&dqglPopDebugGroup) },
        { "glPushDebugGroup", reinterpret_cast<void**>(&dqglPushDebugGroup) },
        { "glReadBuffer", reinterpret_cast<void**>(&dqglReadBuffer) },
        { "glReadPixels", reinterpret_cast<void**>(&dqglReadPixels) },
        { "glRenderbufferStorage", reinterpret_cast<void**>(&dqglRenderbufferStorage) },
        { "glRenderbufferStorageMultisample", reinterpret_cast<void**>(&dqglRenderbufferStorageMultisample) },
        { "glScissor", reinterpret_cast<void**>(&dqglScissor) },
        { "glShaderSource", reinterpret_cast<void**>(&dqglShaderSource) },
        { "glStencilFuncSeparate", reinterpret_cast<void**>(&dqglStencilFuncSeparate) },
        { "glStencilMask", reinterpret_cast<void**>(&dqglStencilMask) },
        { "glStencilMaskSeparate", reinterpret_cast<void**>(&dqglStencilMaskSeparate) },
        { "glStencilOpSeparate", reinterpret_cast<void**>(&dqglStencilOpSeparate) },
        { "glTexImage2D", reinterpret_cast<void**>(&dqglTexImage2D) },
        { "glTexImage2DMultisample", reinterpret_cast<void**>(&dqglTexImage2DMultisample) },
        { "glTexParameteri", reinterpret_cast<void**>(&dqglTexParameteri) },
        { "glTexStorage2D", reinterpret_cast<void**>(&dqglTexStorage2D) },
        { "glTexStorage3D", reinterpret_cast<void**>(&dqglTexStorage3D) },
        { "glTexSubImage2D", reinterpret_cast<void**>(&dqglTexSubImage2D) },
        { "glTexSubImage3D", reinterpret_cast<void**>(&dqglTexSubImage3D) },
        { "glTextureView", reinterpret_cast<void**>(&dqglTextureView) },
        { "glUniform1f", reinterpret_cast<void**>(&dqglUniform1f) },
        { "glUniform1fv", reinterpret_cast<void**>(&dqglUniform1fv) },
        { "glUniform1i", reinterpret_cast<void**>(&dqglUniform1i) },
        { "glUniform1iv", reinterpret_cast<void**>(&dqglUniform1iv) },
        { "glUniform1ui", reinterpret_cast<void**>(&dqglUniform1ui) },
        { "glUniform1uiv", reinterpret_cast<void**>(&dqglUniform1uiv) },
        { "glUniform2fv", reinterpret_cast<void**>(&dqglUniform2fv) },
        { "glUniform3f", reinterpret_cast<void**>(&dqglUniform3f) },
        { "glUniform3fv", reinterpret_cast<void**>(&dqglUniform3fv) },
        { "glUniform4f", reinterpret_cast<void**>(&dqglUniform4f) },
        { "glUniform4fv", reinterpret_cast<void**>(&dqglUniform4fv) },
        { "glUniformBlockBinding", reinterpret_cast<void**>(&dqglUniformBlockBinding) },
        { "glUniformMatrix3fv", reinterpret_cast<void**>(&dqglUniformMatrix3fv) },
        { "glUniformMatrix4fv", reinterpret_cast<void**>(&dqglUniformMatrix4fv) },
        { "glUnmapBuffer", reinterpret_cast<void**>(&dqglUnmapBuffer) },
        { "glUseProgram", reinterpret_cast<void**>(&dqglUseProgram) },
        { "glVertexAttribDivisor", reinterpret_cast<void**>(&dqglVertexAttribDivisor) },
        { "glVertexAttribPointer", reinterpret_cast<void**>(&dqglVertexAttribPointer) },
        { "glViewport", reinterpret_cast<void**>(&dqglViewport) },
    };
    for (Entry const& e : entries) {
        *e.slot = reinterpret_cast<void*>(resolve(e.name));
        if (nullptr == *e.slot)
            allResolved = false;
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);
    UnregisterClassA("dqgl_bootstrap", wc.hInstance);

    s_initialized = true;
    return allResolved;
}

} // namespace dqgl

#endif // _WIN32
