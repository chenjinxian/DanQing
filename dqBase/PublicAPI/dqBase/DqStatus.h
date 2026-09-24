// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/BentleyError.ts
//              imodel-native iModelCore/Bentley/PublicAPI/Bentley/Bentley.h (DqStatus)
// DanQing dqBase — 状态码与错误枚举
//
// 1:1 对齐 itwinjs-core DqStatus / IModelStatus / BriefcaseStatus / ChangeSetStatus。
// 仓外数据/平台层依赖这些枚举。
#pragma once

#include "DqBase.h"
#include "DqTypes.h"

#include <cstdint>

BEGIN_DQ_BASE_NAMESPACE

// ---------------------------------------------------------------------------
// DqStatus — 基础成功/失败（对齐 imodel-native DqStatus）
// Ported from: itwinjs-core BentleyError.ts DqStatus
// ---------------------------------------------------------------------------
enum class DqStatus : uint16_t {
    Success = 0x0000,
    Error   = 0x8000,
};

// ---------------------------------------------------------------------------
// IModelStatus — iModel 操作状态码（73 个成员）
// Ported from: itwinjs-core BentleyError.ts IModelStatus
// ---------------------------------------------------------------------------
enum class IModelStatus : int {
    Success                 = 0,
    AlreadyLoaded           = 0x10001,  // IMODEL_ERROR_BASE + 1
    AlreadyOpen             = 0x10002,
    BadArg                  = 0x10003,
    BadElement              = 0x10004,
    BadModel                = 0x10005,
    BadRequest              = 0x10006,
    BadSchema               = 0x10007,
    CannotUndo              = 0x10008,
    CodeNotReserved         = 0x10009,
    DeletionProhibited      = 0x1000A,
    DuplicateCode           = 0x1000B,
    DuplicateName           = 0x1000C,
    ElementBlockedChange    = 0x1000D,
    FileAlreadyExists       = 0x1000E,
    FileNotFound            = 0x1000F,
    FileNotLoaded           = 0x10010,
    ForeignKeyConstraint    = 0x10011,
    IdExists                = 0x10012,
    InDynamicTransaction    = 0x10013,
    InvalidCategory         = 0x10014,
    InvalidCode             = 0x10015,
    InvalidCodeSpec         = 0x10016,
    InvalidId               = 0x10017,
    InvalidName             = 0x10018,
    InvalidParent           = 0x10019,
    InvalidProfileVersion   = 0x1001A,
    IsCreatingChangeSet     = 0x1001B,
    LockNotHeld             = 0x1001C,
    Mismatch2d3d            = 0x1001D,
    MismatchGcs             = 0x1001E,
    MissingDomain           = 0x1001F,
    MissingHandler          = 0x10020,
    MissingId               = 0x10021,
    NoGeometry              = 0x10022,
    NoMultiTxnOperation     = 0x10023,
    // +36 is skipped in the reference
    NotEnabled              = 0x10025,
    NotFound                = 0x10026,
    NotOpen                 = 0x10027,
    NotOpenForWrite         = 0x10028,
    NotSameUnitBase         = 0x10029,
    NothingToRedo           = 0x1002A,
    NothingToUndo           = 0x1002B,
    ParentBlockedChange     = 0x1002C,
    ReadError               = 0x1002D,
    ReadOnly                = 0x1002E,
    ReadOnlyDomain          = 0x1002F,
    RepositoryManagerError  = 0x10030,
    SQLiteError             = 0x10031,
    TransactionActive       = 0x10032,
    UnitsMissing            = 0x10033,
    UnknownFormat           = 0x10034,
    UpgradeFailed           = 0x10035,
    ValidationFailed        = 0x10036,
    VersionTooNew           = 0x10037,
    VersionTooOld           = 0x10038,
    ViewNotFound            = 0x10039,
    WriteError              = 0x1003A,
    WrongClass              = 0x1003B,
    WrongIModel             = 0x1003C,
    WrongDomain             = 0x1003D,
    WrongElement            = 0x1003E,
    WrongHandler            = 0x1003F,
    WrongModel              = 0x10040,
    ConstraintNotUnique     = 0x10041,
    NoGeoLocation           = 0x10042,
    ServerTimeout           = 0x10043,
    NoContent               = 0x10044,
    NotRegistered           = 0x10045,
    FunctionNotFound        = 0x10046,
    NoActiveCommand         = 0x10047,
    Aborted                 = 0x10048,
};

// ---------------------------------------------------------------------------
// BriefcaseStatus — Briefcase 操作状态码
// Ported from: itwinjs-core BentleyError.ts BriefcaseStatus
// ---------------------------------------------------------------------------
enum class BriefcaseStatus : int {
    CannotAcquire             = 0x20000,
    CannotDownload            = 0x20001,
    CannotUpload              = 0x20002,
    CannotCopy                = 0x20003,
    CannotDelete              = 0x20004,
    VersionNotFound           = 0x20005,
    CannotApplyChanges        = 0x20006,
    DownloadCancelled         = 0x20007,
    ContainsDeletedChangeSets = 0x20008,
};

// ---------------------------------------------------------------------------
// ChangeSetStatus — ChangeSet 操作状态码
// Ported from: itwinjs-core BentleyError.ts ChangeSetStatus
// ---------------------------------------------------------------------------
enum class ChangeSetStatus : int {
    Success                          = 0,
    ApplyError                       = 0x16001,
    ChangeTrackingNotEnabled         = 0x16002,
    CorruptedChangeStream            = 0x16003,
    FileNotFound                     = 0x16004,
    FileWriteError                   = 0x16005,
    HasLocalChanges                  = 0x16006,
    HasUncommittedChanges            = 0x16007,
    InvalidId                        = 0x16008,
    InvalidVersion                   = 0x16009,
    InDynamicTransaction             = 0x1600A,
    IsCreatingChangeSet              = 0x1600B,
    IsNotCreatingChangeSet           = 0x1600C,
    MergePropagationError            = 0x1600D,
    NothingToMerge                   = 0x1600E,
    NoTransactions                   = 0x1600F,
    ParentMismatch                   = 0x16010,
    SQLiteError                      = 0x16011,
    WrongDgnDb                       = 0x16012,
    CouldNotOpenDgnDb                = 0x16013,
    MergeSchemaChangesOnOpen         = 0x16014,
    ReverseOrReinstateSchemaChanges  = 0x16015,
    ProcessSchemaChangesOnOpen       = 0x16016,
    CannotMergeIntoReadonly          = 0x16017,
    CannotMergeIntoMaster            = 0x16018,
    CannotMergeIntoReversed          = 0x16019,
    DownloadCancelled                = 0x1601A,
};

// ---------------------------------------------------------------------------
// DqError — 通用错误结构（保持原有接口）
// ---------------------------------------------------------------------------
struct DqError {
    int code = 0;         // 模块自定义错误码（0 = 成功）
    DqString message;     // 人类可读描述
    DqString detail;      // 可选诊断信息
};

END_DQ_BASE_NAMESPACE
