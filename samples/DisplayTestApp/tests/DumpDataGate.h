// SPDX-License-Identifier: Apache-2.0
// P-4（2026-10-23 数据外置）：大模型 dump 数据门禁——数据缺席（git clone
// --depth 1 新检出 / 未跑 scripts/data/download_dumps.ps1 回灌）时 GTEST_SKIP
// 而非 FAIL。判据：
//   requireDumpRootOrSkip  = <imodelRoot>/manifest.json 存在（joeshouse /
//                            housemodel / baytown / bridge-edit / instances60
//                            五族整族外置——P-4 起 HEAD 不再跟踪）。
//   requireDumpFilesOrSkip = <root>/files/ 非空（newbridge 专用——其 manifest
//                            全入 git 而 files/ 本地持有从不入库，manifest
//                            存在性判据对其无效：新检出假红的根治）。
// 调用点一律放 TEST 体首行（GTEST_SKIP 宏含 return）；fixture 型锁门在
// setup() 断言之前。
#pragma once

#include <filesystem>
#include <string>

#include <gtest/gtest.h>

namespace dta {

// Authored: no reference test exists in itwinjs-core for downloadable-asset
// gating（DanQing 数据外置基建，无参考对应物）。
inline void requireDumpRootOrSkip(std::string const& imodelRoot)
{
    std::error_code ec;
    if (!std::filesystem::exists(imodelRoot + "/manifest.json", ec))
        GTEST_SKIP() << "dump data absent: " << imodelRoot
                     << " -- restore via scripts/data/download_dumps.ps1";
}

inline void requireDumpFilesOrSkip(std::string const& root)
{
    std::error_code ec;
    auto it = std::filesystem::directory_iterator(root + "/files", ec);
    if (ec || it == std::filesystem::directory_iterator{})
        GTEST_SKIP() << "dump payload absent: " << root
                     << "/files (locally-held data, never committed)";
}

}  // namespace dta
