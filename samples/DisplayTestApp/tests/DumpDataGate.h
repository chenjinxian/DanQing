// SPDX-License-Identifier: Apache-2.0
// P-4（2026-10-23 数据外置）：大模型 dump 数据门禁——数据缺席（git clone
// --depth 1 新检出 / 未跑 scripts/data/download_dumps.ps1 回灌）时 GTEST_SKIP
// 而非 FAIL。判据：
//   DTA_REQUIRE_DUMP_ROOT  = <imodelRoot>/manifest.json 存在（joeshouse /
//                            housemodel / baytown / bridge-edit / instances60
//                            五族整族外置——P-4 起 HEAD 不再跟踪）。
//   DTA_REQUIRE_DUMP_FILES = <root>/files/ 非空（newbridge 专用——其 manifest
//                            全入 git 而 files/ 本地持有从不入库，manifest
//                            存在性判据对其无效：新检出假红的根治）。
// **宏形（勿改回 inline 函数）**：GTEST_SKIP() 展开含 `return`——在 inline
// 辅助函数里只会退出辅助函数，TEST 体继续跑（2026-10-23 无数据模拟实录：
// 门禁打印 "Skipped" 后测试照样 FAIL）。宏在 TEST 体内展开，return 才退出
// 测试。调用点一律放 TEST 体首行。
#pragma once

#include <filesystem>
#include <string>

#include <gtest/gtest.h>

// Authored: no reference test exists in itwinjs-core for downloadable-asset
// gating（DanQing 数据外置基建，无参考对应物）。
#define DTA_REQUIRE_DUMP_ROOT(imodelRoot)                           \
    do {                                                            \
        std::error_code dtaEc;                                      \
        if (!std::filesystem::exists(std::string(imodelRoot) +      \
                                         "/manifest.json",          \
                                     dtaEc))                        \
            GTEST_SKIP() << "dump data absent: " << (imodelRoot)    \
                         << " -- restore via "                      \
                            "scripts/data/download_dumps.ps1";      \
    } while (0)

#define DTA_REQUIRE_DUMP_FILES(root)                                    \
    do {                                                                \
        std::error_code dtaEc;                                          \
        auto dtaIt = std::filesystem::directory_iterator(               \
            std::string(root) + "/files", dtaEc);                       \
        if (dtaEc || dtaIt == std::filesystem::directory_iterator{})    \
            GTEST_SKIP() << "dump payload absent: " << (root)           \
                         << "/files (locally-held data, never "         \
                            "committed)";                               \
    } while (0)
