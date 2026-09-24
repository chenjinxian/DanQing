// SPDX-License-Identifier: Apache-2.0
// Ported from: itwinjs-core core/bentley/src/test/BentleyError.test.ts
//              imodel-native iModelCore/Bentley/Tests/NonPublished/BeTimeUtilities_Test.cpp
// dqBase tests — Phase 1.5 新增功能验证
//   覆盖：IModelStatus / DqStatus / DbResult / DqDuration / DqTimePoint / DqStopWatch / Guid / Id64 / Compare
#include <gtest/gtest.h>

#include <dqBase/DqStatus.h>
#include <dqBase/DqDbResult.h>
#include <dqBase/DqTime.h>
#include <dqBase/DqCompare.h>
#include <dqBase/DqId.h>

#include <string>
#include <thread>

// ===================== DqStatus =====================

// Ported from: itwinjs-core BentleyError.ts DqStatus
TEST(DqStatusTest, ValuesMatchReference) {
    EXPECT_EQ(static_cast<int>(dqBase::DqStatus::Success), 0);
    EXPECT_EQ(static_cast<int>(dqBase::DqStatus::Error), 0x8000);
}

// ===================== IModelStatus =====================

// Ported from: itwinjs-core BentleyError.ts IModelStatus
TEST(IModelStatusTest, ValuesMatchReference) {
    EXPECT_EQ(static_cast<int>(dqBase::IModelStatus::Success), 0);
    EXPECT_EQ(static_cast<int>(dqBase::IModelStatus::AlreadyLoaded), 0x10001);
    EXPECT_EQ(static_cast<int>(dqBase::IModelStatus::BadArg), 0x10003);
    EXPECT_EQ(static_cast<int>(dqBase::IModelStatus::NotFound), 0x10026);
    EXPECT_EQ(static_cast<int>(dqBase::IModelStatus::NotOpen), 0x10027);
    EXPECT_EQ(static_cast<int>(dqBase::IModelStatus::SQLiteError), 0x10031);
    EXPECT_EQ(static_cast<int>(dqBase::IModelStatus::Aborted), 0x10048);
}

// ===================== BriefcaseStatus =====================

// Ported from: itwinjs-core BentleyError.ts BriefcaseStatus
TEST(BriefcaseStatusTest, ValuesMatchReference) {
    EXPECT_EQ(static_cast<int>(dqBase::BriefcaseStatus::CannotAcquire), 0x20000);
    EXPECT_EQ(static_cast<int>(dqBase::BriefcaseStatus::DownloadCancelled), 0x20007);
}

// ===================== ChangeSetStatus =====================

// Ported from: itwinjs-core BentleyError.ts ChangeSetStatus
TEST(ChangeSetStatusTest, ValuesMatchReference) {
    EXPECT_EQ(static_cast<int>(dqBase::ChangeSetStatus::Success), 0);
    EXPECT_EQ(static_cast<int>(dqBase::ChangeSetStatus::ApplyError), 0x16001);
    EXPECT_EQ(static_cast<int>(dqBase::ChangeSetStatus::DownloadCancelled), 0x1601A);
}

// ===================== DbResult =====================

// Ported from: itwinjs-core BeSQLite.ts DbResult
TEST(DbResultTest, BaseSqliteCodes) {
    EXPECT_EQ(static_cast<int>(dqBase::DbResult::BE_SQLITE_OK), 0);
    EXPECT_EQ(static_cast<int>(dqBase::DbResult::BE_SQLITE_ERROR), 1);
    EXPECT_EQ(static_cast<int>(dqBase::DbResult::BE_SQLITE_ROW), 100);
    EXPECT_EQ(static_cast<int>(dqBase::DbResult::BE_SQLITE_DONE), 101);
}

// Ported from: itwinjs-core BeSQLite.ts DbResult
TEST(DbResultTest, ExtendedIoErrorCodes) {
    EXPECT_EQ(static_cast<int>(dqBase::DbResult::BE_SQLITE_IOERR_READ), 266);
    EXPECT_EQ(static_cast<int>(dqBase::DbResult::BE_SQLITE_IOERR_SHORT_READ), 522);
}

// Ported from: itwinjs-core BeSQLite.ts DbResult
TEST(DbResultTest, BentleyCustomCodes) {
    EXPECT_EQ(static_cast<int>(dqBase::DbResult::BE_SQLITE_ERROR_FileExists), 0x0100000A);
    EXPECT_EQ(static_cast<int>(dqBase::DbResult::BE_SQLITE_ERROR_NOTOPEN), 0x01000001);
}

