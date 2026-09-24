// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeFileName.h
// DanQing dqBase — 文件名值类型 + 路径操作
//
// 1:1 对齐 imodel-native BeFileName：继承 DqString，提供路径操作实例方法。
// 同时保留静态方法供不需要 BeFileName 实例的场景使用。
#pragma once

#include "Export.h"
#include "DqTypes.h"

#include <cstdint>
#include <string>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// BeFileNameStatus — 文件名操作状态
// Ported from: imodel-native BeFileName.h:64-75
// ---------------------------------------------------------------------------
enum class BeFileNameStatus : int {
    Success        = 0,
    IllegalName    = 1,
    AlreadyExists  = 2,
    CantCreate     = 3,
    FileNotFound   = 4,
    CantDeleteFile = 5,
    AccessViolation = 6,
    CantDeleteDir  = 7,
    UnknownError   = 8,
};

// ---------------------------------------------------------------------------
// BeFileNameAccess — 文件访问权限
// Ported from: imodel-native BeFileName.h:80-85
// ---------------------------------------------------------------------------
enum class BeFileNameAccess : int {
    Read      = 4,
    Write     = 2,
    ReadWrite = Read | Write,
};

// ---------------------------------------------------------------------------
// FileNameParts — 文件名组成部分掩码
// Ported from: imodel-native BeFileName.h:127
// ---------------------------------------------------------------------------
enum FileNameParts : int {
    Device    = 1,
    Directory = 2,
    Basename  = 4,
    Extension = 8,
    DevAndDir = Device | Directory,
    NameAndExt = Basename | Extension,
    All       = DevAndDir | NameAndExt,
};

// ---------------------------------------------------------------------------
// BeFileName — 文件名值类型（IS-A DqString）
// Ported from: imodel-native BeFileName.h:91-581
// ---------------------------------------------------------------------------
struct DQ_BASE_EXPORT BeFileName : DqString
{
public:
    // --- 路径分隔符常量 ---
#if defined(_WIN32)
    static constexpr char kDirSeparator = '\\';
    static constexpr const char* kDirSeparatorStr = "\\";
    static constexpr char kAltDirSeparator = '/';
    static constexpr const char* kAllFilesFilter = "*.*";
#else
    static constexpr char kDirSeparator = '/';
    static constexpr const char* kDirSeparatorStr = "/";
    static constexpr char kAltDirSeparator = '\\';
    static constexpr const char* kAllFilesFilter = "*";
#endif
    static constexpr char kPathSeparator = ';';

    // --- 构造函数 ---
    /** @{ */
    BeFileName() {}

    //! 从 UTF-8 字符串构造
    explicit BeFileName(const DqString& name) { assign(name); }

    //! 从 C 字符串构造
    explicit BeFileName(const char* name) { if (name) assign(name); }

    //! 从文件名各部分构造
    BeFileName(const char* dev, const char* dir, const char* name, const char* ext)
        { BuildName(dev, dir, name, ext); }

    //! 从另一文件名的部分构造
    BeFileName(FileNameParts mask, const char* fullName);
    /** @} */

    // --- 路径修改 ---
    /** @{ */
    //! 清空文件名
    BeFileName& Clear() { clear(); return *this; }

    //! 设置文件名值
    BeFileName& SetName(const DqString& name) { assign(name); return *this; }
    BeFileName& SetName(const char* name) { if (name) assign(name); else clear(); return *this; }

    //! 从各部分构建文件名
    BeFileName& BuildName(const char* dev, const char* dir, const char* name, const char* ext);

    //! 追加路径组件（自动添加分隔符）
    BeFileName& AppendToPath(const BeFileName& additionComponent)
        { return AppendToPath(additionComponent.c_str()); }
    BeFileName& AppendToPath(const char* additionComponent);

    //! 追加扩展名（自动添加点号）
    BeFileName& AppendExtension(const char* extension);

    //! 确保路径以分隔符结尾
    BeFileName& AppendSeparator();

    //! 追加字符串
    BeFileName& AppendString(const char* str) { if (str) append(str); return *this; }

    //! 移除路径最右侧组件
    BeFileName& PopDir();

    //! 移除引号
    BeFileName& RemoveQuotes();

    //! 缩短路径到指定长度（用于 UI 显示）
    DqString Abbreviate(size_t maxLength) const;

    //! 合并多个路径组件
    BeFileName Combine(std::initializer_list<const char*> components) const;
    /** @} */

