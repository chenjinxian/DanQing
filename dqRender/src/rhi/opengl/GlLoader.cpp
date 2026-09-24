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
PFNGLACTIVETEXTUREPROC zoglActiveTexture = nullptr;
PFNGLATTACHSHADERPROC zoglAttachShader = nullptr;
PFNGLBEGINQUERYPROC zoglBeginQuery = nullptr;
PFNGLBINDATTRIBLOCATIONPROC zoglBindAttribLocation = nullptr;
PFNGLBINDBUFFERPROC zoglBindBuffer = nullptr;
PFNGLBINDFRAMEBUFFERPROC zoglBindFramebuffer = nullptr;
PFNGLBINDRENDERBUFFERPROC zoglBindRenderbuffer = nullptr;
PFNGLBINDTEXTUREPROC zoglBindTexture = nullptr;
PFNGLBINDVERTEXARRAYPROC zoglBindVertexArray = nullptr;
PFNGLBLENDCOLORPROC zoglBlendColor = nullptr;
PFNGLBLENDEQUATIONSEPARATEPROC zoglBlendEquationSeparate = nullptr;
PFNGLBLENDEQUATIONSEPARATEIPROC zoglBlendEquationSeparatei = nullptr;
PFNGLBLENDFUNCPROC zoglBlendFunc = nullptr;
PFNGLBLENDFUNCSEPARATEPROC zoglBlendFuncSeparate = nullptr;
PFNGLBLENDFUNCSEPARATEIPROC zoglBlendFuncSeparatei = nullptr;
PFNGLBLITFRAMEBUFFERPROC zoglBlitFramebuffer = nullptr;
PFNGLBUFFERDATAPROC zoglBufferData = nullptr;
PFNGLBUFFERSUBDATAPROC zoglBufferSubData = nullptr;
PFNGLCHECKFRAMEBUFFERSTATUSPROC zoglCheckFramebufferStatus = nullptr;
PFNGLCLEARPROC zoglClear = nullptr;
PFNGLCLEARCOLORPROC zoglClearColor = nullptr;
PFNGLCLEARDEPTHPROC zoglClearDepth = nullptr;
PFNGLCLEARSTENCILPROC zoglClearStencil = nullptr;
PFNGLCLIENTWAITSYNCPROC zoglClientWaitSync = nullptr;
PFNGLCOLORMASKPROC zoglColorMask = nullptr;
PFNGLCOMPILESHADERPROC zoglCompileShader = nullptr;
PFNGLCREATEPROGRAMPROC zoglCreateProgram = nullptr;
PFNGLCREATESHADERPROC zoglCreateShader = nullptr;
PFNGLCULLFACEPROC zoglCullFace = nullptr;
PFNGLDELETEBUFFERSPROC zoglDeleteBuffers = nullptr;
PFNGLDELETEFRAMEBUFFERSPROC zoglDeleteFramebuffers = nullptr;
PFNGLDELETEPROGRAMPROC zoglDeleteProgram = nullptr;
PFNGLDELETEQUERIESPROC zoglDeleteQueries = nullptr;
PFNGLDELETERENDERBUFFERSPROC zoglDeleteRenderbuffers = nullptr;
PFNGLDELETESHADERPROC zoglDeleteShader = nullptr;
PFNGLDELETESYNCPROC zoglDeleteSync = nullptr;
PFNGLDELETETEXTURESPROC zoglDeleteTextures = nullptr;
PFNGLDELETEVERTEXARRAYSPROC zoglDeleteVertexArrays = nullptr;
PFNGLDEPTHFUNCPROC zoglDepthFunc = nullptr;
PFNGLDEPTHMASKPROC zoglDepthMask = nullptr;
PFNGLDEPTHRANGEPROC zoglDepthRange = nullptr;
PFNGLDISABLEPROC zoglDisable = nullptr;
PFNGLDISABLEVERTEXATTRIBARRAYPROC zoglDisableVertexAttribArray = nullptr;
PFNGLDRAWARRAYSPROC zoglDrawArrays = nullptr;
PFNGLDRAWARRAYSINSTANCEDPROC zoglDrawArraysInstanced = nullptr;
PFNGLDRAWBUFFERPROC zoglDrawBuffer = nullptr;
PFNGLDRAWBUFFERSPROC zoglDrawBuffers = nullptr;
PFNGLDRAWELEMENTSPROC zoglDrawElements = nullptr;
PFNGLDRAWELEMENTSINSTANCEDPROC zoglDrawElementsInstanced = nullptr;
PFNGLENABLEPROC zoglEnable = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC zoglEnableVertexAttribArray = nullptr;
PFNGLENDQUERYPROC zoglEndQuery = nullptr;
PFNGLFENCESYNCPROC zoglFenceSync = nullptr;
PFNGLFINISHPROC zoglFinish = nullptr;
PFNGLFLUSHPROC zoglFlush = nullptr;
PFNGLFRAMEBUFFERRENDERBUFFERPROC zoglFramebufferRenderbuffer = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DPROC zoglFramebufferTexture2D = nullptr;
PFNGLFRONTFACEPROC zoglFrontFace = nullptr;
PFNGLGENBUFFERSPROC zoglGenBuffers = nullptr;
PFNGLGENFRAMEBUFFERSPROC zoglGenFramebuffers = nullptr;
PFNGLGENQUERIESPROC zoglGenQueries = nullptr;
PFNGLGENRENDERBUFFERSPROC zoglGenRenderbuffers = nullptr;
PFNGLGENTEXTURESPROC zoglGenTextures = nullptr;
PFNGLGENVERTEXARRAYSPROC zoglGenVertexArrays = nullptr;
PFNGLGENERATEMIPMAPPROC zoglGenerateMipmap = nullptr;
PFNGLGETACTIVEUNIFORMPROC zoglGetActiveUniform = nullptr;
PFNGLGETATTRIBLOCATIONPROC zoglGetAttribLocation = nullptr;
PFNGLGETBOOLEANVPROC zoglGetBooleanv = nullptr;
PFNGLGETBUFFERSUBDATAPROC zoglGetBufferSubData = nullptr;
PFNGLGETBUFFERPARAMETERIVPROC zoglGetBufferParameteriv = nullptr;
PFNGLGETVERTEXATTRIBIVPROC zoglGetVertexAttribiv = nullptr;
PFNGLGETVERTEXATTRIBPOINTERVPROC zoglGetVertexAttribPointerv = nullptr;
PFNGLGETERRORPROC zoglGetError = nullptr;
PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC zoglGetFramebufferAttachmentParameteriv = nullptr;
PFNGLGETINTEGERVPROC zoglGetIntegerv = nullptr;
PFNGLGETTEXPARAMETERIVPROC zoglGetTexParameteriv = nullptr;
PFNGLGETPROGRAMINFOLOGPROC zoglGetProgramInfoLog = nullptr;
PFNGLGETPROGRAMIVPROC zoglGetProgramiv = nullptr;
PFNGLGETQUERYOBJECTUIVPROC zoglGetQueryObjectuiv = nullptr;
PFNGLGETQUERYOBJECTUI64VPROC zoglGetQueryObjectui64v = nullptr;
PFNGLGETSHADERINFOLOGPROC zoglGetShaderInfoLog = nullptr;
PFNGLGETSHADERIVPROC zoglGetShaderiv = nullptr;
PFNGLGETSTRINGPROC zoglGetString = nullptr;
PFNGLGETSTRINGIPROC zoglGetStringi = nullptr;
PFNGLGETSYNCIVPROC zoglGetSynciv = nullptr;
PFNGLGETTEXIMAGEPROC zoglGetTexImage = nullptr;
PFNGLGETUNIFORMFVPROC zoglGetUniformfv = nullptr;
PFNGLGETUNIFORMLOCATIONPROC zoglGetUniformLocation = nullptr;
PFNGLINVALIDATEFRAMEBUFFERPROC zoglInvalidateFramebuffer = nullptr;
PFNGLISENABLEDPROC zoglIsEnabled = nullptr;
PFNGLLINEWIDTHPROC zoglLineWidth = nullptr;
PFNGLLINKPROGRAMPROC zoglLinkProgram = nullptr;
PFNGLMAPBUFFERRANGEPROC zoglMapBufferRange = nullptr;
PFNGLPIXELSTOREIPROC zoglPixelStorei = nullptr;
PFNGLPOLYGONOFFSETPROC zoglPolygonOffset = nullptr;
PFNGLPOPDEBUGGROUPPROC zoglPopDebugGroup = nullptr;
PFNGLPUSHDEBUGGROUPPROC zoglPushDebugGroup = nullptr;
PFNGLREADBUFFERPROC zoglReadBuffer = nullptr;
PFNGLREADPIXELSPROC zoglReadPixels = nullptr;
PFNGLRENDERBUFFERSTORAGEPROC zoglRenderbufferStorage = nullptr;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC zoglRenderbufferStorageMultisample = nullptr;
PFNGLSCISSORPROC zoglScissor = nullptr;
PFNGLSHADERSOURCEPROC zoglShaderSource = nullptr;
PFNGLSTENCILFUNCSEPARATEPROC zoglStencilFuncSeparate = nullptr;
PFNGLSTENCILMASKPROC zoglStencilMask = nullptr;
PFNGLSTENCILMASKSEPARATEPROC zoglStencilMaskSeparate = nullptr;
PFNGLSTENCILOPSEPARATEPROC zoglStencilOpSeparate = nullptr;
PFNGLTEXIMAGE2DPROC zoglTexImage2D = nullptr;
PFNGLTEXIMAGE2DMULTISAMPLEPROC zoglTexImage2DMultisample = nullptr;
PFNGLTEXPARAMETERIPROC zoglTexParameteri = nullptr;
PFNGLTEXSTORAGE2DPROC zoglTexStorage2D = nullptr;
PFNGLTEXSTORAGE3DPROC zoglTexStorage3D = nullptr;
PFNGLTEXSUBIMAGE2DPROC zoglTexSubImage2D = nullptr;
PFNGLTEXSUBIMAGE3DPROC zoglTexSubImage3D = nullptr;
PFNGLTEXTUREVIEWPROC zoglTextureView = nullptr;
PFNGLUNIFORM1FPROC zoglUniform1f = nullptr;
PFNGLUNIFORM1FVPROC zoglUniform1fv = nullptr;
PFNGLUNIFORM1IPROC zoglUniform1i = nullptr;
PFNGLUNIFORM1IVPROC zoglUniform1iv = nullptr;
PFNGLUNIFORM1UIPROC zoglUniform1ui = nullptr;
PFNGLUNIFORM1UIVPROC zoglUniform1uiv = nullptr;
PFNGLUNIFORM2FVPROC zoglUniform2fv = nullptr;
PFNGLUNIFORM3FPROC zoglUniform3f = nullptr;
PFNGLUNIFORM3FVPROC zoglUniform3fv = nullptr;
PFNGLUNIFORM4FPROC zoglUniform4f = nullptr;
PFNGLUNIFORM4FVPROC zoglUniform4fv = nullptr;
PFNGLUNIFORMBLOCKBINDINGPROC zoglUniformBlockBinding = nullptr;
PFNGLUNIFORMMATRIX3FVPROC zoglUniformMatrix3fv = nullptr;
PFNGLUNIFORMMATRIX4FVPROC zoglUniformMatrix4fv = nullptr;
PFNGLUNMAPBUFFERPROC zoglUnmapBuffer = nullptr;
PFNGLUSEPROGRAMPROC zoglUseProgram = nullptr;
PFNGLVERTEXATTRIBDIVISORPROC zoglVertexAttribDivisor = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC zoglVertexAttribPointer = nullptr;
PFNGLVIEWPORTPROC zoglViewport = nullptr;

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

