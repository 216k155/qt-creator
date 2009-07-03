/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://www.qtsoftware.com/contact.
**
**************************************************************************/

#include "editorview.h"
#include "editormanager.h"
#include "coreimpl.h"
#include "minisplitter.h"
#include "openeditorsmodel.h"

#include <utils/qtcassert.h>

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QMimeData>

#include <QtGui/QApplication>
#include <QtGui/QComboBox>
#include <QtGui/QHBoxLayout>
#include <QtGui/QLabel>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QStackedWidget>
#include <QtGui/QStyle>
#include <QtGui/QStyleOption>
#include <QtGui/QToolBar>
#include <QtGui/QToolButton>
#include <QtGui/QMenu>
#include <QtGui/QClipboard>

#ifdef Q_WS_MAC
#include <qmacstyle_mac.h>
#endif

Q_DECLARE_METATYPE(Core::IEditor *)

using namespace Core;
using namespace Core::Internal;


//================EditorView====================

EditorView::EditorView(OpenEditorsModel *model, QWidget *parent) :
    QWidget(parent),
    m_model(model),
    m_toolBar(new QWidget),
    m_container(new QStackedWidget(this)),
    m_editorList(new QComboBox),
    m_closeButton(new QToolButton),
    m_lockButton(new QToolButton),
    m_defaultToolBar(new QToolBar(this)),
    m_infoWidget(new QFrame(this)),
    m_editorForInfoWidget(0),
    m_statusHLine(new QFrame(this)),
    m_statusWidget(new QFrame(this))
{
    QVBoxLayout *tl = new QVBoxLayout(this);
    tl->setSpacing(0);
    tl->setMargin(0);
    {
        if (!m_model) {
            m_model = CoreImpl::instance()->editorManager()->openedEditorsModel();
        }

        m_editorList->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        m_editorList->setMinimumContentsLength(20);
        m_editorList->setModel(m_model);
        m_editorList->setMaxVisibleItems(40);
        m_editorList->setContextMenuPolicy(Qt::CustomContextMenu);

        QToolBar *editorListToolBar = new QToolBar;

        editorListToolBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
        editorListToolBar->addWidget(m_editorList);

        m_defaultToolBar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        m_activeToolBar = m_defaultToolBar;

        QHBoxLayout *toolBarLayout = new QHBoxLayout;
        toolBarLayout->setMargin(0);
        toolBarLayout->setSpacing(0);
        toolBarLayout->addWidget(m_defaultToolBar);
        m_toolBar->setLayout(toolBarLayout);

        m_lockButton->setAutoRaise(true);
        m_lockButton->setProperty("type", QLatin1String("dockbutton"));

        m_closeButton->setAutoRaise(true);
        m_closeButton->setIcon(QIcon(":/core/images/closebutton.png"));
        m_closeButton->setProperty("type", QLatin1String("dockbutton"));

        QToolBar *rightToolBar = new QToolBar;
        rightToolBar->setLayoutDirection(Qt::RightToLeft);
        rightToolBar->addWidget(m_closeButton);
        rightToolBar->addWidget(m_lockButton);

        QHBoxLayout *toplayout = new QHBoxLayout;
        toplayout->setSpacing(0);
        toplayout->setMargin(0);
        toplayout->addWidget(editorListToolBar);
        toplayout->addWidget(m_toolBar, 1); // Custom toolbar stretches
        toplayout->addWidget(rightToolBar);

        QWidget *top = new QWidget;
        QVBoxLayout *vlayout = new QVBoxLayout(top);
        vlayout->setSpacing(0);
        vlayout->setMargin(0);
        vlayout->addLayout(toplayout);
        tl->addWidget(top);

        connect(m_editorList, SIGNAL(activated(int)), this, SLOT(listSelectionActivated(int)));
        connect(m_editorList, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(listContextMenu(QPoint)));
        connect(m_lockButton, SIGNAL(clicked()), this, SLOT(makeEditorWritable()));
        connect(m_closeButton, SIGNAL(clicked()), this, SLOT(closeView()), Qt::QueuedConnection);
    }
    {
        m_infoWidget->setFrameStyle(QFrame::Panel | QFrame::Raised);
        m_infoWidget->setLineWidth(1);
        m_infoWidget->setForegroundRole(QPalette::ToolTipText);
        m_infoWidget->setBackgroundRole(QPalette::ToolTipBase);
        m_infoWidget->setAutoFillBackground(true);

        QHBoxLayout *hbox = new QHBoxLayout(m_infoWidget);
        hbox->setMargin(2);
        m_infoWidgetLabel = new QLabel("Placeholder");
        m_infoWidgetLabel->setForegroundRole(QPalette::ToolTipText);
        hbox->addWidget(m_infoWidgetLabel);
        hbox->addStretch(1);

        m_infoWidgetButton = new QToolButton;
        m_infoWidgetButton->setText(tr("Placeholder"));
        hbox->addWidget(m_infoWidgetButton);

        QToolButton *closeButton = new QToolButton;
        closeButton->setAutoRaise(true);
        closeButton->setIcon(QIcon(":/core/images/clear.png"));
        closeButton->setToolTip(tr("Close"));
        connect(closeButton, SIGNAL(clicked()), m_infoWidget, SLOT(hide()));

        hbox->addWidget(closeButton);

        m_infoWidget->setVisible(false);
        tl->addWidget(m_infoWidget);
    }

    tl->addWidget(m_container);

    {
        m_statusHLine->setFrameStyle(QFrame::HLine);

        m_statusWidget->setFrameStyle(QFrame::Panel | QFrame::Raised);
        m_statusWidget->setLineWidth(1);
        //m_statusWidget->setForegroundRole(QPalette::ToolTipText);
        //m_statusWidget->setBackgroundRole(QPalette::ToolTipBase);
        m_statusWidget->setAutoFillBackground(true);


        QHBoxLayout *hbox = new QHBoxLayout(m_statusWidget);
        hbox->setMargin(2);
        m_statusWidgetLabel = new QLabel("Placeholder");
        m_statusWidgetLabel->setForegroundRole(QPalette::ToolTipText);
        hbox->addWidget(m_statusWidgetLabel);
        hbox->addStretch(1);

        m_statusWidgetButton = new QToolButton;
        m_statusWidgetButton->setText(tr("Placeholder"));
        hbox->addWidget(m_statusWidgetButton);

        m_statusHLine->setVisible(false);
        m_statusWidget->setVisible(false);
        tl->addWidget(m_statusHLine);
        tl->addWidget(m_statusWidget);
    }
}

