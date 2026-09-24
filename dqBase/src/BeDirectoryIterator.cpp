// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeDirectoryIterator.h
// DanQing dqBase — 目录迭代器实现
#include "dqBase/BeDirectoryIterator.h"

#include <cstring>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <dirent.h>
    #include <sys/stat.h>
#endif

BEGIN_DQ_BASE_NAMESPACE

#if defined(_WIN32)
// Windows 迭代状态（FindFirstFileA 已产出首条目，需暂存；置于 .cpp 以免公开头引 windows.h）
namespace {
struct WinIterState {
    HANDLE handle;
    WIN32_FIND_DATAA data;
    bool pendingFirst;
};
} // namespace
#endif

BeDirectoryIterator::~BeDirectoryIterator() {
    Close();
}

bool BeDirectoryIterator::Open(const DqString& dirPath) {
    Close();
    m_dirPath = dirPath;
#if defined(_WIN32)
    // 对齐 POSIX 分支语义：目录不存在/不可读 → false。
    // pattern = dir + 分隔符 + '*'（FindFirstFileA 惯例，两种分隔符皆可）。
    DqString pattern = dirPath;
    if (!pattern.empty() && pattern.back() != '\\' && pattern.back() != '/')
        pattern += '\\';
    pattern += '*';
    auto* state = new WinIterState;
    state->handle = FindFirstFileA(pattern.c_str(), &state->data);
    if (state->handle == INVALID_HANDLE_VALUE) {
        delete state;
        return false;
    }
    state->pendingFirst = true;
    m_dirHandle = state;
    return true;
#else
    m_dirHandle = opendir(dirPath.c_str());
    return m_dirHandle != nullptr;
#endif
}

bool BeDirectoryIterator::GetNextEntry(DqString& name, bool& isDir, bool& isFile) {
#if defined(_WIN32)
    if (!m_dirHandle) return false;
    auto* state = static_cast<WinIterState*>(m_dirHandle);
    for (;;) {
        if (state->pendingFirst) {
            state->pendingFirst = false;
        } else {
            if (!FindNextFileA(state->handle, &state->data))
                return false;
        }
        if (std::strcmp(state->data.cFileName, ".") != 0
            && std::strcmp(state->data.cFileName, "..") != 0)
            break;
    }
    name = state->data.cFileName;
    isDir = (state->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    isFile = !isDir; // 对齐 POSIX 分支 fallback 语义（非目录即按文件计）
    return true;
#else
    if (!m_dirHandle) return false;
    auto* dir = static_cast<DIR*>(m_dirHandle);
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0)
            continue;
        name = entry->d_name;
        struct stat st;
        DqString fullPath = m_dirPath + "/" + name;
        if (stat(fullPath.c_str(), &st) == 0) {
            isDir = S_ISDIR(st.st_mode);
            isFile = S_ISREG(st.st_mode);
        } else {
            isDir = false;
            isFile = true;
        }
        return true;
    }
    return false;
#endif
}

void BeDirectoryIterator::Close() {
#if defined(_WIN32)
    if (m_dirHandle) {
        auto* state = static_cast<WinIterState*>(m_dirHandle);
        if (state->handle != INVALID_HANDLE_VALUE)
            FindClose(state->handle);
        delete state;
        m_dirHandle = nullptr;
    }
#else
    if (m_dirHandle) {
        closedir(static_cast<DIR*>(m_dirHandle));
        m_dirHandle = nullptr;
    }
#endif
}

void BeDirectoryIterator::WalkDirsAndMatch(DqVector<DqString>& results,
                                            const DqString& rootDir,
                                            const DqString& pattern,
                                            bool recurse) {
    BeDirectoryIterator iter;
    if (!iter.Open(rootDir)) return;
    DqString name;
    bool isDir, isFile;
    while (iter.GetNextEntry(name, isDir, isFile)) {
        DqString fullPath = rootDir + "/" + name;
        if (isFile) {
            // 简单的通配符匹配：只支持 "*"
            if (pattern == "*" || pattern.empty())
                results.push_back(fullPath);
        } else if (isDir && recurse) {
            WalkDirsAndMatch(results, fullPath, pattern, true);
        }
    }
}

END_DQ_BASE_NAMESPACE
