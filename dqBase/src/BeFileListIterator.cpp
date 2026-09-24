// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/BeFileListIterator.h
// DanQing dqBase — 文件列表迭代器实现
#include "dqBase/BeFileListIterator.h"

BEGIN_DQ_BASE_NAMESPACE

BeFileListIterator::BeFileListIterator(const DqString& fileList)
    : m_fileList(fileList)
{
}

bool BeFileListIterator::GetNextFileName(DqString& fileName) {
    if (m_pos >= m_fileList.size()) return false;
    size_t end = m_fileList.find(';', m_pos);
    if (end == DqString::npos) end = m_fileList.size();
    fileName = m_fileList.substr(m_pos, end - m_pos);
    m_pos = end + 1;
    return !fileName.empty();
}

END_DQ_BASE_NAMESPACE