EditorView::~EditorView()
{
}

void EditorView::showEditorInfoBar(const QString &kind,
                                           const QString &infoText,
                                           const QString &buttonText,
                                           QObject *object, const char *member)
{
    m_infoWidgetKind = kind;
    m_infoWidgetLabel->setText(infoText);
    m_infoWidgetButton->setText(buttonText);
    m_infoWidgetButton->disconnect();
    if (object && member)
        connect(m_infoWidgetButton, SIGNAL(clicked()), object, member);
    m_infoWidget->setVisible(true);
    m_editorForInfoWidget = currentEditor();
}

void EditorView::hideEditorInfoBar(const QString &kind)
{
    if (kind == m_infoWidgetKind)
        m_infoWidget->setVisible(false);
}

void EditorView::showEditorStatusBar(const QString &kind,
                                     const QString &infoText,
                                     const QString &buttonText,
                                     QObject *object, const char *member)
{
    m_statusWidgetKind = kind;
    m_statusWidgetLabel->setText(infoText);
    m_statusWidgetButton->setText(buttonText);
    m_statusWidgetButton->disconnect();
    if (object && member)
        connect(m_statusWidgetButton, SIGNAL(clicked()), object, member);
    m_statusWidget->setVisible(true);
    m_statusHLine->setVisible(true);
    //m_editorForInfoWidget = currentEditor();
}

