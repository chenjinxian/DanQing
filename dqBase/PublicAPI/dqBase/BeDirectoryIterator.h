// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeDirectoryIterator.h
// DanQing dqBase — 目录迭代器
//
// 1:1 对齐 imodel-native BeDirectoryIterator。
#pragma once

#include "Export.h"
#include "BeFileName.h"
#include "DqTypes.h"

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// BeDirectoryIterator — 目录遍历器
// Ported from: imodel-native BeDirectoryIterator.h
// ---------------------------------------------------------------------------
class DQ_BASE_EXPORT BeDirectoryIterator {
public:
    BeDirectoryIterator() = default;
    ~BeDirectoryIterator();

    /// 打开目录
    bool Open(const DqString& dirPath);

    /// 获取下一个条目
    bool GetNextEntry(DqString& name, bool& isDir, bool& isFile);

    /// 关闭目录
    void Close();

    /// 静态工具：递归遍历目录收集文件
    static void WalkDirsAndMatch(DqVector<DqString>& results,
                                  const DqString& rootDir,
                                  const DqString& pattern,
                                  bool recurse = true);

private:
    void* m_dirHandle = nullptr;
    DqString m_dirPath;
};

END_DQ_BASE_NAMESPACE