    // --- 路径解析 ---
    /** @{ */
    //! 解析完整文件名为设备、目录、文件名、扩展名
    static void ParseName(DqString* dev, DqString* dir, DqString* name, DqString* ext, const char* fullFileName);
    void ParseName(DqString* dev, DqString* dir, DqString* name, DqString* ext) const
        { ParseName(dev, dir, name, ext, c_str()); }
    /** @} */

    // --- 路径获取 ---
    /** @{ */
    //! 获取目录部分（含结尾分隔符）
    BeFileName GetDirectoryName() const;
    static DqString GetDirectoryNameStatic(const DqString& path);

    //! 获取不含设备的目录部分
    BeFileName GetDirectoryWithoutDevice() const;

    //! 获取扩展名（不含点号）
    DqString GetExtension() const;
    static DqString GetExtensionStatic(const DqString& path);

    //! 获取文件名+扩展名（不含目录）
    DqString GetFileNameAndExtension() const;
    static DqString GetFileNameAndExtensionStatic(const DqString& path);

    //! 获取文件名（不含目录和扩展名）
    DqString GetFileNameWithoutExtension() const;
    static DqString GetFileNameWithoutExtensionStatic(const DqString& path);

    //! 获取设备符（Windows 盘符，Unix 始终空）
    DqString GetDevice() const;

    //! 获取基本名称（= GetFileNameAndExtension）
    BeFileName GetBaseName() const { return BeFileName(GetFileNameAndExtension()); }
    /** @} */

    // --- 属性查询 ---
    /** @{ */
    //! 获取文件名
    const char* GetName() const { return c_str(); }

    //! 是否为空
    bool IsEmpty() const { return empty(); }

    //! 获取文件名长度
    size_t GetNameSize() const { return size(); }

    //! 是否为绝对路径
    bool IsAbsolutePath() const;
    static bool IsAbsolutePathStatic(const DqString& path);

    //! 路径是否存在
    bool DoesPathExist() const;
    static bool DoesPathExistStatic(const DqString& path);

    //! 是否为目录
    bool IsDirectory() const;
    static bool IsDirectoryStatic(const DqString& path);

    //! 是否为符号链接
    bool IsSymbolicLink() const;

    //! 是否等价于另一路径
    bool IsEquivalentTo(const char* rhs) const;
    /** @} */

    // --- 文件操作 ---
    /** @{ */
    //! 创建目录
    static BeFileNameStatus CreateNewDirectory(const DqString& path);

    //! 删除文件
    BeFileNameStatus BeDeleteFile() const;
    static BeFileNameStatus BeDeleteFileStatic(const DqString& path);

    //! 复制文件
    static BeFileNameStatus BeCopyFile(const BeFileName& existingFileName, const BeFileName& newFileName, bool failIfFileExists = false);

    //! 移动/重命名文件
    static BeFileNameStatus BeMoveFile(const BeFileName& oldFileName, const BeFileName& newFileName, int numRetries = 0);

    //! 设置只读状态
    BeFileNameStatus SetFileReadOnly(bool readOnly) const;

    //! 查询只读状态
    bool IsFileReadOnly() const;

    //! 获取文件大小
    BeFileNameStatus GetFileSize(uint64_t& sz) const;

    //! 获取文件时间
    BeFileNameStatus GetFileTime(time_t* ctime, time_t* atime, time_t* mtime) const;

    //! 设置文件时间
    BeFileNameStatus SetFileTime(const time_t* atime, const time_t* mtime) const;

    //! 检查访问权限
    BeFileNameStatus CheckAccess(BeFileNameAccess accs) const;
    /** @} */

    // --- 路径解析/规范化 ---
    /** @{ */
    //! 规范化路径（修正分隔符、移除 ./.. 等）
    static BeFileNameStatus FixPathName(DqString& path, const char* original, bool keepTrailingSeparator = true);

    //! 获取完整路径名
    BeFileNameStatus BeGetFullPathName();
    static BeFileNameStatus BeGetFullPathNameStatic(DqString& path, const char* src);

    //! 查找相对路径
    static void FindRelativePath(DqString& relativePath, const char* targetFileName, const char* rootFileName);

    //! 解析相对路径为绝对路径
    static int ResolveRelativePath(DqString& fullPath, const char* relativeFileName, const char* basePath);
    /** @} */

    // --- 静态工具方法（兼容旧 API）---
    /** @{ */
    static DqString CombineStatic(const DqString& dir, const DqString& file);
    static DqString AppendSeparatorStatic(const DqString& path);
    /** @} */
};

END_DQ_BASE_NAMESPACE