void EditorView::hideEditorStatusBar(const QString &kind)
{
    if (kind == m_statusWidgetKind) {
        m_statusWidget->setVisible(false);
        m_statusHLine->setVisible(false);
    }
}

void EditorView::addEditor(IEditor *editor)
{
    if (m_editors.contains(editor))
        return;

    m_editors.append(editor);

    m_container->addWidget(editor->widget());
    m_widgetEditorMap.insert(editor->widget(), editor);

    QToolBar *toolBar = editor->toolBar();
    if (toolBar) {
        toolBar->setVisible(false); // will be made visible in setCurrentEditor
        m_toolBar->layout()->addWidget(toolBar);
    }
    connect(editor, SIGNAL(changed()), this, SLOT(checkEditorStatus()));

    if (editor == currentEditor())
        setCurrentEditor(editor);
}

bool EditorView::hasEditor(IEditor *editor) const
{
    return m_editors.contains(editor);
}

void EditorView::closeView()
{
    EditorManager *em = CoreImpl::instance()->editorManager();
    if (IEditor *editor = currentEditor()) {
            em->closeDuplicate(editor);
    }
}

void EditorView::removeEditor(IEditor *editor)
{
    QTC_ASSERT(editor, return);
    if (!m_editors.contains(editor))
        return;

    const int index = m_container->indexOf(editor->widget());
    QTC_ASSERT((index != -1), return);
    bool wasCurrent = (index == m_container->currentIndex());
    m_editors.removeAll(editor);

    m_container->removeWidget(editor->widget());
    m_widgetEditorMap.remove(editor->widget());
    editor->widget()->setParent(0);
    disconnect(editor, SIGNAL(changed()), this, SLOT(updateEditorStatus()));
    QToolBar *toolBar = editor->toolBar();
    if (toolBar != 0) {
        if (m_activeToolBar == toolBar) {
            m_activeToolBar = m_defaultToolBar;
            m_activeToolBar->setVisible(true);
        }
        m_toolBar->layout()->removeWidget(toolBar);
        toolBar->setVisible(false);
        toolBar->setParent(0);
    }
    if (wasCurrent)
        setCurrentEditor(m_editors.count() ? m_editors.last() : 0);
}

IEditor *EditorView::currentEditor() const
{
    if (m_container->count() > 0)
        return m_widgetEditorMap.value(m_container->currentWidget());
    return 0;
}

void EditorView::setCurrentEditor(IEditor *editor)
{
    if (!editor || m_container->count() <= 0
        || m_container->indexOf(editor->widget()) == -1) {
        // ### TODO the combo box m_editorList should show an empty item
        return;
    }

    m_editors.removeAll(editor);
    m_editors.append(editor);

    const int idx = m_container->indexOf(editor->widget());
    QTC_ASSERT(idx >= 0, return);
    m_container->setCurrentIndex(idx);
    m_editorList->setCurrentIndex(m_model->indexOf(editor).row());
    updateEditorStatus(editor);
    updateToolBar(editor);

    // FIXME: this keeps the editor hidden if switching from A to B and back
    if (editor != m_editorForInfoWidget) {
        m_infoWidget->hide();
        m_editorForInfoWidget = 0;
    }
}

void EditorView::checkEditorStatus()
{
    IEditor *editor = qobject_cast<IEditor *>(sender());
    if (editor == currentEditor())
        updateEditorStatus(editor);
}

