// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/BeSQLite.ts
//              imodel-native iModelCore/BeSQLite/PublicAPI/BeSQLite/BeSQLite.h
// DanQing dqBase — SQLite/BeSQLite 数据库枚举
//
// 1:1 对齐 itwinjs-core BeSQLite.ts 的 OpenMode / DbOpcode / DbResult。
// 仓外数据层依赖这些枚举。
#pragma once

#include "DqBase.h"

#include <cstdint>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// OpenMode — 数据库打开模式
// Ported from: itwinjs-core BeSQLite.ts OpenMode
// ---------------------------------------------------------------------------
enum class OpenMode : uint32_t {
    Readonly  = 0x00000001,
    ReadWrite = 0x00000002,
};

// ---------------------------------------------------------------------------
// DbOpcode — 数据库变更操作码
// Ported from: itwinjs-core BeSQLite.ts DbOpcode
// ---------------------------------------------------------------------------
enum class DbOpcode : int {
    Delete = 9,
    Insert = 18,
    Update = 23,
};

// ---------------------------------------------------------------------------
// DbResult — SQLite 返回码（87 个成员）
// Ported from: itwinjs-core BeSQLite.ts DbResult
// ---------------------------------------------------------------------------
enum class DbResult : int {
    // --- 基础 SQLite 返回码 ---
    BE_SQLITE_OK           = 0,
    BE_SQLITE_ERROR        = 1,
    BE_SQLITE_INTERNAL     = 2,
    BE_SQLITE_PERM         = 3,
    BE_SQLITE_ABORT        = 4,
    BE_SQLITE_BUSY         = 5,
    BE_SQLITE_LOCKED       = 6,
    BE_SQLITE_NOMEM        = 7,
    BE_SQLITE_READONLY     = 8,
    BE_SQLITE_INTERRUPT    = 9,
    BE_SQLITE_IOERR        = 10,
    BE_SQLITE_CORRUPT      = 11,
    BE_SQLITE_NOTFOUND     = 12,
    BE_SQLITE_FULL         = 13,
    BE_SQLITE_CANTOPEN     = 14,
    BE_SQLITE_PROTOCOL     = 15,
    BE_SQLITE_EMPTY        = 16,
    BE_SQLITE_SCHEMA       = 17,
    BE_SQLITE_TOOBIG       = 18,
    BE_SQLITE_CONSTRAINT_BASE = 19,
    BE_SQLITE_MISMATCH     = 20,
    BE_SQLITE_MISUSE       = 21,
    BE_SQLITE_NOLFS        = 22,
    BE_SQLITE_AUTH         = 23,
    BE_SQLITE_FORMAT       = 24,
    BE_SQLITE_RANGE        = 25,
    BE_SQLITE_NOTADB       = 26,
    BE_SQLITE_ROW          = 100,
    BE_SQLITE_DONE         = 101,

    // --- 扩展 I/O 错误码 (BE_SQLITE_IOERR | (N << 8)) ---
    BE_SQLITE_IOERR_READ              = 266,   // 10 | (1 << 8)
    BE_SQLITE_IOERR_SHORT_READ        = 522,   // 10 | (2 << 8)
    BE_SQLITE_IOERR_WRITE             = 778,   // 10 | (3 << 8)
    BE_SQLITE_IOERR_FSYNC             = 1034,  // 10 | (4 << 8)
    BE_SQLITE_IOERR_DIR_FSYNC         = 1290,  // 10 | (5 << 8)
    BE_SQLITE_IOERR_TRUNCATE          = 1546,  // 10 | (6 << 8)
    BE_SQLITE_IOERR_FSTAT             = 1802,  // 10 | (7 << 8)
    BE_SQLITE_IOERR_UNLOCK            = 2058,  // 10 | (8 << 8)
    BE_SQLITE_IOERR_RDLOCK            = 2314,  // 10 | (9 << 8)
    BE_SQLITE_IOERR_DELETE            = 2570,  // 10 | (10 << 8)
    BE_SQLITE_IOERR_BLOCKED           = 2826,  // 10 | (11 << 8)
    BE_SQLITE_IOERR_NOMEM             = 3082,  // 10 | (12 << 8)
    BE_SQLITE_IOERR_ACCESS            = 3338,  // 10 | (13 << 8)
    BE_SQLITE_IOERR_CHECKRESERVEDLOCK = 3594,  // 10 | (14 << 8)
    BE_SQLITE_IOERR_LOCK              = 3850,  // 10 | (15 << 8)
    BE_SQLITE_IOERR_CLOSE             = 4106,  // 10 | (16 << 8)
    BE_SQLITE_IOERR_DIR_CLOSE         = 4362,  // 10 | (17 << 8)
    BE_SQLITE_IOERR_SHMOPEN           = 4618,  // 10 | (18 << 8)
    BE_SQLITE_IOERR_SHMSIZE           = 4874,  // 10 | (19 << 8)
    BE_SQLITE_IOERR_SHMLOCK           = 5130,  // 10 | (20 << 8)
    BE_SQLITE_IOERR_SHMMAP            = 5386,  // 10 | (21 << 8)
    BE_SQLITE_IOERR_SEEK              = 5642,  // 10 | (22 << 8)
    BE_SQLITE_IOERR_DELETE_NOENT      = 5898,  // 10 | (23 << 8)

