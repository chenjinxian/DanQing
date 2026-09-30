// SPDX-License-Identifier: LGPL-2.1-or-later
// DanQing DisplayTestApp — ReadMe 展示页（M-L(3) Task B）。
//
// Start 页第三分组 "ReadMe" 卡片的落点：滚动只读页（QTextBrowser——Qt Widgets
// 自带，无新依赖），与 StartView 同构（Gui::MDIView 子类）。内容 = 分析报告 §4
//（docs/DTA功能对照与专业化分析-2026-09-30.md）陈列的 12 条已锁能力，分三组
//（A 真实 iModel 渲染管线 / B 交互拾取 / C 诊断）；每条 = 标题 + 一句话说明 +
// 判据测试名（§11.11 判据有效性纪律——只陈列像素锁/同构锁在案的能力）。
//
// 口径纪律（§4.2）：数据面是"本地回放真实后端采集数据（dump 回放，零网络）"，
// 不写"支持打开任意 .bim"；不放截图位（截图会漂移）。
#pragma once

#include "MDIView.h"

class QTextBrowser;

namespace StartGui {

class ReadMeView : public Gui::MDIView
{
    Q_OBJECT
public:
    explicit ReadMeView(QWidget* parent = nullptr);

    const char* getName() const override { return "ReadMeView"; }

private:
    QTextBrowser* m_browser = nullptr;
};

}  // namespace StartGui
