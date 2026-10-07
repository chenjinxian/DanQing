// SPDX-License-Identifier: Apache-2.0
// DanQing DisplayTestApp — Saved Views 下拉面板实现
// Ported from: itwinjs-core test-apps/display-test-app/src/frontend/SavedViews.ts
#include "SavedViewsPanel.h"
#include "FeatureOverridesPanel.h"  // Provider（recall/save 的 override 面）

#include <dqApp/IModelConnection.h>
#include <dqApp/SelectionSet.h>
#include <dqApp/Viewport.h>
#include <dqApp/tile/DumpIModelConnection.h>  // props ↔ JSON 缝
#include <dqApp/tile/DumpTileTreeProps.h>     // dumpjson

#include <QDir>
#include <QEvent>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QString>

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>

namespace Gui {

// --- 持久化缝（EQUIVALENCE 见 SavedViewsPanel.h 文件头） ---
namespace {

std::string savedViewsDir()
{
    if (char const* env = std::getenv("DANQING_SAVED_VIEWS_DIR"))
        return env;
    return "./saved-views";
}

std::string sanitizeKey(std::string const& key)
{
    std::string out;
    for (char c : key)
        out.push_back(std::isalnum(static_cast<unsigned char>(c)) ? c : '_');
    return out;
}

}  // namespace

std::string readExternalSavedViews(std::string const& filename)
{
    std::string const path = savedViewsDir() + "/" + sanitizeKey(filename) + ".json";
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs)
        return "";
    return std::string((std::istreambuf_iterator<char>(ifs)),
                       std::istreambuf_iterator<char>());
}

void writeExternalSavedViews(std::string const& filename,
                             std::string const& esvString)
{
    std::string const dir = savedViewsDir();
    QDir().mkpath(QString::fromStdString(dir));
    std::string const path = dir + "/" + sanitizeKey(filename) + ".json";
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    ofs << esvString;
}

// --- _overrideElements 载荷（provider.toJSON/overrideElementsByArray 的
//     字串化——[{ "id": …, "fsa": … }]；dumpjson 解析 + 手写发出） ---
namespace {

void appendJsonEscapedLocal(std::string& out, std::string const& s)
{
    out.push_back('"');
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
        }
    }
    out.push_back('"');
}

std::string overrideElementsToJsonString(
    std::vector<FeatureOverridesProvider::ElementOverride> const& ovrs)
{
    std::string out = "[";
    bool first = true;
    for (auto const& o : ovrs) {
        if (!first)
            out.push_back(',');
        first = false;
        out += "{\"id\":";
        appendJsonEscapedLocal(out, o.id);
        out += ",\"fsa\":";   // ← FeatureOverrides.ts:52 键名 fsa（2026-10-07 审计 B2：原 fsaJson 自造）
        appendJsonEscapedLocal(out, o.fsaJson);
        out += "}";
    }
    out.push_back(']');
    return out;
}

std::optional<std::vector<FeatureOverridesProvider::ElementOverride>>
overrideElementsFromJsonString(std::string const& json)
{
    auto doc = dqApp::dumpjson::parseJsonDocument(json);
    if (!doc || doc->type != dqApp::dumpjson::JsonValue::Type::Array)
        return std::nullopt;
    std::vector<FeatureOverridesProvider::ElementOverride> out;
    for (auto const& obj : doc->arr) {
        if (obj.type != dqApp::dumpjson::JsonValue::Type::Object)
            return std::nullopt;
        FeatureOverridesProvider::ElementOverride eo;
        if (dqApp::dumpjson::JsonValue const* v = obj.find("id"))
            eo.id = v->str;
        if (dqApp::dumpjson::JsonValue const* v = obj.find("fsa"))
            eo.fsaJson = v->str;
        out.push_back(std::move(eo));
    }
    return out;
}

}  // namespace