void EditorView::updateEditorStatus(IEditor *editor)
{
    static const QIcon lockedIcon(QLatin1String(":/core/images/locked.png"));
    static const QIcon unlockedIcon(QLatin1String(":/core/images/unlocked.png"));

    if (editor->file()->isReadOnly()) {
        m_lockButton->setIcon(lockedIcon);
        m_lockButton->setEnabled(!editor->file()->fileName().isEmpty());
        m_lockButton->setToolTip(tr("Make writable"));
    } else {
        m_lockButton->setIcon(unlockedIcon);
        m_lockButton->setEnabled(false);
        m_lockButton->setToolTip(tr("File is writable"));
    }
    if (currentEditor() == editor)
        m_editorList->setToolTip(
                editor->file()->fileName().isEmpty()
                ? editor->displayName()
                    : QDir::toNativeSeparators(editor->file()->fileName())
                    );

}

void EditorView::updateToolBar(IEditor *editor)
{
    QToolBar *toolBar = editor->toolBar();
    if (!toolBar)
        toolBar = m_defaultToolBar;
    if (m_activeToolBar == toolBar)
        return;
    toolBar->setVisible(true);
    m_activeToolBar->setVisible(false);
    m_activeToolBar = toolBar;
}

int EditorView::editorCount() const
{
    return m_container->count();
}

QList<IEditor *> EditorView::editors() const
{
    return m_widgetEditorMap.values();
}


void EditorView::makeEditorWritable()
{
    CoreImpl::instance()->editorManager()->makeEditorWritable(currentEditor());
}

void EditorView::listSelectionActivated(int index)
{
    EditorManager *em = CoreImpl::instance()->editorManager();
    QAbstractItemModel *model = m_editorList->model();
    if (IEditor *editor = model->data(model->index(index, 0), Qt::UserRole).value<IEditor*>()) {
        em->activateEditor(this, editor);
    } else {
        em->activateEditor(model->index(index, 0), this);
    }
}

void EditorView::listContextMenu(QPoint pos)
{
    QModelIndex index = m_model->index(m_editorList->currentIndex(), 0);
    QString fileName = m_model->data(index, Qt::UserRole + 1).toString();
    if (fileName.isEmpty())
        return;
    QMenu menu;
    menu.addAction(tr("Copy full path to clipboard"));
    if (menu.exec(m_editorList->mapToGlobal(pos))) {
        QApplication::clipboard()->setText(fileName);
    }
}

SplitterOrView::SplitterOrView(OpenEditorsModel *model)
{
    Q_ASSERT(model);
    m_isRoot = true;
    m_layout = new QStackedLayout(this);
    m_view = new EditorView(model);
    m_splitter = 0;
    m_layout->addWidget(m_view);
    setFocusPolicy(Qt::ClickFocus);
}

SplitterOrView::SplitterOrView(Core::IEditor *editor)
{
    m_isRoot = false;
    m_layout = new QStackedLayout(this);
    m_view = new EditorView();
    if (editor)
        m_view->addEditor(editor);
    m_splitter = 0;
    m_layout->addWidget(m_view);
    setFocusPolicy(Qt::ClickFocus);
}

SplitterOrView::~SplitterOrView()
{
    delete m_layout;
    m_layout = 0;
    delete m_view;
    m_view = 0;
    delete m_splitter;
    m_splitter = 0;
}



void SplitterOrView::focusInEvent(QFocusEvent *)
{
    CoreImpl::instance()->editorManager()->setCurrentView(this);
}

void SplitterOrView::paintEvent(QPaintEvent *)
{
    if  (CoreImpl::instance()->editorManager()->currentView() != this)
        return;
    QPainter painter(this);

    // Discreet indication where an editor would be
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    QColor shadeBrush(Qt::black);
    shadeBrush.setAlpha(10);
    painter.setBrush(shadeBrush);
    const int r = 3;
    painter.drawRoundedRect(rect().adjusted(r, r, -r, -r), r * 2, r * 2);

    if (hasFocus()) {
#ifdef Q_WS_MAC
        // With QMacStyle, we have to draw our own focus rect, since I didn't find
        // a way to draw the nice mac focus rect _inside_ this widget
        if (qobject_cast<QMacStyle *>(style())) {
            painter.setPen(Qt::DotLine);
            painter.setBrush(Qt::NoBrush);
            painter.setOpacity(0.75);
            painter.drawRect(rect());
        } else {
#endif
            QStyleOptionFocusRect option;
            option.initFrom(this);
            option.backgroundColor = palette().color(QPalette::Background);

            // Some styles require a certain state flag in order to draw the focus rect
            option.state |= QStyle::State_KeyboardFocusChange;

            style()->drawPrimitive(QStyle::PE_FrameFocusRect, &option, &painter);
#ifdef Q_WS_MAC
        }
#endif
    }
}

