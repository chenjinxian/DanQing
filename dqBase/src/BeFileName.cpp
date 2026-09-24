// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeFileName.h
//              and iModelCore/Bentley/src/BeFileName.cpp
// DanQing dqBase — 文件名操作实现
#include "dqBase/BeFileName.h"
#include "dqBase/DqTime.h" // SetFileTime 用 DqTimeUtilities::ConvertUnixTimeToFiletime（ref 同）

#include <algorithm>
#include <cstring>
#include <sys/stat.h>

#if defined(_WIN32)
    #include <windows.h>
    #include <direct.h>
    #include <fcntl.h>   // _O_RDWR / _O_BINARY（SetFileTime，ref:1987）
    #include <io.h>      // _sopen_s / _get_osfhandle / _close
    #include <share.h>   // _SH_DENYNO
#else
    #include <unistd.h>
    #include <errno.h>
    #include <libgen.h>
    #include <climits>
    #include <utime.h>
#endif

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// 构造函数
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeFileName.cpp — BeFileName(FileNameParts, WCharCP)
BeFileName::BeFileName(FileNameParts mask, const char* fullName) {
    DqString dev, dir, name, ext;
    ParseName(&dev, &dir, &name, &ext, fullName);
    if (mask & Device)    append(dev);
    if (mask & Directory) append(dir);
    if (mask & Basename)  append(name);
    if (mask & Extension) append(ext);
}

// ---------------------------------------------------------------------------
// 路径修改
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeFileName.cpp — BuildName
BeFileName& BeFileName::BuildName(const char* dev, const char* dir, const char* name, const char* ext) {
    clear();
    if (dev && dev[0]) append(dev);
    if (dir && dir[0]) append(dir);
    if (name && name[0]) append(name);
    if (ext && ext[0]) {
        if (ext[0] != '.') append(".");
        append(ext);
    }
    return *this;
}

// Ported from: imodel-native BeFileName.cpp — AppendToPath
BeFileName& BeFileName::AppendToPath(const char* additionComponent) {
    if (!additionComponent || !additionComponent[0])
        return *this;
    if (!empty() && back() != kDirSeparator && back() != kAltDirSeparator)
        push_back(kDirSeparator);
    append(additionComponent);
    return *this;
}

// Ported from: imodel-native BeFileName.cpp — AppendExtension
BeFileName& BeFileName::AppendExtension(const char* extension) {
    if (!extension || !extension[0])
        return *this;
    if (extension[0] != '.')
        append(".");
    append(extension);
    return *this;
}

// Ported from: imodel-native BeFileName.cpp — AppendSeparator
BeFileName& BeFileName::AppendSeparator() {
    // 两种分隔符皆识别为「已有分隔符」（Windows 输入常含 '/'，避免追加出 "/tmp/\"）
    if (!empty() && back() != kDirSeparator && back() != kAltDirSeparator)
        push_back(kDirSeparator);
    return *this;
}

// Ported from: imodel-native BeFileName.cpp — PopDir
BeFileName& BeFileName::PopDir() {
    if (empty()) return *this;
    // Remove trailing separator if present（两种分隔符皆识别——ref _wsplitpath 同样
    // 接受 windows 与 unix 分隔符；Windows 下输入常含 '/'）
    if (back() == kDirSeparator || back() == kAltDirSeparator)
        pop_back();
    // Find last separator（同上，两种皆查）
    DqString seps{kDirSeparator, kAltDirSeparator};
    auto pos = find_last_of(seps);
    if (pos == DqString::npos)
        clear();
    else
        erase(pos + 1);
    return *this;
}

// Ported from: imodel-native BeFileName.cpp — RemoveQuotes
BeFileName& BeFileName::RemoveQuotes() {
    if (size() >= 2 && front() == '"' && back() == '"') {
        erase(0, 1);
        pop_back();
    }
    return *this;
}