SavedViewPicker::SavedViewPicker(dqApp::Viewport* vp, QWidget* parent,
                                 ApplySavedView applySavedView)
    : QWidget(parent)
    , m_vp(vp)
    , m_imodel(vp ? vp->GetIModel() : nullptr)
    , m_applySavedView(std::move(applySavedView))
{
    setWindowTitle(QStringLiteral("Saved Views"));
    setMinimumWidth(300);

    auto* layout = new QGridLayout(this);
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setObjectName(QStringLiteral("txt_viewName"));
    m_nameEdit->setToolTip(QStringLiteral("Name of new saved view to create"));
    layout->addWidget(m_nameEdit, 0, 0, 1, 2);

    m_viewsList = new QListWidget(this);
    m_viewsList->setObjectName(QStringLiteral("viewsList"));
    layout->addWidget(m_viewsList, 1, 0, 1, 2);

    m_createButton = new QPushButton(QStringLiteral("Create"), this);
    m_createButton->setObjectName(QStringLiteral("btn_createSavedView"));
    m_createButton->setToolTip(QStringLiteral("Create new saved view"));
    m_recallButton = new QPushButton(QStringLiteral("Recall"), this);
    m_recallButton->setObjectName(QStringLiteral("btn_recallSavedView"));
    m_recallButton->setToolTip(QStringLiteral("Recall selected view"));
    m_updateButton = new QPushButton(QStringLiteral("Update"), this);
    m_updateButton->setObjectName(QStringLiteral("btn_updateSavedView"));
    m_updateButton->setToolTip(QStringLiteral("Update selected view"));
    m_deleteButton = new QPushButton(QStringLiteral("Delete"), this);
    m_deleteButton->setObjectName(QStringLiteral("btn_deleteSavedView"));
    m_deleteButton->setToolTip(QStringLiteral("Delete selected view"));
    layout->addWidget(m_createButton, 2, 0, 1, 2);
    layout->addWidget(m_recallButton, 3, 0);
    layout->addWidget(m_updateButton, 3, 1);
    layout->addWidget(m_deleteButton, 4, 0, 1, 2);

    // Enter=saveView（:88-92 keypresshandler）/ 名字有效性（:170-175）。
    connect(m_nameEdit, &QLineEdit::returnPressed, this, [this] { saveView(); });
    connect(m_nameEdit, &QLineEdit::textChanged, this,
            [this](QString const& text) { setNewViewName(text.toStdString()); });
    // 双击=Recall（:118）/ Delete 键=Delete（:110-113）/ 选择变更（:109）。
    connect(m_viewsList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem*) { recallView(); });
    connect(m_viewsList, &QListWidget::itemSelectionChanged, this, [this] {
        setSelectedView(static_cast<int>(m_viewsList->currentRow()));
    });
    connect(m_createButton, &QPushButton::clicked, this, [this] { saveView(); });
    connect(m_recallButton, &QPushButton::clicked, this, [this] { recallView(); });
    connect(m_updateButton, &QPushButton::clicked, this, [this] { updateView(); });
    connect(m_deleteButton, &QPushButton::clicked, this, [this] { deleteView(); });
    // Delete 键（列表持有焦点时）。
    m_viewsList->installEventFilter(this);

    populate();
}

void SavedViewPicker::onViewChanged()
{
    if (m_vp && m_imodel != m_vp->GetIModel()) {
        m_imodel = m_vp->GetIModel();
        populate();
    }
}

void SavedViewPicker::populate()
{
    if (m_imodel == nullptr || m_imodel->IsClosed())
        return;
    m_views.loadFromString(readExternalSavedViews(m_imodel->GetKey()));
    populateFromViewList();
}

void SavedViewPicker::populateFromViewList()
{
    m_selectedIndex = -1;

    m_viewsList->clear();
    for (size_t i = 0; i < m_views.length(); ++i) {
        auto* item = new QListWidgetItem(
            QString::fromStdString(m_views.get(i).name()), m_viewsList);
        item->setData(Qt::UserRole, static_cast<int>(i));
    }
    m_nameEdit->clear();
    m_newViewName.clear();

    updateActionEnablement();
}

void SavedViewPicker::setNewViewName(std::string const& name)
{
    m_newViewName = name;
    updateActionEnablement();
}

void SavedViewPicker::setSelectedView(int index)
{
    m_selectedIndex = index;
    m_viewsList->setCurrentRow(index);
    updateActionEnablement();
}

void SavedViewPicker::updateActionEnablement()
{
    bool const haveSelection = m_selectedIndex >= 0
        && m_selectedIndex < static_cast<int>(m_views.length());
    m_recallButton->setEnabled(haveSelection);
    m_updateButton->setEnabled(haveSelection);
    m_deleteButton->setEnabled(haveSelection);
    // Create：名字空或已存在 → 置灰（:172-174；名字已存在 → 红）。
    bool const viewExists = findView(m_newViewName) != nullptr;
    m_createButton->setEnabled(!m_newViewName.empty() && !viewExists);
}

NamedViewStatePropsString const* SavedViewPicker::selectedEntry() const
{
    if (m_selectedIndex < 0 || m_selectedIndex >= static_cast<int>(m_views.length()))
        return nullptr;
    return &m_views.get(static_cast<size_t>(m_selectedIndex));
}

NamedViewStatePropsString const* SavedViewPicker::findView(
    std::string const& name) const
{
    int const index = m_views.findName(name);
    return -1 != index ? &m_views.get(static_cast<size_t>(index)) : nullptr;
}

