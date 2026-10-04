// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Saved Views 下拉面板
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/SavedViews.ts
//              （SavedViewPicker :19-286——ToolBarDropDown 的 Qt 弹出等价，
//               ViewSettingsPanel/FeatureOverridesPanel 先例）
//
// EQUIVALENCE（§11.10）：
//   - 持久化：参考源 = DtaRpcInterface.readExternalSavedViews/
//     writeExternalSavedViews（SavedViews.ts:71-72/:274-280——后端键值存储，
//     键 = iModel.key）；发散 = DanQing §8.2 零网络——本地文件
//     <dir>/<sanitized key>.json，dir = env DANQING_SAVED_VIEWS_DIR 缺省
//     "./saved-views"（相对 CWD；测试经 env 钉绝对路径取确定性）。验证法 =
//     SavedViewsTest 持久化 round-trip 锁（第二 picker populate 见条目）。
//   - displayTransforms：参考 DisplayTransformProvider（SavedViews.ts:246-247
//     保存 / :206-207 恢复）；DanQing 无 DisplayTransformProvider——保存侧恒
//     缺席、恢复侧不可达（NamedVSPSProps._displayTransforms 载体保留）。
//   - selectedElements 的 id 域：参考 Id64 字串数组；DanQing SelectionSet 为
//     uint32 域（TD-28② 同族——保存/恢复自洽，跨实现不互操作）。
//   - onViewChanged（:55-65）：参考 ToolBarDropDown 常驻下拉的 iModel 变更
//     刷新；DanQing 弹出面板按需创建——构造即 populate 等价覆盖（面板无跨
//     iModel 生命周期）。
#pragma once

#include "NamedViews.h"

#include <dqApp/ViewState.h>

#include <QWidget>

#include <functional>
#include <string>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;

namespace dqApp {
class IModelConnection;
class Viewport;
}

namespace Gui {

// Ported from: itwinjs-core SavedViews.ts SavedViewPicker（:19-286）。
class SavedViewPicker final : public QWidget {
    Q_OBJECT
public:
    // ApplySavedView（:15-17）——viewer.applySavedView 的注入缝（DanQing 缺省
    // = Viewport::ChangeView，参考 Viewer 侧 = changeView + 标题刷新——
    // DanQing 标题刷新挂 OnChangeView 事件，ChangeView 即全覆盖）。
    using ApplySavedView = std::function<void(dqBase::RefPtr<dqApp::ViewState>)>;

    SavedViewPicker(dqApp::Viewport* vp, QWidget* parent,
                    ApplySavedView applySavedView = nullptr);

    // onViewChanged（:55-65）——iModel 变更 → populate 重读存储。
    void onViewChanged();
    // populate（:67-75）——读外部存储 + 重建 UI。
    void populate();

    // --- 行为面（参考私有方法 → 测试/宿主直接驱动；DTA = DOM 事件等价） ---
    void saveView();                                        // :221-223
    void saveViewWithName(std::string const& newName);      // :225-263
    void recallView();                                      // :180-208
    void updateView();                                      // :265-271
    void deleteView();                                      // :210-213
    void deleteViewByName(std::string const& name);         // :215-219

    // 文本框面（:84-93——Enter=saveView；onkeyup 名字有效性 :170-175）。
    void setNewViewName(std::string const& name);
    // 列表选择面（viewsDiv.onchange :109）。
    void setSelectedView(int index);

    NamedVSPSList const& views() const { return m_views; }

private:
    void populateFromViewList();  // :77-178
    NamedViewStatePropsString const* selectedEntry() const;
    NamedViewStatePropsString const* findView(std::string const& name) const;  // :282-285
    void saveNamedViews();       // :273-280
    void updateActionEnablement();  // :163-166/:177 的使能面

protected:
    // 列表 Delete 键（:110-113——viewsList keyup "Delete" → deleteView）。
    bool eventFilter(QObject* watched, QEvent* event) override;

    dqApp::Viewport* m_vp;
    dqApp::IModelConnection* m_imodel;
    NamedVSPSList m_views;
    int m_selectedIndex = -1;  // ← _selectedView（NamedViewStatePropsString）
    std::string m_newViewName;
    ApplySavedView m_applySavedView;

    QLineEdit* m_nameEdit = nullptr;
    QListWidget* m_viewsList = nullptr;
    QPushButton* m_createButton = nullptr;
    QPushButton* m_recallButton = nullptr;
    QPushButton* m_updateButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
};

// --- 持久化缝（EQUIVALENCE 见文件头） ---
// readExternalSavedViews（:72——缺席/不可读 = 空串 = loadFromString 清空语义）。
std::string readExternalSavedViews(std::string const& filename);
// writeExternalSavedViews（:279）。
void writeExternalSavedViews(std::string const& filename,
                             std::string const& esvString);

}  // namespace Gui