// Ported from: imodel-native BeFileName.cpp — Abbreviate
DqString BeFileName::Abbreviate(size_t maxLength) const {
    if (size() <= maxLength)
        return *this;
    if (maxLength <= 3)
        return "...";
    return "..." + substr(size() - maxLength + 3);
}

// Ported from: imodel-native BeFileName.cpp — Combine
BeFileName BeFileName::Combine(std::initializer_list<const char*> components) const {
    BeFileName result(*this);
    for (const auto* comp : components) {
        if (comp && comp[0])
            result.AppendToPath(comp);
    }
    return result;
}

// ---------------------------------------------------------------------------
// 路径解析
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeFileName.cpp — ParseName
void BeFileName::ParseName(DqString* dev, DqString* dir, DqString* name, DqString* ext, const char* fullFileName) {
    if (dev) dev->clear();
    if (dir) dir->clear();
    if (name) name->clear();
    if (ext) ext->clear();

    if (!fullFileName || !fullFileName[0])
        return;

    DqString path(fullFileName);

    // Find device (e.g., "C:" on Windows)
    size_t dirStart = 0;
#if defined(_WIN32)
    if (path.size() >= 2 && path[1] == ':') {
        if (dev) *dev = path.substr(0, 2);
        dirStart = 2;
    }
#endif

    // Find last separator
    size_t lastSep = path.find_last_of(kDirSeparator);
    if (lastSep == DqString::npos)
        lastSep = path.find_last_of(kAltDirSeparator);

    if (lastSep != DqString::npos && lastSep >= dirStart) {
        if (dir) *dir = path.substr(dirStart, lastSep - dirStart + 1);
        // File name + extension
        DqString filePart = path.substr(lastSep + 1);
        size_t dotPos = filePart.find_last_of('.');
        if (dotPos != DqString::npos && dotPos > 0) {
            if (name) *name = filePart.substr(0, dotPos);
            if (ext) *ext = filePart.substr(dotPos);
        } else {
            if (name) *name = filePart;
        }
    } else {
        // No separator — entire path is filename
        DqString filePart = path.substr(dirStart);
        size_t dotPos = filePart.find_last_of('.');
        if (dotPos != DqString::npos && dotPos > 0) {
            if (name) *name = filePart.substr(0, dotPos);
            if (ext) *ext = filePart.substr(dotPos);
        } else {
            if (name) *name = filePart;
        }
    }
}

// ---------------------------------------------------------------------------
// 路径获取
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeFileName.cpp — GetDirectoryName
BeFileName BeFileName::GetDirectoryName() const {
    BeFileName result;
    auto pos = find_last_of(kDirSeparator);
    if (pos == DqString::npos) {
        pos = find_last_of(kAltDirSeparator);
    }
    if (pos != DqString::npos)
        result.assign(substr(0, pos + 1));
    return result;
}

DqString BeFileName::GetDirectoryNameStatic(const DqString& path) {
    auto pos = path.find_last_of(BeFileName::kDirSeparator);
    if (pos == DqString::npos)
        pos = path.find_last_of(BeFileName::kAltDirSeparator);
    if (pos == DqString::npos) return ".";
    return path.substr(0, pos + 1);
}

// Ported from: imodel-native BeFileName.cpp — GetDirectoryWithoutDevice
BeFileName BeFileName::GetDirectoryWithoutDevice() const {
    BeFileName dir = GetDirectoryName();
#if defined(_WIN32)
    if (dir.size() >= 2 && dir[1] == ':')
        dir.erase(0, 2);
#endif
    return dir;
}

// Ported from: imodel-native BeFileName.cpp — GetExtension
DqString BeFileName::GetExtension() const {
    DqString name = GetFileNameAndExtension();
    auto pos = name.find_last_of('.');
    if (pos == DqString::npos) return "";
    return name.substr(pos + 1);  // 不含点号（对齐参考）
}