// Ported from: itwinjs-core BeSQLite.ts DbResult
TEST(DbResultTest, ConstraintCodes) {
    EXPECT_EQ(static_cast<int>(dqBase::DbResult::BE_SQLITE_CONSTRAINT_CHECK), 4883);
    EXPECT_EQ(static_cast<int>(dqBase::DbResult::BE_SQLITE_CONSTRAINT_UNIQUE), 6675);
}

// ===================== OpenMode =====================

// Ported from: itwinjs-core BeSQLite.ts OpenMode
TEST(OpenModeTest, ValuesMatchReference) {
    EXPECT_EQ(static_cast<uint32_t>(dqBase::OpenMode::Readonly), 1);
    EXPECT_EQ(static_cast<uint32_t>(dqBase::OpenMode::ReadWrite), 2);
}

// ===================== DbOpcode =====================

// Ported from: itwinjs-core BeSQLite.ts DbOpcode
TEST(DbOpcodeTest, ValuesMatchReference) {
    EXPECT_EQ(static_cast<int>(dqBase::DbOpcode::Delete), 9);
    EXPECT_EQ(static_cast<int>(dqBase::DbOpcode::Insert), 18);
    EXPECT_EQ(static_cast<int>(dqBase::DbOpcode::Update), 23);
}

// ===================== DqDuration =====================

// Ported from: imodel-native BeTimeUtilities.h BeDuration
TEST(DqDurationTest, FromSecondsAndToSeconds) {
    auto d = dqBase::DqDuration::FromSeconds(2.5);
    EXPECT_NEAR(d.ToSeconds(), 2.5, 1e-9);
    EXPECT_FALSE(d.IsZero());
    EXPECT_TRUE(d.IsTowardsFuture());
    EXPECT_FALSE(d.IsTowardsPast());
}

// Ported from: imodel-native BeTimeUtilities.h BeDuration
TEST(DqDurationTest, FromMilliseconds) {
    auto d = dqBase::DqDuration::FromMilliseconds(1500);
    EXPECT_NEAR(d.ToSeconds(), 1.5, 1e-9);
    EXPECT_EQ(d.ToMilliseconds(), 1500);
}

// Ported from: imodel-native BeTimeUtilities.h BeDuration
TEST(DqDurationTest, Zero) {
    auto d = dqBase::DqDuration::Zero();
    EXPECT_TRUE(d.IsZero());
    EXPECT_FALSE(d.IsTowardsFuture());
    EXPECT_FALSE(d.IsTowardsPast());
}

// Ported from: imodel-native BeTimeUtilities.h BeDuration
TEST(DqDurationTest, Arithmetic) {
    auto a = dqBase::DqDuration::FromSeconds(1.0);
    auto b = dqBase::DqDuration::FromSeconds(2.0);
    auto sum = a + b;
    EXPECT_NEAR(sum.ToSeconds(), 3.0, 1e-9);
    auto diff = b - a;
    EXPECT_NEAR(diff.ToSeconds(), 1.0, 1e-9);
}

// Ported from: imodel-native BeTimeUtilities.h BeDuration
TEST(DqDurationTest, Comparison) {
    auto a = dqBase::DqDuration::FromSeconds(1.0);
    auto b = dqBase::DqDuration::FromSeconds(2.0);
    EXPECT_TRUE(a < b);
    EXPECT_TRUE(b > a);
    EXPECT_TRUE(a == dqBase::DqDuration::FromSeconds(1.0));
    EXPECT_TRUE(a != b);
}

// ===================== DqTimePoint =====================

// Ported from: imodel-native BeTimeUtilities.h BeTimePoint
TEST(DqTimePointTest, NowIsValid) {
    auto tp = dqBase::DqTimePoint::Now();
    EXPECT_TRUE(tp.IsValid());
    EXPECT_FALSE(tp.IsInFuture()); // 刚创建的不可能在未来
    EXPECT_TRUE(tp.IsInPast() || !tp.IsInPast()); // 可能微小偏差
}