void SavedViewPicker::recallView()
{
    auto const* sel = selectedEntry();
    if (sel == nullptr)
        return;

    auto const vsp = dqApp::deserializeViewStatePropsJson(
        sel->viewStatePropsString());
    if (!vsp.has_value())
        return;
    auto spatial = dqApp::SpatialViewState::CreateFromProps(*vsp, m_imodel);
    if (!spatial.IsValid())
        return;
    spatial->SetCodeValue(sel->name());  // viewState.code.value = name（:186）
    dqBase::RefPtr<dqApp::ViewState> view = spatial;
    if (m_applySavedView)
        m_applySavedView(view);
    else
        m_vp->ChangeView(view);

    auto const& overrideElementsString = sel->overrideElements();
    if (overrideElementsString.has_value()) {
        auto const overrideElements =
            overrideElementsFromJsonString(*overrideElementsString);
        if (overrideElements.has_value()) {
            auto* provider = FeatureOverridesProvider::getOrCreate(m_vp);
            if (provider != nullptr)
                provider->overrideElementsByArray(*overrideElements);
        }
    }

    auto const& selectedElementsString = sel->selectedElements();
    if (selectedElementsString.has_value()) {
        auto doc = dqApp::dumpjson::parseJsonDocument(*selectedElementsString);
        if (doc && doc->type == dqApp::dumpjson::JsonValue::Type::Array) {
            QSet<uint32_t> ids;
            for (auto const& e : doc->arr)
                ids.insert(static_cast<uint32_t>(std::strtoul(e.str.c_str(),
                                                              nullptr, 0)));
            m_imodel->GetSelectionSet().EmptyAll();
            m_imodel->GetSelectionSet().add(ids);
            m_vp->RenderFrame();
        }
    }

    // displayTransforms（:206-207）：DanQing 无 DisplayTransformProvider——
    // 不可达（保存侧恒缺席；文件头 EQUIVALENCE 登记）。
}

void SavedViewPicker::deleteView()
{
    auto const* sel = selectedEntry();
    if (sel != nullptr)
        deleteViewByName(sel->name());
}

void SavedViewPicker::deleteViewByName(std::string const& name)
{
    m_views.removeName(name);
    populateFromViewList();
    saveNamedViews();
}

void SavedViewPicker::saveView()
{
    saveViewWithName(m_newViewName);
}

void SavedViewPicker::saveViewWithName(std::string const& newName)
{
    if (newName.empty() || findView(newName) != nullptr)
        return;  // :226 同名拒绝

    auto* view3d = m_vp->GetView() ? m_vp->GetView()->AsViewState3d() : nullptr;
    if (view3d == nullptr)
        return;
    auto* spatial = view3d->AsSpatialViewState();
    if (spatial == nullptr)
        return;
    dqApp::ViewStateProps const props = spatial->ToProps();
    std::string const json = dqApp::serializeViewStatePropsJson(props);

    // selectedElements（:232-237）——选择集非空才写；参考形态 = Id64 字串数组
    //（JSON.stringify(seList)——字符串元素）。
    std::optional<std::string> selectedElementsString;
    if (m_imodel->GetSelectionSet().size() > 0) {
        std::string s = "[";
        bool first = true;
        for (uint32_t id : m_imodel->GetSelectionSet().GetElements()) {
            if (!first)
                s.push_back(',');
            first = false;
            s += '"';
            s += std::to_string(id);
            s += '"';
        }
        s.push_back(']');
        selectedElementsString = std::move(s);
    }

    // overrideElements（:239-244）——provider 存在且非空才写（getOrCreate 的
    // 空 toJSON = nullopt ⇒ 缺席，1:1）。
    std::optional<std::string> overrideElementsString;
    if (auto* provider = FeatureOverridesProvider::getOrCreate(m_vp)) {
        if (auto const overrideElements = provider->toJSON())
            overrideElementsString = overrideElementsToJsonString(*overrideElements);
    }

    NamedVSPSProps nvsp;
    nvsp._name = newName;
    nvsp._viewStatePropsString = json;
    nvsp._selectedElements = std::move(selectedElementsString);
    nvsp._overrideElements = std::move(overrideElementsString);
    // _displayTransforms：恒缺席（文件头 EQUIVALENCE）。
    m_views.insert(NamedViewStatePropsString(std::move(nvsp)));
    populateFromViewList();

    saveNamedViews();
}

void SavedViewPicker::updateView()
{
    auto const* sel = selectedEntry();
    if (sel == nullptr)
        return;
    std::string const name = sel->name();
    deleteViewByName(name);
    saveViewWithName(name);
}

void SavedViewPicker::saveNamedViews()
{
    if (m_vp == nullptr || m_vp->GetView() == nullptr
        || m_vp->GetIModel() == nullptr)
        return;
    writeExternalSavedViews(m_vp->GetIModel()->GetKey(), m_views.getPrintString());
}

// 列表 Delete 键（:110-113——"Delete" keyup 在 viewsList 上 → deleteView）。
bool SavedViewPicker::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_viewsList && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Delete) {
            deleteView();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

}  // namespace Gui
