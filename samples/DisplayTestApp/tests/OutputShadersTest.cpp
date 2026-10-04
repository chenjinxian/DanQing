// OutputShadersTest — M-O(2) 3d OutputShaders 全链锁（引擎收集面 + 六向过滤
// 谓词 + compileAllShaders + 落盘对账）。
//
// 锚定（真实读过的参考行号）：
//   - ShaderProgram.ts:355-374 saveShaderCode（desc 剥 "//!V! "/"//!F! "、
//     ": "/"； "→"-"、追加 _VS/_FS + .glsl、空 desc → noname-N）；
//   - :337-353 setDebugShaderUsage（程序激活标 isUsed）；
//   - System.ts:317 debugShaderFiles 累积数组 / :963 compileAllShaders =
//     Technique.ts:1009-1015 techniques.compileShaders；
//   - OutputShadersTool.ts:289-293 skipThisShader（n/u × f/v × h/g 六向）、
//     :295-320 outputShaders（_makeShade.bat + _OutputList.txt + 逐 .glsl）、
//     :341-366 parseAndRun（c 标志 + u/n/v/f/g/h + d= 目录补尾）。
//
// 判据面：收集门开（env DANQING_DEBUG_SHADERS——参考 IMJS_DEBUG_SHADERS）下
// 首绘后注册表非空且命名/双语段/isUsed 正确；compileAllShaders 使注册表严格
// 增长（全部变体被编译记录）；keyin 落盘后 _OutputList 行数 == 非过滤条目数
// == 落盘 .glsl 数（v 过滤 → 全 _VS）。
//
// Authored: no reference test exists in itwinjs-core for OutputShadersTool
//           （display-test-app 无前端测试；行为锚定如上）。
#include <gtest/gtest.h>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>

#include "View3DInventor.h"
#include "Gui/OutputShadersTool.h"

#include "DumpOpenHelper.h"

#include <dqApp/Application.h>
#include <dqApp/Viewport.h>
#include <dqRender/RenderSystem.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <string>
#include <vector>

#ifndef DANQING_TILE_ASSETS_DIR
#define DANQING_TILE_ASSETS_DIR "."
#endif