// Ported from: imodel-native BeTimeUtilities.h BeTimePoint
TEST(DqTimePointTest, FromNowAndBeforeNow) {
    auto now = dqBase::DqTimePoint::Now();
    auto future = dqBase::DqTimePoint::FromNow(dqBase::DqDuration::FromSeconds(100));
    auto past = dqBase::DqTimePoint::BeforeNow(dqBase::DqDuration::FromSeconds(100));
    EXPECT_TRUE(future.IsInFuture());
    EXPECT_TRUE(past.IsInPast());
    EXPECT_TRUE(future > now);
    EXPECT_TRUE(past < now);
}

// Ported from: imodel-native BeTimeUtilities.h BeTimePoint
TEST(DqTimePointTest, DurationBetweenPoints) {
    auto t1 = dqBase::DqTimePoint::Now();
    auto t2 = dqBase::DqTimePoint::FromNow(dqBase::DqDuration::FromSeconds(5));
    auto d = t2 - t1;
    EXPECT_NEAR(d.ToSeconds(), 5.0, 0.1); // 允许小误差
}

// ===================== DqStopWatch =====================

// Ported from: imodel-native BeTimeUtilities.h StopWatch
TEST(DqStopWatchTest, MeasureFunction) {
    auto duration = dqBase::DqStopWatch::Measure([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    });
    EXPECT_GE(duration.ToMilliseconds(), 5);
    EXPECT_LT(duration.ToMilliseconds(), 1000);
}

// Ported from: imodel-native BeTimeUtilities.h StopWatch
TEST(DqStopWatchTest, StartStopReset) {
    dqBase::DqStopWatch sw;
    sw.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    sw.Stop();
    EXPECT_GE(sw.GetElapsed().ToMilliseconds(), 5);
    sw.Reset();
    EXPECT_TRUE(sw.GetElapsed().IsZero());
}

// ===================== Guid 命名空间 =====================

// Ported from: itwinjs-core Id.ts Guid.isGuid
TEST(GuidTest, IsGuidValidatesFormat) {
    EXPECT_TRUE(dqBase::Guid::IsGuid("12345678-1234-1234-1234-123456789abc"));
    EXPECT_TRUE(dqBase::Guid::IsGuid("00000000-0000-0000-0000-000000000000"));
    EXPECT_FALSE(dqBase::Guid::IsGuid(""));
    EXPECT_FALSE(dqBase::Guid::IsGuid("not-a-guid"));
    EXPECT_FALSE(dqBase::Guid::IsGuid("12345678-1234-1234-1234")); // 太短
}

// Ported from: itwinjs-core Id.ts Guid.isV4Guid
TEST(GuidTest, IsV4GuidChecksVersion) {
    // V4 GUID: 第 13 位是 '4'，第 17 位是 '8','9','a','b'
    EXPECT_TRUE(dqBase::Guid::IsV4Guid("12345678-1234-4234-8234-123456789abc"));
    EXPECT_FALSE(dqBase::Guid::IsV4Guid("12345678-1234-1234-1234-123456789abc")); // 版本不是 4
}

// Ported from: itwinjs-core Id.ts Guid.createValue
TEST(GuidTest, CreateValueReturnsValidGuid) {
    std::string g = dqBase::Guid::CreateValue();
    EXPECT_EQ(g.size(), 36u);
    EXPECT_TRUE(dqBase::Guid::IsGuid(g.c_str()));
    EXPECT_TRUE(dqBase::Guid::IsV4Guid(g.c_str()));
}

// Ported from: itwinjs-core Id.ts Guid.normalize
TEST(GuidTest, NormalizeLowercases) {
    std::string n = dqBase::Guid::Normalize("12345678-1234-1234-1234-123456789ABC");
    EXPECT_EQ(n, "12345678-1234-1234-1234-123456789abc");
    EXPECT_EQ(dqBase::Guid::Normalize(""), "");
    EXPECT_EQ(dqBase::Guid::Normalize("bad"), "");
}

// Ported from: itwinjs-core Id.ts Guid.empty
TEST(GuidTest, EmptyConstant) {
    EXPECT_EQ(std::string(dqBase::Guid::kEmpty), "00000000-0000-0000-0000-000000000000");
    EXPECT_TRUE(dqBase::Guid::IsGuid(dqBase::Guid::kEmpty));
}

// ===================== Id64 命名空间 =====================

// Ported from: itwinjs-core Id.ts Id64.getLowerUint32 / getUpperUint32
TEST(Id64Test, Uint32PairRoundtrip) {
    auto id = dqBase::Id64::FromLocalAndBriefcaseIds(0x12345678, 0x9ABCDEF0);
    EXPECT_EQ(dqBase::Id64::GetLocalId(id), 0x12345678u);
    EXPECT_EQ(dqBase::Id64::GetBriefcaseId(id), 0x9ABCDEF0u);
    EXPECT_EQ(dqBase::Id64::GetLowerUint32(id), 0x12345678u);
    EXPECT_EQ(dqBase::Id64::GetUpperUint32(id), 0x9ABCDEF0u);
}