SplitterOrView *SplitterOrView::findFirstView()
{
    if (m_splitter) {
        for (int i = 0; i < m_splitter->count(); ++i) {
            if (SplitterOrView *splitterOrView = qobject_cast<SplitterOrView*>(m_splitter->widget(i)))
                if (SplitterOrView *result = splitterOrView->findFirstView())
                    return result;
        }
        return 0;
    }
    return this;
}

SplitterOrView *SplitterOrView::findEmptyView()
{
    if (m_splitter) {
        for (int i = 0; i < m_splitter->count(); ++i) {
            if (SplitterOrView *splitterOrView = qobject_cast<SplitterOrView*>(m_splitter->widget(i)))
                if (SplitterOrView *result = splitterOrView->findEmptyView())
                    return result;
        }
        return 0;
    }
    if (!hasEditors())
        return this;
    return 0;
}

SplitterOrView *SplitterOrView::findView(Core::IEditor *editor)
{
    if (!editor || hasEditor(editor))
        return this;
    if (m_splitter) {
        for (int i = 0; i < m_splitter->count(); ++i) {
            if (SplitterOrView *splitterOrView = qobject_cast<SplitterOrView*>(m_splitter->widget(i)))
                if (SplitterOrView *result = splitterOrView->findView(editor))
                    return result;
        }
    }
    return 0;
}

SplitterOrView *SplitterOrView::findView(EditorView *view)
{
    if (view == m_view)
        return this;
    if (m_splitter) {
        for (int i = 0; i < m_splitter->count(); ++i) {
            if (SplitterOrView *splitterOrView = qobject_cast<SplitterOrView*>(m_splitter->widget(i)))
                if (SplitterOrView *result = splitterOrView->findView(view))
                    return result;
        }
    }
    return 0;
}

SplitterOrView *SplitterOrView::findSplitter(Core::IEditor *editor)
{
    if (m_splitter) {
        for (int i = 0; i < m_splitter->count(); ++i) {
            if (SplitterOrView *splitterOrView = qobject_cast<SplitterOrView*>(m_splitter->widget(i))) {
                if (splitterOrView->hasEditor(editor))
                    return this;
                if (SplitterOrView *result = splitterOrView->findSplitter(editor))
                    return result;
            }
        }
    }
    return 0;
}

SplitterOrView *SplitterOrView::findSplitter(SplitterOrView *child)
{
    if (m_splitter) {
        for (int i = 0; i < m_splitter->count(); ++i) {
            if (SplitterOrView *splitterOrView = qobject_cast<SplitterOrView*>(m_splitter->widget(i))) {
                if (splitterOrView == child)
                    return this;
                if (SplitterOrView *result = splitterOrView->findSplitter(child))
                    return result;
            }
        }
    }
    return 0;
}

SplitterOrView *SplitterOrView::findNextView(SplitterOrView *view)
{
    bool found = false;
    return findNextView_helper(view, &found);
}

SplitterOrView *SplitterOrView::findNextView_helper(SplitterOrView *view, bool *found)
{
    if (*found && m_view) {
        return this;
    }

    if (this == view) {
        *found = true;
        return 0;
    }

    if (m_splitter) {
        for (int i = 0; i < m_splitter->count(); ++i) {
            if (SplitterOrView *splitterOrView = qobject_cast<SplitterOrView*>(m_splitter->widget(i))) {
                if (SplitterOrView *result = splitterOrView->findNextView_helper(view, found))
                    return result;
            }
        }
    }
    return 0;
}

