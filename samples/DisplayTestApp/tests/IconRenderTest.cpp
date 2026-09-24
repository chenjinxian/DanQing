// Authored: no reference test exists in FreeCAD for icon-render conformance
//
// QtSvg emits "qt.svg: Invalid path data; path truncated" warnings when an
// SVG's <path d="..."> data contains a construct its renderer cannot parse
// (unsupported command sequence, malformed arc flags, scientific-notation
// coordinates, stray characters, etc.). The warning carries no filename, so
// this probe attributes each warning to the SVG being rendered via a
// thread-local "current SVG" + qInstallMessageHandler. It is a permanent
// conformance check: regressions on :/icons SVGs fail the suite.
#include <gtest/gtest.h>

#include <QApplication>
#include <QDir>
#include <QImage>
#include <QMessageLogContext>
#include <QObject>
#include <QPainter>
#include <QString>
#include <QStringList>
#include <QSvgRenderer>
#include <QtGlobal>

#include "QtTestFixtures.h"

namespace {
// Synchronous attribution: QSvgRenderer::render() is single-threaded; the
// handler reads g_currentSvg to learn which icon is mid-render when QtSvg
// complains. g_sawPathTruncation latches per-SVG.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static thread_local QString g_currentSvg;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static thread_local bool g_sawPathTruncation = false;

// Saved previous handler so we can forward (preserves unrelated Qt logging).
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static QtMessageHandler g_prevHandler = nullptr;

void svgMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    // QtSvg's warning text: "qt.svg: Invalid path data; path truncated".
    if (msg.contains(QLatin1String("Invalid path data")) ||
        msg.contains(QLatin1String("path truncated"))) {
        g_sawPathTruncation = true;
    }
    if (g_prevHandler) {
        g_prevHandler(type, context, msg);
    }
}
}  // namespace

// Authored: no reference test exists in FreeCAD for icon-render conformance
// Renders every SVG registered under :/icons and asserts none of them emit
// QtSvg path-truncation warnings. Regression guard for FreeCAD-icon imports.
TEST(IconRenderTest, AllIconsRenderWithoutPathTruncation)
{
    qtApp();  // ensure QApplication singleton (QSvgRenderer needs a QGuiApplication)

    // entryList over a qrc dir returns the registered aliases verbatim.
    const QStringList svgs = QDir(":/icons").entryList(QStringList{"*.svg"}, QDir::Files);
    ASSERT_FALSE(svgs.isEmpty()) << "No SVGs found under :/icons — resource init failed?";

    QStringList bad;
    g_prevHandler = qInstallMessageHandler(svgMessageHandler);
    for (const QString& name : svgs) {
        g_currentSvg = name;
        g_sawPathTruncation = false;
        {
            QSvgRenderer renderer(QStringLiteral(":/icons/") + name);
            if (!renderer.isValid()) {
                bad.append(name + QStringLiteral("  [QSvgRenderer::isValid() == false]"));
                continue;
            }
            QImage img(64, 64, QImage::Format_ARGB32);
            img.fill(Qt::transparent);
            QPainter painter(&img);
            renderer.render(&painter);
            painter.end();
        }
        if (g_sawPathTruncation) {
            bad.append(name);
        }
    }
    qInstallMessageHandler(g_prevHandler);
    g_prevHandler = nullptr;

    EXPECT_TRUE(bad.isEmpty())
        << "These :/icons SVGs produced 'qt.svg: Invalid path data; path truncated' "
        << "under QtSvg rendering:\n"
        << bad.join(QStringLiteral("\n")).toUtf8().constData();
}