DqString BeFileName::GetExtensionStatic(const DqString& path) {
    DqString name = GetFileNameAndExtensionStatic(path);
    auto pos = name.find_last_of('.');
    if (pos == DqString::npos) return "";
    return name.substr(pos + 1);
}

// Ported from: imodel-native BeFileName.cpp — GetFileNameAndExtension
DqString BeFileName::GetFileNameAndExtension() const {
    auto pos = find_last_of(kDirSeparator);
    if (pos == DqString::npos)
        pos = find_last_of(kAltDirSeparator);
    if (pos == DqString::npos) return *this;
    return substr(pos + 1);
}

DqString BeFileName::GetFileNameAndExtensionStatic(const DqString& path) {
    auto pos = path.find_last_of(BeFileName::kDirSeparator);
    if (pos == DqString::npos)
        pos = path.find_last_of(BeFileName::kAltDirSeparator);
    if (pos == DqString::npos) return path;
    return path.substr(pos + 1);
}

// Ported from: imodel-native BeFileName.cpp — GetFileNameWithoutExtension
DqString BeFileName::GetFileNameWithoutExtension() const {
    DqString name = GetFileNameAndExtension();
    auto pos = name.find_last_of('.');
    if (pos == DqString::npos) return name;
    return name.substr(0, pos);
}

DqString BeFileName::GetFileNameWithoutExtensionStatic(const DqString& path) {
    DqString name = GetFileNameAndExtensionStatic(path);
    auto pos = name.find_last_of('.');
    if (pos == DqString::npos) return name;
    return name.substr(0, pos);
}

// Ported from: imodel-native BeFileName.cpp — GetDevice
DqString BeFileName::GetDevice() const {
#if defined(_WIN32)
    if (size() >= 2 && (*this)[1] == ':')
        return substr(0, 2);
#endif
    return "";
}

// ---------------------------------------------------------------------------
// 属性查询
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeFileName.cpp — IsAbsolutePath
bool BeFileName::IsAbsolutePath() const {
    if (empty()) return false;
#if defined(_WIN32)
    return size() >= 2 && (*this)[1] == ':';
#else
    return (*this)[0] == '/';
#endif
}

bool BeFileName::IsAbsolutePathStatic(const DqString& path) {
    if (path.empty()) return false;
#if defined(_WIN32)
    return path.size() >= 2 && path[1] == ':';
#else
    return path[0] == '/';
#endif
}

// Ported from: imodel-native BeFileName.cpp — DoesPathExist
bool BeFileName::DoesPathExist() const {
    struct stat st;
    return stat(c_str(), &st) == 0;
}