QSize SplitterOrView::minimumSizeHint() const
{
    if (m_splitter)
        return m_splitter->minimumSizeHint();
    return QSize(64, 64);
}

QSplitter *SplitterOrView::takeSplitter()
{
    QSplitter *oldSplitter = m_splitter;
    if (m_splitter)
        m_layout->removeWidget(m_splitter);
    m_splitter = 0;
    return oldSplitter;
}

EditorView *SplitterOrView::takeView()
{
    EditorView *oldView = m_view;
    if (m_view)
        m_layout->removeWidget(m_view);
    m_view = 0;
    return oldView;
}

void SplitterOrView::split(Qt::Orientation orientation)
{
    Q_ASSERT(m_view && m_splitter == 0);
    m_splitter = new MiniSplitter(this);
    m_splitter->setOrientation(orientation);
    m_layout->addWidget(m_splitter);
    EditorManager *em = CoreImpl::instance()->editorManager();
    Core::IEditor *e = m_view->currentEditor();

    SplitterOrView *view = 0;
    if (e) {

        m_view->removeEditor(e);
        m_splitter->addWidget(new SplitterOrView(e));
#if 1
        if (e->duplicateSupported()) {
            Core::IEditor *duplicate = em->duplicateEditor(e);
            m_splitter->addWidget(new SplitterOrView(duplicate));
        } else {
            m_splitter->addWidget(new SplitterOrView());
        }
#else
        m_splitter->addWidget((view = new SplitterOrView()));
#endif
    } else {
        m_splitter->addWidget(new SplitterOrView());
        m_splitter->addWidget((view = new SplitterOrView()));
    }

    m_layout->setCurrentWidget(m_splitter);

    if (m_view && !m_isRoot) {
        em->emptyView(m_view);
        delete m_view;
        m_view = 0;
    }

    em->setCurrentView(view);
#if 1
    if (e)
        em->activateEditor(e);
#endif
}

void SplitterOrView::unsplitAll()
{
    m_splitter->hide();
    m_layout->removeWidget(m_splitter); // workaround Qt bug
    unsplitAll_helper();
    delete m_splitter;
    m_splitter = 0;
}

void SplitterOrView::unsplitAll_helper()
{
    if (!m_isRoot && m_view)
        CoreImpl::instance()->editorManager()->emptyView(m_view);
    if (m_splitter) {
        for (int i = 0; i < m_splitter->count(); ++i) {
            if (SplitterOrView *splitterOrView = qobject_cast<SplitterOrView*>(m_splitter->widget(i))) {
                splitterOrView->unsplitAll_helper();
            }
        }
    }
}

void SplitterOrView::unsplit()
{
    if (!m_splitter)
        return;

    Q_ASSERT(m_splitter->count() == 1);
    EditorManager *em = CoreImpl::instance()->editorManager();
    SplitterOrView *childSplitterOrView = qobject_cast<SplitterOrView*>(m_splitter->widget(0));
    QSplitter *oldSplitter = m_splitter;
    m_splitter = 0;

    if (childSplitterOrView->isSplitter()) {
        Q_ASSERT(childSplitterOrView->view() == 0);
        m_splitter = childSplitterOrView->takeSplitter();
        m_layout->addWidget(m_splitter);
        m_layout->setCurrentWidget(m_splitter);
    } else {
        EditorView *childView = childSplitterOrView->view();
        Q_ASSERT(childView);
        if (m_view) {
            if (IEditor *e = childView->currentEditor()) {
                childView->removeEditor(e);
                m_view->addEditor(e);
                m_view->setCurrentEditor(e);
            }
            em->emptyView(childView);
        } else {
            m_view = childSplitterOrView->takeView();
            m_layout->addWidget(m_view);
        }
        m_layout->setCurrentWidget(m_view);
    }
    delete oldSplitter;
    em->setCurrentView(findFirstView());
}
