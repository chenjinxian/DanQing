// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/Desktop/FileSystem.h
// DanQing dqBase — 桌面文件系统操作
//
// 1:1 对齐 imodel-native Desktop::FileSystem。
// 桌面平台专用的文件系统操作。
#pragma once

#include "../Export.h"
#include "../DqTypes.h"
#include "../BeFileName.h"

#include <cstdint>

BEGIN_DQ_BASE_NAMESPACE

namespace Desktop {

// ---------------------------------------------------------------------------
// FileSystem — 桌面文件系统操作
// Ported from: imodel-native Desktop/FileSystem.h
// ---------------------------------------------------------------------------
struct DQ_BASE_EXPORT FileSystem {
    /// 获取临时路径
    /// Ported from: imodel-native Desktop/FileSystem.h BeGetTempPath
    static BeFileNameStatus BeGetTempPath(DqString& tempPath);

    /// 获取临时文件名
    /// Ported from: imodel-native Desktop/FileSystem.h BeGetTempFileName
    static BeFileNameStatus BeGetTempFileName(DqString& tempFileName, const DqString& pathName, const char* prefixString);

    /// 获取磁盘可用空间
    /// Ported from: imodel-native Desktop/FileSystem.h BeGetDiskFreeSpace
    static BeFileNameStatus BeGetDiskFreeSpace(uint64_t& freeBytes, const DqString& dirName);

    /// 获取当前工作目录
    /// Ported from: imodel-native Desktop/FileSystem.h GetCwd
    static BeFileNameStatus GetCwd(DqString& currentDirectory);

    /// 获取可执行文件所在目录（全路径，不含可执行文件名）。
    /// @param moduleName 可选模块名（Windows 下用于获取模块目录），nullptr 表示主可执行文件。
    /// Ported from: imodel-native Desktop/FileSystem.h GetExecutableDir
    static BeFileName GetExecutableDir(const DqString* moduleName = nullptr);

    /// 获取当前库文件所在目录（全路径，不含库名）。
    /// Ported from: imodel-native Desktop/FileSystem.h GetLibraryDir
    static BeFileName GetLibraryDir();
};

} // namespace Desktop

END_DQ_BASE_NAMESPACE
