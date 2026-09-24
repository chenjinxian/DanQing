# glheaders — Khronos 官方 OpenGL 头（Windows vendor）

| 文件 | 来源 | 许可证 |
|---|---|---|
| `GL/glcorearb.h` | [KhronosGroup/OpenGL-Registry](https://github.com/KhronosGroup/OpenGL-Registry) `api/GL/glcorearb.h` | MIT（Khronos Group，见文件头 SPDX） |
| `KHR/khrplatform.h` | [KhronosGroup/EGL-Registry](https://github.com/KhronosGroup/EGL-Registry) `api/KHR/khrplatform.h` | MIT（Khronos Group，见文件头） |

## 为什么 vendor

- Windows SDK 仅自带 GL 1.1 头（`um/gl/GL.h`），现代 GL 常量（`GL_FUNC_ADD`、
  `GL_TEXTURE_MAX_ANISOTROPY_EXT` 等）与 `glext.h` 从不随 SDK 发行。
- `dqRender/src/rhi/opengl/Gl.h` 的 `_WIN32` 分支包含 `glcorearb.h`
  （macOS 走系统 `OpenGL/gl3.h`，Linux 走系统 `gl.h`/`glext.h`）。
- 与 filament vendor GL 头（`third_party/glheaders`）同做法。
- 函数符号经 `GlLoader.{h,cpp}`（bluegl 机制）运行时装载，不定义
  `GL_GLEXT_PROTOTYPES`。

## 更新

直接覆盖对应文件（Khronos registry 随 GL 版本再生成）。