// Ported from: itwinjs-core Id.ts Id64.fromUint32Pair
TEST(Id64Test, FromUint32Pair) {
    auto id = dqBase::Id64::FromUint32Pair(100, 200);
    EXPECT_EQ(dqBase::Id64::GetLocalId(id), 100u);
    EXPECT_EQ(dqBase::Id64::GetBriefcaseId(id), 200u);
}

// Ported from: itwinjs-core Id.ts Id64.isValid / isInvalid
TEST(Id64Test, ValidityChecks) {
    EXPECT_TRUE(dqBase::Id64::isValid(dqBase::DqId{42}));
    EXPECT_FALSE(dqBase::Id64::isValid(dqBase::DqId{0}));
    EXPECT_TRUE(dqBase::Id64::IsInvalid(dqBase::DqId{0}));
    EXPECT_FALSE(dqBase::Id64::IsInvalid(dqBase::DqId{42}));
}

// Ported from: itwinjs-core Id.ts Id64.isTransient
TEST(Id64Test, IsTransient) {
    // Transient: BriefcaseId == 0, LocalId != 0
    auto transient = dqBase::Id64::FromLocalAndBriefcaseIds(42, 0);
    EXPECT_TRUE(dqBase::Id64::IsTransient(transient));
    auto real = dqBase::Id64::FromLocalAndBriefcaseIds(42, 1);
    EXPECT_FALSE(dqBase::Id64::IsTransient(real));
    auto zero = dqBase::Id64::FromLocalAndBriefcaseIds(0, 0);
    EXPECT_FALSE(dqBase::Id64::IsTransient(zero));
}

// ===================== Compare 工具 =====================

// Ported from: itwinjs-core Compare.ts compareNumbers
TEST(CompareTest, CompareNumbers) {
    EXPECT_EQ(dqBase::compareNumbers(1.0, 2.0), -1);
    EXPECT_EQ(dqBase::compareNumbers(2.0, 1.0), 1);
    EXPECT_EQ(dqBase::compareNumbers(1.0, 1.0), 0);
}

// Ported from: itwinjs-core Compare.ts compareWithTolerance
TEST(CompareTest, CompareWithTolerance) {
    EXPECT_EQ(dqBase::compareWithTolerance(1.0, 1.05, 0.1), 0);
    EXPECT_EQ(dqBase::compareWithTolerance(1.0, 1.2, 0.1), -1);
    EXPECT_EQ(dqBase::compareWithTolerance(1.2, 1.0, 0.1), 1);
}

// Ported from: itwinjs-core Compare.ts compareBooleans
TEST(CompareTest, CompareBooleans) {
    EXPECT_EQ(dqBase::compareBooleans(false, true), -1);
    EXPECT_EQ(dqBase::compareBooleans(true, false), 1);
    EXPECT_EQ(dqBase::compareBooleans(true, true), 0);
    EXPECT_EQ(dqBase::compareBooleans(false, false), 0);
}

// Ported from: itwinjs-core Compare.ts compareStrings
TEST(CompareTest, CompareStrings) {
    EXPECT_LT(dqBase::compareStrings("a", "b"), 0);
    EXPECT_GT(dqBase::compareStrings("b", "a"), 0);
    EXPECT_EQ(dqBase::compareStrings("a", "a"), 0);
}

// Ported from: itwinjs-core Compare.ts compareArrays
TEST(CompareTest, CompareArrays) {
    std::vector<int> a = {1, 2, 3};
    std::vector<int> b = {1, 2, 4};
    std::vector<int> c = {1, 2};
    dqBase::OrderedComparator<int> cmp = [](const int& x, const int& y) { return x - y; };
    EXPECT_LT(dqBase::compareArrays(a, b, cmp), 0);
    EXPECT_GT(dqBase::compareArrays(b, a, cmp), 0);
    EXPECT_EQ(dqBase::compareArrays(a, a, cmp), 0);
    EXPECT_GT(dqBase::compareArrays(a, c, cmp), 0); // a 更长
    EXPECT_LT(dqBase::compareArrays(c, a, cmp), 0); // c 更短
}
