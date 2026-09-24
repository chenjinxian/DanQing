// OverlayTitleBar.h — 1:1 port of FreeCAD OverlayTitleBar + prepareTitleWidget
// Ported from: FreeCAD src/Gui/OverlayWidgets.h   lines 477-502
// Ported from: FreeCAD src/Gui/OverlayWidgets.cpp  lines 2059-2111 (prepareTitleWidget)
// Ported from: FreeCAD src/Gui/OverlayWidgets.cpp  lines 2145-2222 (paintEvent)
// Ported from: FreeCAD src/Gui/OverlayManager.cpp  lines 1009-1035 (createTitleBar)
//
// FreeCAD 结构：OverlayTitleBar 本身只负责绘制标题文字 + 拖拽。
// 按钮由 prepareTitleWidget() 创建并插入布局，OverlayTitleBar 通过
// setTitleItem() 拿到 spacer 的 geometry，paintEvent 在该区域内绘制标题。

#pragma once

#include <QBoxLayout>
#include <QDockWidget>
#include <QPainter>
#include <QSpacerItem>
#include <QToolButton>

namespace Gui
{

// Ported from: FreeCAD src/Gui/OverlayWidgets.h line 591
class OverlayToolButton: public QToolButton
{
    Q_OBJECT
public:
    explicit OverlayToolButton(QWidget* parent)
        : QToolButton(parent)
    {}
};

// Ported from: FreeCAD src/Gui/OverlayWidgets.h lines 477-502
class OverlayTitleBar: public QWidget
{
    Q_OBJECT
public:
    explicit OverlayTitleBar(QWidget* parent)
        : QWidget(parent)
    {
        setFocusPolicy(Qt::ClickFocus);
        setMouseTracking(true);
        setCursor(Qt::OpenHandCursor);
    }

    void setTitleItem(QLayoutItem* item)
    {
        titleItem = item;
    }

    // Ported from: FreeCAD src/Gui/OverlayWidgets.cpp line 2059
    // prepareTitleWidget — 创建布局：[spacing] [spacer] [buttons...]
    // spacer 的 geometry 就是标题文字的绘制区域
    static QLayoutItem* prepareTitleWidget(
        QWidget* widget, const QList<QAction*>& actions)
    {
        auto* layout = new QBoxLayout(QBoxLayout::LeftToRight, widget);
        layout->addSpacing(5);
        layout->setContentsMargins(1, 1, 1, 1);

        int buttonSize = widget->fontMetrics().ascent()
                       + widget->fontMetrics().descent();
        auto* spacer = new QSpacerItem(
            buttonSize, buttonSize,
            QSizePolicy::Expanding, QSizePolicy::Minimum);
        layout->addSpacerItem(spacer);

        for (auto* action : actions) {
            auto* button = new OverlayToolButton(widget);
            button->setObjectName(action->data().toString());
            button->setDefaultAction(action);
            button->setAutoRaise(true);
            button->setContentsMargins(0, 0, 0, 0);
            button->setFixedSize(buttonSize, buttonSize);
            layout->addWidget(button);
        }

        return spacer;
    }

    // Ported from: FreeCAD src/Gui/OverlayManager.cpp line 1009
    // createTitleBar — 创建 OverlayTitleBar + actions + prepareTitleWidget
    static OverlayTitleBar* createTitleBar(QDockWidget* dock)
    {
        auto* widget = new OverlayTitleBar(dock);
        widget->setObjectName(QStringLiteral("OverlayTitle"));

        // Ported from: FreeCAD src/Gui/OverlayManager.cpp lines 437-439
        // Icons use qss:overlay/icons/ path — mapped to :/icons/overlay/ in resources
        QList<QAction*> actions;

        auto* actOverlay = new QAction(widget);
        actOverlay->setData(QStringLiteral("OBTN Overlay"));
        actOverlay->setToolTip(QObject::tr("Toggle overlay"));
        actOverlay->setIcon(QIcon(QLatin1String(":/icons/overlay/overlay.svg")));
        actions.append(actOverlay);

        if (dock->features().testFlag(QDockWidget::DockWidgetFloatable)) {
            auto* actFloat = new QAction(widget);
            actFloat->setData(QStringLiteral("OBTN Float"));
            actFloat->setToolTip(QObject::tr("Toggle floating window"));
            actFloat->setIcon(QIcon(QLatin1String(":/icons/overlay/float.svg")));
            actions.append(actFloat);
            QObject::connect(actFloat, &QAction::triggered, dock, [dock]() {
                dock->setFloating(!dock->isFloating());
            });
        }

        if (dock->features().testFlag(QDockWidget::DockWidgetClosable)) {
            auto* actClose = new QAction(widget);
            actClose->setData(QStringLiteral("OBTN Close"));
            actClose->setToolTip(QObject::tr("Close dock window"));
            actClose->setIcon(QIcon(QLatin1String(":/icons/overlay/close.svg")));
            actions.append(actClose);
            QObject::connect(actClose, &QAction::triggered, dock, [dock]() {
                dock->toggleViewAction()->activate(QAction::Trigger);
            });
        }

        widget->setTitleItem(prepareTitleWidget(widget, actions));
        return widget;
    }

protected:
    // Ported from: FreeCAD src/Gui/OverlayWidgets.cpp line 2158
    void paintEvent(QPaintEvent*) override
    {
        if (!titleItem) {
            return;
        }

        QDockWidget* dock = qobject_cast<QDockWidget*>(parentWidget());
        if (!dock) {
            return;
        }

        QPainter painter(this);
        QRect r = titleItem->geometry();
        int flags = Qt::AlignCenter;

        QString title = dock->windowTitle();
        QString text = painter.fontMetrics().elidedText(title, Qt::ElideRight, r.width());
        painter.drawText(r, flags, text);
    }

private:
    QLayoutItem* titleItem = nullptr;
};

}  // namespace Gui
