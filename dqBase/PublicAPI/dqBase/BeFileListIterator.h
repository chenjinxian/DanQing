// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeFileListIterator.h
// DanQing dqBase — 文件列表迭代器
//
// 1:1 对齐 imodel-native BeFileListIterator。
// 遍历分号分隔的文件路径列表（支持通配符）。
#pragma once

#include "Export.h"
#include "BeFileName.h"
#include "DqTypes.h"

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// BeFileListIterator — 文件列表迭代器
// Ported from: imodel-native BeFileListIterator.h
// ---------------------------------------------------------------------------
class DQ_BASE_EXPORT BeFileListIterator {
public:
    explicit BeFileListIterator(const DqString& fileList);

    /// 获取下一个文件名。返回 false 如果没有更多文件。
    bool GetNextFileName(DqString& fileName);

private:
    DqString m_fileList;
    size_t m_pos = 0;
};

END_DQ_BASE_NAMESPACE