bool BeFileName::DoesPathExistStatic(const DqString& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

// Ported from: imodel-native BeFileName.cpp — IsDirectory
bool BeFileName::IsDirectory() const {
    struct stat st;
    if (stat(c_str(), &st) != 0) return false;
#if defined(_WIN32)
    return (st.st_mode & _S_IFDIR) != 0;
#else
    return S_ISDIR(st.st_mode);
#endif
}

bool BeFileName::IsDirectoryStatic(const DqString& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
#if defined(_WIN32)
    return (st.st_mode & _S_IFDIR) != 0;
#else
    return S_ISDIR(st.st_mode);
#endif
}

// Ported from: imodel-native BeFileName.cpp — IsSymbolicLink
bool BeFileName::IsSymbolicLink() const {
#if defined(_WIN32)
    return false;  // Windows: simplified
#else
    struct stat st;
    if (lstat(c_str(), &st) != 0) return false;
    return S_ISLNK(st.st_mode);
#endif
}

// Ported from: imodel-native BeFileName.cpp — IsEquivalentTo
bool BeFileName::IsEquivalentTo(const char* rhs) const {
    if (!rhs) return empty();
    BeFileName fixedThis, fixedRhs;
    FixPathName(fixedThis, c_str());
    FixPathName(fixedRhs, rhs);
    return fixedThis == fixedRhs;
}

// ---------------------------------------------------------------------------
// 文件操作
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeFileName.cpp — CreateNewDirectory
BeFileNameStatus BeFileName::CreateNewDirectory(const DqString& path) {
#if defined(_WIN32)
    if (_mkdir(path.c_str()) == 0) return BeFileNameStatus::Success;
#else
    if (mkdir(path.c_str(), 0755) == 0) return BeFileNameStatus::Success;
#endif
    if (errno == EEXIST) return BeFileNameStatus::AlreadyExists;
    return BeFileNameStatus::CantCreate;
}

// Ported from: imodel-native BeFileName.cpp — BeDeleteFile
BeFileNameStatus BeFileName::BeDeleteFile() const {
    return BeDeleteFileStatic(*this);
}

BeFileNameStatus BeFileName::BeDeleteFileStatic(const DqString& path) {
#if defined(_WIN32)
    if (::DeleteFileA(path.c_str())) return BeFileNameStatus::Success;
#else
    if (unlink(path.c_str()) == 0) return BeFileNameStatus::Success;
#endif
    return BeFileNameStatus::CantDeleteFile;
}

// Ported from: imodel-native BeFileName.cpp — BeCopyFile
BeFileNameStatus BeFileName::BeCopyFile(const BeFileName& existingFileName, const BeFileName& newFileName, bool failIfFileExists) {
    if (failIfFileExists && newFileName.DoesPathExist())
        return BeFileNameStatus::AlreadyExists;
#if defined(_WIN32)
    if (CopyFileA(existingFileName.c_str(), newFileName.c_str(), FALSE))
        return BeFileNameStatus::Success;
#else
    // Simple copy: read source, write dest
    FILE* src = fopen(existingFileName.c_str(), "rb");
    if (!src) return BeFileNameStatus::FileNotFound;
    FILE* dst = fopen(newFileName.c_str(), "wb");
    if (!dst) { fclose(src); return BeFileNameStatus::CantCreate; }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0)
        fwrite(buf, 1, n, dst);
    fclose(src);
    fclose(dst);
    return BeFileNameStatus::Success;
#endif
    return BeFileNameStatus::UnknownError;
}

// Ported from: imodel-native BeFileName.cpp — BeMoveFile
BeFileNameStatus BeFileName::BeMoveFile(const BeFileName& oldFileName, const BeFileName& newFileName, int /*numRetries*/) {
#if defined(_WIN32)
    if (MoveFileA(oldFileName.c_str(), newFileName.c_str()))
        return BeFileNameStatus::Success;
#else
    if (rename(oldFileName.c_str(), newFileName.c_str()) == 0)
        return BeFileNameStatus::Success;
#endif
    return BeFileNameStatus::UnknownError;
}

// Ported from: imodel-native BeFileName.cpp — SetFileReadOnly
BeFileNameStatus BeFileName::SetFileReadOnly(bool readOnly) const {
#if defined(_WIN32)
    DWORD attrs = GetFileAttributesA(c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) return BeFileNameStatus::FileNotFound;
    if (readOnly)
        attrs |= FILE_ATTRIBUTE_READONLY;
    else
        attrs &= ~FILE_ATTRIBUTE_READONLY;
    if (SetFileAttributesA(c_str(), attrs))
        return BeFileNameStatus::Success;
#else
    struct stat st;
    if (stat(c_str(), &st) != 0) return BeFileNameStatus::FileNotFound;
    mode_t mode = st.st_mode;
    if (readOnly)
        mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
    else
        mode |= S_IWUSR;
    if (chmod(c_str(), mode) == 0)
        return BeFileNameStatus::Success;
#endif
    return BeFileNameStatus::UnknownError;
}

// Ported from: imodel-native BeFileName.cpp — IsFileReadOnly
bool BeFileName::IsFileReadOnly() const {
#if defined(_WIN32)
    DWORD attrs = GetFileAttributesA(c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_READONLY);
#else
    struct stat st;
    if (stat(c_str(), &st) != 0) return false;
    return (st.st_mode & S_IWUSR) == 0;
#endif
}