    // --- Bentley 自定义扩展错误码 (BE_SQLITE_IOERR | (N << 24)) ---
    BE_SQLITE_ERROR_FileExists                     = 0x0100000A, // 10 | (1 << 24)
    BE_SQLITE_ERROR_AlreadyOpen                    = 0x0200000A,
    BE_SQLITE_ERROR_NoPropertyTable                = 0x0300000A,
    BE_SQLITE_ERROR_FileNotFound                   = 0x0400000A,
    BE_SQLITE_ERROR_NoTxnActive                    = 0x0500000A,
    BE_SQLITE_ERROR_BadDbProfile                   = 0x0600000A,
    BE_SQLITE_ERROR_InvalidProfileVersion          = 0x0700000A,
    BE_SQLITE_ERROR_ProfileUpgradeFailed           = 0x0800000A,
    BE_SQLITE_ERROR_ProfileTooOldForReadWrite      = 0x0900000A,
    BE_SQLITE_ERROR_ProfileTooOld                  = 0x0A00000A,
    BE_SQLITE_ERROR_ProfileTooNewForReadWrite      = 0x0B00000A,
    BE_SQLITE_ERROR_ProfileTooNew                  = 0x0C00000A,
    BE_SQLITE_ERROR_ChangeTrackError               = 0x0D00000A,
    BE_SQLITE_ERROR_InvalidChangeSetVersion        = 0x0E00000A,
    BE_SQLITE_ERROR_SchemaUpgradeRequired          = 0x0F00000A,
    BE_SQLITE_ERROR_SchemaTooNew                   = 0x1000000A,
    BE_SQLITE_ERROR_SchemaTooOld                   = 0x1100000A,
    BE_SQLITE_ERROR_SchemaLockFailed               = 0x1200000A,
    BE_SQLITE_ERROR_SchemaUpgradeFailed            = 0x1300000A,
    BE_SQLITE_ERROR_SchemaImportFailed             = 0x1400000A,
    BE_SQLITE_ERROR_CouldNotAcquireLocksOrCodes    = 0x1500000A,
    BE_SQLITE_ERROR_SchemaUpgradeRecommended       = 0x1600000A,
    BE_SQLITE_ERROR_DataTransformRequired          = 0x1700000A,

    // --- Bentley ERROR 扩展码 (BE_SQLITE_ERROR | (N << 24)) ---
    BE_SQLITE_ERROR_NOTOPEN                        = 0x01000001, // 1 | (1 << 24)
    BE_SQLITE_ERROR_PropagateChangesFailed         = 0x02000001,

    // --- 标准 SQLite 扩展码 (base | (N << 8)) ---
    BE_SQLITE_LOCKED_SHAREDCACHE     = 262,   // 6 | (1 << 8)
    BE_SQLITE_BUSY_RECOVERY          = 133,   // 5 | (1 << 8)
    BE_SQLITE_CANTOPEN_NOTEMPDIR     = 3670,  // 14 | (1 << 8)
    BE_SQLITE_CANTOPEN_ISDIR         = 3926,  // 14 | (2 << 8)
    BE_SQLITE_CANTOPEN_FULLPATH      = 4182,  // 14 | (3 << 8)
    BE_SQLITE_CORRUPT_VTAB           = 2827,  // 11 | (1 << 8)
    BE_SQLITE_READONLY_RECOVERY      = 2056,  // 8 | (1 << 8)
    BE_SQLITE_READONLY_CANTLOCK      = 2312,  // 8 | (2 << 8)
    BE_SQLITE_READONLY_ROLLBACK      = 2568,  // 8 | (3 << 8)
    BE_SQLITE_ABORT_ROLLBACK         = 516,   // 4 | (2 << 8)

    // --- CONSTRAINT 扩展码 ---
    BE_SQLITE_CONSTRAINT_CHECK       = 4883,  // 19 | (1 << 8)
    BE_SQLITE_CONSTRAINT_COMMITHOOK  = 5139,  // 19 | (2 << 8)
    BE_SQLITE_CONSTRAINT_FOREIGNKEY  = 5395,  // 19 | (3 << 8)
    BE_SQLITE_CONSTRAINT_FUNCTION    = 5651,  // 19 | (4 << 8)
    BE_SQLITE_CONSTRAINT_NOTNULL     = 5907,  // 19 | (5 << 8)
    BE_SQLITE_CONSTRAINT_PRIMARYKEY  = 6163,  // 19 | (6 << 8)
    BE_SQLITE_CONSTRAINT_TRIGGER     = 6419,  // 19 | (7 << 8)
    BE_SQLITE_CONSTRAINT_UNIQUE      = 6675,  // 19 | (8 << 8)
    BE_SQLITE_CONSTRAINT_VTAB        = 6931,  // 19 | (9 << 8)
};

END_DQ_BASE_NAMESPACE