namespace zogl {

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
    wc.lpszClassName = "zogl_bootstrap";
    if (0 == RegisterClassA(&wc))
        return false;

    HWND hwnd = CreateWindowExA(0, "zogl_bootstrap", "zogl", WS_OVERLAPPED,
        0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr);
    if (nullptr == hwnd) {
        UnregisterClassA("zogl_bootstrap", wc.hInstance);
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
        UnregisterClassA("zogl_bootstrap", wc.hInstance);
        return false;
    }

    HGLRC hglrc = wglCreateContext(hdc);
    if (nullptr == hglrc) {
        ReleaseDC(hwnd, hdc);
        DestroyWindow(hwnd);
        UnregisterClassA("zogl_bootstrap", wc.hInstance);
        return false;
    }
    wglMakeCurrent(hdc, hglrc);

    bool allResolved = true;
    struct Entry { char const* name; void** slot; };
    Entry const entries[] = {
        { "glActiveTexture", reinterpret_cast<void**>(&zoglActiveTexture) },
        { "glAttachShader", reinterpret_cast<void**>(&zoglAttachShader) },
        { "glBeginQuery", reinterpret_cast<void**>(&zoglBeginQuery) },
        { "glBindAttribLocation", reinterpret_cast<void**>(&zoglBindAttribLocation) },
        { "glBindBuffer", reinterpret_cast<void**>(&zoglBindBuffer) },
        { "glBindFramebuffer", reinterpret_cast<void**>(&zoglBindFramebuffer) },
        { "glBindRenderbuffer", reinterpret_cast<void**>(&zoglBindRenderbuffer) },
        { "glBindTexture", reinterpret_cast<void**>(&zoglBindTexture) },
        { "glBindVertexArray", reinterpret_cast<void**>(&zoglBindVertexArray) },
        { "glBlendColor", reinterpret_cast<void**>(&zoglBlendColor) },
        { "glBlendEquationSeparate", reinterpret_cast<void**>(&zoglBlendEquationSeparate) },
        { "glBlendEquationSeparatei", reinterpret_cast<void**>(&zoglBlendEquationSeparatei) },
        { "glBlendFunc", reinterpret_cast<void**>(&zoglBlendFunc) },
        { "glBlendFuncSeparate", reinterpret_cast<void**>(&zoglBlendFuncSeparate) },
        { "glBlendFuncSeparatei", reinterpret_cast<void**>(&zoglBlendFuncSeparatei) },
        { "glBlitFramebuffer", reinterpret_cast<void**>(&zoglBlitFramebuffer) },
        { "glBufferData", reinterpret_cast<void**>(&zoglBufferData) },
        { "glBufferSubData", reinterpret_cast<void**>(&zoglBufferSubData) },
        { "glCheckFramebufferStatus", reinterpret_cast<void**>(&zoglCheckFramebufferStatus) },
        { "glClear", reinterpret_cast<void**>(&zoglClear) },
        { "glClearColor", reinterpret_cast<void**>(&zoglClearColor) },
        { "glClearDepth", reinterpret_cast<void**>(&zoglClearDepth) },
        { "glClearStencil", reinterpret_cast<void**>(&zoglClearStencil) },
        { "glClientWaitSync", reinterpret_cast<void**>(&zoglClientWaitSync) },
        { "glColorMask", reinterpret_cast<void**>(&zoglColorMask) },
        { "glCompileShader", reinterpret_cast<void**>(&zoglCompileShader) },
        { "glCreateProgram", reinterpret_cast<void**>(&zoglCreateProgram) },
        { "glCreateShader", reinterpret_cast<void**>(&zoglCreateShader) },
        { "glCullFace", reinterpret_cast<void**>(&zoglCullFace) },
        { "glDeleteBuffers", reinterpret_cast<void**>(&zoglDeleteBuffers) },
        { "glDeleteFramebuffers", reinterpret_cast<void**>(&zoglDeleteFramebuffers) },
        { "glDeleteProgram", reinterpret_cast<void**>(&zoglDeleteProgram) },
        { "glDeleteQueries", reinterpret_cast<void**>(&zoglDeleteQueries) },
        { "glDeleteRenderbuffers", reinterpret_cast<void**>(&zoglDeleteRenderbuffers) },
        { "glDeleteShader", reinterpret_cast<void**>(&zoglDeleteShader) },
        { "glDeleteSync", reinterpret_cast<void**>(&zoglDeleteSync) },
        { "glDeleteTextures", reinterpret_cast<void**>(&zoglDeleteTextures) },
        { "glDeleteVertexArrays", reinterpret_cast<void**>(&zoglDeleteVertexArrays) },
        { "glDepthFunc", reinterpret_cast<void**>(&zoglDepthFunc) },
        { "glDepthMask", reinterpret_cast<void**>(&zoglDepthMask) },
        { "glDepthRange", reinterpret_cast<void**>(&zoglDepthRange) },
        { "glDisable", reinterpret_cast<void**>(&zoglDisable) },
        { "glDisableVertexAttribArray", reinterpret_cast<void**>(&zoglDisableVertexAttribArray) },
        { "glDrawArrays", reinterpret_cast<void**>(&zoglDrawArrays) },
        { "glDrawArraysInstanced", reinterpret_cast<void**>(&zoglDrawArraysInstanced) },
        { "glDrawBuffer", reinterpret_cast<void**>(&zoglDrawBuffer) },
        { "glDrawBuffers", reinterpret_cast<void**>(&zoglDrawBuffers) },
        { "glDrawElements", reinterpret_cast<void**>(&zoglDrawElements) },
        { "glDrawElementsInstanced", reinterpret_cast<void**>(&zoglDrawElementsInstanced) },
        { "glEnable", reinterpret_cast<void**>(&zoglEnable) },
        { "glEnableVertexAttribArray", reinterpret_cast<void**>(&zoglEnableVertexAttribArray) },
        { "glEndQuery", reinterpret_cast<void**>(&zoglEndQuery) },
        { "glFenceSync", reinterpret_cast<void**>(&zoglFenceSync) },
        { "glFinish", reinterpret_cast<void**>(&zoglFinish) },
        { "glFlush", reinterpret_cast<void**>(&zoglFlush) },
        { "glFramebufferRenderbuffer", reinterpret_cast<void**>(&zoglFramebufferRenderbuffer) },
        { "glFramebufferTexture2D", reinterpret_cast<void**>(&zoglFramebufferTexture2D) },
        { "glFrontFace", reinterpret_cast<void**>(&zoglFrontFace) },
        { "glGenBuffers", reinterpret_cast<void**>(&zoglGenBuffers) },
        { "glGenFramebuffers", reinterpret_cast<void**>(&zoglGenFramebuffers) },
        { "glGenQueries", reinterpret_cast<void**>(&zoglGenQueries) },
        { "glGenRenderbuffers", reinterpret_cast<void**>(&zoglGenRenderbuffers) },
        { "glGenTextures", reinterpret_cast<void**>(&zoglGenTextures) },
        { "glGenVertexArrays", reinterpret_cast<void**>(&zoglGenVertexArrays) },
        { "glGenerateMipmap", reinterpret_cast<void**>(&zoglGenerateMipmap) },
        { "glGetActiveUniform", reinterpret_cast<void**>(&zoglGetActiveUniform) },
        { "glGetAttribLocation", reinterpret_cast<void**>(&zoglGetAttribLocation) },
        { "glGetBooleanv", reinterpret_cast<void**>(&zoglGetBooleanv) },
        { "glGetBufferSubData", reinterpret_cast<void**>(&zoglGetBufferSubData) },
        { "glGetBufferParameteriv", reinterpret_cast<void**>(&zoglGetBufferParameteriv) },
        { "glGetVertexAttribiv", reinterpret_cast<void**>(&zoglGetVertexAttribiv) },
        { "glGetVertexAttribPointerv", reinterpret_cast<void**>(&zoglGetVertexAttribPointerv) },
        { "glGetAttribLocation", reinterpret_cast<void**>(&zoglGetAttribLocation) },
        { "glGetError", reinterpret_cast<void**>(&zoglGetError) },
        { "glGetFramebufferAttachmentParameteriv", reinterpret_cast<void**>(&zoglGetFramebufferAttachmentParameteriv) },
        { "glGetIntegerv", reinterpret_cast<void**>(&zoglGetIntegerv) },
        { "glGetTexParameteriv", reinterpret_cast<void**>(&zoglGetTexParameteriv) },
        { "glGetProgramInfoLog", reinterpret_cast<void**>(&zoglGetProgramInfoLog) },
        { "glGetProgramiv", reinterpret_cast<void**>(&zoglGetProgramiv) },
        { "glGetQueryObjectuiv", reinterpret_cast<void**>(&zoglGetQueryObjectuiv) },
        { "glGetQueryObjectui64v", reinterpret_cast<void**>(&zoglGetQueryObjectui64v) },
        { "glGetShaderInfoLog", reinterpret_cast<void**>(&zoglGetShaderInfoLog) },
        { "glGetShaderiv", reinterpret_cast<void**>(&zoglGetShaderiv) },
        { "glGetString", reinterpret_cast<void**>(&zoglGetString) },
        { "glGetStringi", reinterpret_cast<void**>(&zoglGetStringi) },
        { "glGetSynciv", reinterpret_cast<void**>(&zoglGetSynciv) },
        { "glGetTexImage", reinterpret_cast<void**>(&zoglGetTexImage) },
        { "glGetUniformfv", reinterpret_cast<void**>(&zoglGetUniformfv) },
        { "glGetUniformLocation", reinterpret_cast<void**>(&zoglGetUniformLocation) },
        { "glInvalidateFramebuffer", reinterpret_cast<void**>(&zoglInvalidateFramebuffer) },
        { "glIsEnabled", reinterpret_cast<void**>(&zoglIsEnabled) },
        { "glLineWidth", reinterpret_cast<void**>(&zoglLineWidth) },
        { "glLinkProgram", reinterpret_cast<void**>(&zoglLinkProgram) },
        { "glMapBufferRange", reinterpret_cast<void**>(&zoglMapBufferRange) },
        { "glPixelStorei", reinterpret_cast<void**>(&zoglPixelStorei) },
        { "glPolygonOffset", reinterpret_cast<void**>(&zoglPolygonOffset) },
        { "glPopDebugGroup", reinterpret_cast<void**>(&zoglPopDebugGroup) },
        { "glPushDebugGroup", reinterpret_cast<void**>(&zoglPushDebugGroup) },
        { "glReadBuffer", reinterpret_cast<void**>(&zoglReadBuffer) },
        { "glReadPixels", reinterpret_cast<void**>(&zoglReadPixels) },
        { "glRenderbufferStorage", reinterpret_cast<void**>(&zoglRenderbufferStorage) },
        { "glRenderbufferStorageMultisample", reinterpret_cast<void**>(&zoglRenderbufferStorageMultisample) },
        { "glScissor", reinterpret_cast<void**>(&zoglScissor) },
        { "glShaderSource", reinterpret_cast<void**>(&zoglShaderSource) },
        { "glStencilFuncSeparate", reinterpret_cast<void**>(&zoglStencilFuncSeparate) },
        { "glStencilMask", reinterpret_cast<void**>(&zoglStencilMask) },
        { "glStencilMaskSeparate", reinterpret_cast<void**>(&zoglStencilMaskSeparate) },
        { "glStencilOpSeparate", reinterpret_cast<void**>(&zoglStencilOpSeparate) },
        { "glTexImage2D", reinterpret_cast<void**>(&zoglTexImage2D) },
        { "glTexImage2DMultisample", reinterpret_cast<void**>(&zoglTexImage2DMultisample) },
        { "glTexParameteri", reinterpret_cast<void**>(&zoglTexParameteri) },
        { "glTexStorage2D", reinterpret_cast<void**>(&zoglTexStorage2D) },
        { "glTexStorage3D", reinterpret_cast<void**>(&zoglTexStorage3D) },
        { "glTexSubImage2D", reinterpret_cast<void**>(&zoglTexSubImage2D) },
        { "glTexSubImage3D", reinterpret_cast<void**>(&zoglTexSubImage3D) },
        { "glTextureView", reinterpret_cast<void**>(&zoglTextureView) },
        { "glUniform1f", reinterpret_cast<void**>(&zoglUniform1f) },
        { "glUniform1fv", reinterpret_cast<void**>(&zoglUniform1fv) },
        { "glUniform1i", reinterpret_cast<void**>(&zoglUniform1i) },
        { "glUniform1iv", reinterpret_cast<void**>(&zoglUniform1iv) },
        { "glUniform1ui", reinterpret_cast<void**>(&zoglUniform1ui) },
        { "glUniform1uiv", reinterpret_cast<void**>(&zoglUniform1uiv) },
        { "glUniform2fv", reinterpret_cast<void**>(&zoglUniform2fv) },
        { "glUniform3f", reinterpret_cast<void**>(&zoglUniform3f) },
        { "glUniform3fv", reinterpret_cast<void**>(&zoglUniform3fv) },
        { "glUniform4f", reinterpret_cast<void**>(&zoglUniform4f) },
        { "glUniform4fv", reinterpret_cast<void**>(&zoglUniform4fv) },
        { "glUniformBlockBinding", reinterpret_cast<void**>(&zoglUniformBlockBinding) },
        { "glUniformMatrix3fv", reinterpret_cast<void**>(&zoglUniformMatrix3fv) },
        { "glUniformMatrix4fv", reinterpret_cast<void**>(&zoglUniformMatrix4fv) },
        { "glUnmapBuffer", reinterpret_cast<void**>(&zoglUnmapBuffer) },
        { "glUseProgram", reinterpret_cast<void**>(&zoglUseProgram) },
        { "glVertexAttribDivisor", reinterpret_cast<void**>(&zoglVertexAttribDivisor) },
        { "glVertexAttribPointer", reinterpret_cast<void**>(&zoglVertexAttribPointer) },
        { "glViewport", reinterpret_cast<void**>(&zoglViewport) },
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
    UnregisterClassA("zogl_bootstrap", wc.hInstance);

    s_initialized = true;
    return allResolved;
}

} // namespace zogl

#endif // _WIN32