// Ported from: imodel-native BeFileName.cpp — GetFileSize
BeFileNameStatus BeFileName::GetFileSize(uint64_t& sz) const {
    struct stat st;
    if (stat(c_str(), &st) != 0) return BeFileNameStatus::FileNotFound;
    sz = static_cast<uint64_t>(st.st_size);
    return BeFileNameStatus::Success;
}

// Ported from: imodel-native BeFileName.cpp — GetFileTime
BeFileNameStatus BeFileName::GetFileTime(time_t* ctime, time_t* atime, time_t* mtime) const {
    struct stat st;
    if (stat(c_str(), &st) != 0) return BeFileNameStatus::FileNotFound;
    if (ctime) *ctime = st.st_ctime;
    if (atime) *atime = st.st_atime;
    if (mtime) *mtime = st.st_mtime;
    return BeFileNameStatus::Success;
}

// Ported from: imodel-native BeFileName.cpp — SetFileTime（ref:1958-1992，Win32 分支）
BeFileNameStatus BeFileName::SetFileTime(const time_t* atime, const time_t* mtime) const {
#if defined(_WIN32)
    // 对齐 ref:1968-1992（窄字符适配：_wsopen_s → _sopen_s）
    FILETIME* pAccessTime = nullptr;
    FILETIME* pModifTime = nullptr;

    FILETIME AccessTime;
    if (nullptr != atime) {
        DqTimeUtilities::ConvertUnixTimeToFiletime(AccessTime, *atime);
        pAccessTime = &AccessTime;
    }

    FILETIME ModifTime;
    if (nullptr != mtime) {
        DqTimeUtilities::ConvertUnixTimeToFiletime(ModifTime, *mtime);
        pModifTime = &ModifTime;
    }

    int fh;
    if (_sopen_s(&fh, c_str(), _O_RDWR | _O_BINARY, _SH_DENYNO, 0) != 0)
        return BeFileNameStatus::FileNotFound;
    ::SetFileTime(reinterpret_cast<HANDLE>(_get_osfhandle(fh)), nullptr, pAccessTime, pModifTime);
    _close(fh);
    return BeFileNameStatus::Success;
#else
    struct utimbuf times;
    struct stat st;
    if (stat(c_str(), &st) != 0) return BeFileNameStatus::FileNotFound;
    times.actime = atime ? *atime : st.st_atime;
    times.modtime = mtime ? *mtime : st.st_mtime;
    if (utime(c_str(), &times) == 0)
        return BeFileNameStatus::Success;
    return BeFileNameStatus::UnknownError;
#endif
}

// Ported from: imodel-native BeFileName.cpp — CheckAccess（ref:2064-2083）
BeFileNameStatus BeFileName::CheckAccess(BeFileNameAccess accs) const {
#if defined(_WIN32)
    // 对齐 ref:2074（窄字符适配：_waccess → _access）
    int i = _access(c_str(), static_cast<int>(accs));
    return (0 == i) ? BeFileNameStatus::Success
                    : (-1 == i) ? BeFileNameStatus::FileNotFound
                                : BeFileNameStatus::AccessViolation;
#else
    int mode = 0;
    if (static_cast<int>(accs) & static_cast<int>(BeFileNameAccess::Read))
        mode |= R_OK;
    if (static_cast<int>(accs) & static_cast<int>(BeFileNameAccess::Write))
        mode |= W_OK;
    if (access(c_str(), mode) == 0)
        return BeFileNameStatus::Success;
    return BeFileNameStatus::AccessViolation;
#endif
}

// ---------------------------------------------------------------------------
// 路径规范化
// ---------------------------------------------------------------------------