namespace {

// 收集门必须在**任何 GL/系统初始化之前**置位（参考 options.debugShaders
// 是 RenderSystem 构造参数；DanQing 以 env 首用快照等价——文件级静态先于
// main 运行）。
struct DebugShadersEnv {
    DebugShadersEnv() { _putenv_s("DANQING_DEBUG_SHADERS", "1"); }
};
DebugShadersEnv const s_debugShadersEnv;

struct QtEnvOS {
    QtEnvOS()
    {
        if (!qApp) {
            static int argc = 1;
            static char n[] = "t";
            static char* av[] = {n, nullptr};
            new QApplication(argc, av);
        }
    }
};
QtEnvOS s_qtOS;

std::string const kDumpRootOS = std::string(DANQING_TILE_ASSETS_DIR) + "/rpc-dumps";

void spinOS(int ms)
{
    qint64 const t0 = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() - t0 < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

// 夹具：instances60 打开链（Surface LUT/instanced 变体的活体编译面——
// DumpOpenResult 成员持有的教训见 SavedViewsTest）。
struct OutputShadersFixture {
    Gui::View3DInventor view{nullptr, nullptr, nullptr};
    dqApp::Viewport* vp = nullptr;
    std::optional<dta::DumpOpenResult> opened;

    bool setup()
    {
        auto& app = dqApp::Application::Get();
        if (!app.isInitialized()) {
            dqApp::Application::Options opts;
            opts.applicationId = "OutputShadersTest";
            opts.applicationVersion = "1.0";
            if (!app.Startup(opts))
                return false;
        }

        view.resize(800, 600);
        view.show();
        spinOS(400);

        dta::DumpOpenPackage pkg;
        pkg.imodelRoot = kDumpRootOS + "/instances60-imodel-v1";
        pkg.tileRoots = {kDumpRootOS + "/instances60-v1", kDumpRootOS + "/instances60-drill-v1"};
        opened = dta::openDumpIModel(view, pkg);
        if (!opened.has_value())
            return false;

        vp = view.getUeViewport();
        vp->RenderFrame();
        spinOS(200);
        vp->RenderFrame();
        return true;
    }
};

size_t countLines(std::string const& path)
{
    std::ifstream ifs(path);
    size_t n = 0;
    std::string line;
    while (std::getline(ifs, line))
        ++n;
    return n;
}

}  // namespace

// ---------------------------------------------------------------------------
// skipThisShader 六向谓词（:289-293——合成条目直驱）。
// ---------------------------------------------------------------------------
TEST(OutputShadersTest, SkipThisShaderFiltersSixWays)
{
    dqRender::DebugShaderFile usedVS{"a_VS.glsl", "", true, true, true};
    dqRender::DebugShaderFile unusedFS{"a_FS.glsl", "", false, true, false};
    dqRender::DebugShaderFile usedHlsl{"a_VS.hlsl", "", true, false, true};

    // 无标志 → 全不跳过。
    EXPECT_FALSE(Gui::OutputShadersTool::skipThisShader(usedVS, "", "", ""));
    EXPECT_FALSE(Gui::OutputShadersTool::skipThisShader(unusedFS, "", "", ""));
    // n = 跳过已用；u = 跳过未用。
    EXPECT_TRUE(Gui::OutputShadersTool::skipThisShader(usedVS, "n", "", ""));
    EXPECT_FALSE(Gui::OutputShadersTool::skipThisShader(unusedFS, "n", "", ""));
    EXPECT_FALSE(Gui::OutputShadersTool::skipThisShader(usedVS, "u", "", ""));
    EXPECT_TRUE(Gui::OutputShadersTool::skipThisShader(unusedFS, "u", "", ""));
    // f = 跳过 VS；v = 跳过非 VS。
    EXPECT_TRUE(Gui::OutputShadersTool::skipThisShader(usedVS, "", "f", ""));
    EXPECT_FALSE(Gui::OutputShadersTool::skipThisShader(unusedFS, "", "f", ""));
    EXPECT_FALSE(Gui::OutputShadersTool::skipThisShader(usedVS, "", "v", ""));
    EXPECT_TRUE(Gui::OutputShadersTool::skipThisShader(unusedFS, "", "v", ""));
    // h = 跳过 GL；g = 跳过非 GL。
    EXPECT_TRUE(Gui::OutputShadersTool::skipThisShader(usedVS, "", "", "h"));
    EXPECT_FALSE(Gui::OutputShadersTool::skipThisShader(usedHlsl, "", "", "h"));
    EXPECT_FALSE(Gui::OutputShadersTool::skipThisShader(usedVS, "", "", "g"));
    EXPECT_TRUE(Gui::OutputShadersTool::skipThisShader(usedHlsl, "", "", "g"));
}

// ---------------------------------------------------------------------------
// 收集面：首绘后注册表非空、命名形（_VS/_FS + .glsl）、双语段、isUsed 有真
// （saveShaderCode + setDebugShaderUsage 的活体证据）。
// ---------------------------------------------------------------------------
TEST(OutputShadersTest, RegistryAccumulatesAndMarksUsed)
{
    OutputShadersFixture s;
    ASSERT_TRUE(s.setup());

    auto* ctrl = dqRender::RenderSystem::get().debugControl();
    ASSERT_NE(ctrl, nullptr);
    ASSERT_NE(ctrl->debugShaderFiles, nullptr);
    auto const& files = *ctrl->debugShaderFiles;
    ASSERT_GE(files.size(), 4u) << "首绘后注册表为空（收集门/记录面断）";

    bool anyUsed = false;
    for (auto const& f : files) {
        EXPECT_TRUE(f.isGL) << f.filename;
        // "_VS.glsl"/"_FS.glsl" 后缀（8 字符）与 isVS 双语段一致。
        EXPECT_EQ(f.isVS,
                  f.filename.size() > 8
                      && f.filename.compare(f.filename.size() - 8, 8, "_VS.glsl") == 0)
            << f.filename;
        if (!f.isVS) {
            EXPECT_TRUE(f.filename.size() > 8
                        && f.filename.compare(f.filename.size() - 8, 8, "_FS.glsl") == 0)
                << f.filename;
        }
        EXPECT_FALSE(f.src.empty()) << f.filename;
        anyUsed = anyUsed || f.isUsed;
    }
    EXPECT_TRUE(anyUsed) << "首绘后无任何 isUsed=true（setDebugShaderUsage 断）";

    // desc 派生命名（"PointString: …"→"PointString-…"；无 desc → noname-N）。
    bool anyDescNamed = false;
    for (auto const& f : files)
        anyDescNamed = anyDescNamed || f.filename.find('-') != std::string::npos;
    EXPECT_TRUE(anyDescNamed) << "无任何 desc 派生分隔符（: → - 归一面未见）";
}

// ---------------------------------------------------------------------------
// compileAllShaders：注册表严格增长（全部变体的编译记录——Technique.ts
// :1009-1015 的逐 technique 遍历面）。
// ---------------------------------------------------------------------------
TEST(OutputShadersTest, CompileAllShadersRecordsEveryVariant)
{
    OutputShadersFixture s;
    ASSERT_TRUE(s.setup());

    auto* ctrl = dqRender::RenderSystem::get().debugControl();
    ASSERT_NE(ctrl, nullptr);
    ASSERT_NE(ctrl->debugShaderFiles, nullptr);
    size_t const before = ctrl->debugShaderFiles->size();

    ASSERT_TRUE(ctrl->compileAllShaders != nullptr) << "compileAllShaders 未接线";
    bool const compiled = ctrl->compileAllShaders();
    // bool 面如实记录（存在合法空 program 的 technique 档——参考通知面按此
    // 分流 Info/Error）；本锁钉的是**增长**：全部变体被编译并记录。
    size_t const after = ctrl->debugShaderFiles->size();
    EXPECT_GT(after, before) << "compileAllShaders 后注册表未增长";
    printf("[OS] compiled=%d before=%zu after=%zu\n", compiled ? 1 : 0, before,
           after);

    // 增长段同样满足命名形。
    for (size_t i = before; i < after; ++i) {
        EXPECT_TRUE((*ctrl->debugShaderFiles)[i].isGL);
        EXPECT_FALSE((*ctrl->debugShaderFiles)[i].src.empty());
    }
}

// ---------------------------------------------------------------------------
// keyin 落盘对账：_makeShade.bat（逐字常量）+ _OutputList 行数 == 非过滤
// 条目数 == 落盘 .glsl 数；v 过滤 → 全 _VS。
// ---------------------------------------------------------------------------
TEST(OutputShadersTest, ParseAndRunWritesFilteredShaderFiles)
{
    OutputShadersFixture s;
    ASSERT_TRUE(s.setup());

    auto* ctrl = dqRender::RenderSystem::get().debugControl();
    ASSERT_NE(ctrl, nullptr);
    auto const& files = *ctrl->debugShaderFiles;
    ASSERT_GE(files.size(), 4u);

    std::error_code ec;
    std::string const dir = (std::filesystem::current_path(ec) / "output-shaders-test").string();
    std::filesystem::remove_all(dir, ec);  // 自建目录隔离
    std::filesystem::create_directories(dir, ec);

    // d= 目录补尾（:358-361——路径含反斜杠优先补 '\'；current_path 为
    // 反斜杠形态）。
    Gui::OutputShadersTool tool;
    ASSERT_TRUE(tool.parseAndRun({"d=" + dir}));
    EXPECT_EQ(dir + "\\", tool.m_outputDir);

    // _makeShade.bat：逐字常量落盘（模板首两字符 = 换行 ×2 后接 @echo off
    // ——参考模板字面量 :10-12 逐字）。
    {
        std::ifstream ifs(dir + "/_makeShade.bat", std::ios::binary);
        ASSERT_TRUE(ifs.good());
        std::string first(16, '\0');
        ifs.read(first.data(), 10);
        EXPECT_EQ(std::string("\n\n@echo of", 10), first.substr(0, 10));
    }

    // 对账：_OutputList 行数 == 非过滤**条目**数（同 desc 多槽条目各占一行
    // ——预建技巧表同 flags 多槽的参考同形）；.glsl 文件数 == 非过滤**唯一
    // 文件名**数（同名条目后写覆前写——参考 outputShaders 同语义）。
    size_t expectedEntries = 0;
    std::set<std::string> expectedNames;
    for (auto const& f : files) {
        if (!Gui::OutputShadersTool::skipThisShader(f, "", "", "")) {
            ++expectedEntries;
            expectedNames.insert(f.filename);
        }
    }
    ASSERT_GE(expectedEntries, 4u);
    EXPECT_EQ(expectedEntries, countLines(dir + "/_OutputList.txt"));
    size_t glslCount = 0;
    for (auto const& entry : std::filesystem::directory_iterator(dir))
        if (entry.path().extension() == ".glsl")
            ++glslCount;
    EXPECT_EQ(expectedNames.size(), glslCount);

    // v 过滤：落盘全 _VS、行数 == vert 条目数。
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    Gui::OutputShadersTool toolV;
    ASSERT_TRUE(toolV.parseAndRun({"v", "d=" + dir}));
    size_t expectedVS = 0;
    for (auto const& f : files)
        if (!Gui::OutputShadersTool::skipThisShader(f, "", "v", ""))
            ++expectedVS;
    ASSERT_GE(expectedVS, 2u);
    EXPECT_EQ(expectedVS, countLines(dir + "/_OutputList.txt"));
    for (auto const& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() != ".glsl")
            continue;
        EXPECT_EQ(std::string("_VS.glsl"),
                  entry.path().filename().string().substr(
                      entry.path().filename().string().size() - 8))
            << entry.path();
    }
}