// Ported from: imodel-native BeFileName.cpp — FixPathName
BeFileNameStatus BeFileName::FixPathName(DqString& path, const char* original, bool keepTrailingSeparator) {
    if (!original) { path.clear(); return BeFileNameStatus::Success; }
    path = original;

    // Normalize separators
#if defined(_WIN32)
    std::replace(path.begin(), path.end(), '/', '\\');
#else
    std::replace(path.begin(), path.end(), '\\', '/');
#endif

    // Remove double separators
    DqString doubleSep;
    doubleSep += kDirSeparator;
    doubleSep += kDirSeparator;
    size_t pos;
    while ((pos = path.find(doubleSep)) != DqString::npos)
        path.erase(pos, 1);

    // Remove trailing separator unless requested
    if (!keepTrailingSeparator && !path.empty() && path.back() == kDirSeparator)
        path.pop_back();

    return BeFileNameStatus::Success;
}

// Ported from: imodel-native BeFileName.cpp — BeGetFullPathName
BeFileNameStatus BeFileName::BeGetFullPathName() {
    DqString fullPath;
    auto status = BeGetFullPathNameStatic(fullPath, c_str());
    if (status == BeFileNameStatus::Success)
        assign(fullPath);
    return status;
}

BeFileNameStatus BeFileName::BeGetFullPathNameStatic(DqString& path, const char* src) {
    if (!src) return BeFileNameStatus::IllegalName;
#if defined(_WIN32)
    char buf[MAX_PATH];
    if (_fullpath(buf, src, MAX_PATH)) {
        path = buf;
        return BeFileNameStatus::Success;
    }
#else
    char buf[PATH_MAX];
    if (realpath(src, buf)) {
        path = buf;
        return BeFileNameStatus::Success;
    }
#endif
    return BeFileNameStatus::FileNotFound;
}

// Ported from: imodel-native BeFileName.cpp — FindRelativePath
void BeFileName::FindRelativePath(DqString& relativePath, const char* targetFileName, const char* rootFileName) {
    // Simplified: if same directory prefix, strip it
    if (!targetFileName || !rootFileName) {
        relativePath = targetFileName ? targetFileName : "";
        return;
    }
    DqString target(targetFileName);
    DqString root(rootFileName);

    // Find common prefix
    size_t lastSep = 0;
    size_t minLen = std::min(target.size(), root.size());
    for (size_t i = 0; i < minLen; ++i) {
        if (target[i] == root[i]) {
            if (target[i] == kDirSeparator)
                lastSep = i + 1;
        } else {
            break;
        }
    }

    if (lastSep == 0) {
        relativePath = target;
        return;
    }

    // Count remaining separators in root to determine .. count
    DqString remaining = root.substr(lastSep);
    int upCount = 0;
    for (char c : remaining) {
        if (c == kDirSeparator) ++upCount;
    }
    if (!remaining.empty()) ++upCount;

    relativePath.clear();
    for (int i = 0; i < upCount; ++i) {
        relativePath += "..";
        relativePath += kDirSeparator;
    }
    relativePath += target.substr(lastSep);
}

// Ported from: imodel-native BeFileName.cpp — ResolveRelativePath
int BeFileName::ResolveRelativePath(DqString& fullPath, const char* relativeFileName, const char* basePath) {
    if (!relativeFileName || !basePath) return -1;

    BeFileName base(basePath);
    BeFileName rel(relativeFileName);

    if (rel.IsAbsolutePath()) {
        fullPath = relativeFileName;
        return 0;
    }

    base.AppendToPath(rel.c_str());
    fullPath = base;
    return 0;
}

// ---------------------------------------------------------------------------
// 静态工具方法（兼容旧 API）
// ---------------------------------------------------------------------------

DqString BeFileName::CombineStatic(const DqString& dir, const DqString& file) {
    BeFileName result(dir);
    result.AppendToPath(file.c_str());
    return result;
}

DqString BeFileName::AppendSeparatorStatic(const DqString& path) {
    BeFileName result(path);
    result.AppendSeparator();
    return result;
}

END_DQ_BASE_NAMESPACE
