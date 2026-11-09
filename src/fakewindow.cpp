/*
 * Copyright (C) 2021 LingmoOS Team.
 *
 * Author:     rekols <revenmartin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "fakewindow.h"
#include "docksettings.h"

// Qt
#include <QDebug>
#include <QApplication>
#include <QSurfaceFormat>
#include <QQuickView>
#include <QScreen>
#include <QTimer>

#include <KWindowSystem>
#include <KX11Extras>

// X11
#include <NETWM>

#include <LayerShellQt/Window>

FakeWindow::FakeWindow(QQuickView *parent)
    : QQuickView(parent)
    , m_delayedContainsMouse(false)
    , m_containsMouse(false)
{
    setColor(Qt::transparent);
    setDefaultAlphaBuffer(true);
    setFlags(Qt::FramelessWindowHint |
             Qt::WindowStaysOnTopHint |
             Qt::NoDropShadowWindowHint |
             Qt::WindowDoesNotAcceptFocus);
    if (KWindowSystem::isPlatformWayland()) {
        // A thin strip along the dock's edge, above the windows, that takes no
        // keyboard; it sits on the edge even where other panels reserve room
        m_layerShell = LayerShellQt::Window::get(this);
        m_layerShell->setScope(QStringLiteral("lingmo-dock-edge"));
        m_layerShell->setLayer(LayerShellQt::Window::LayerTop);
        m_layerShell->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
        m_layerShell->setActivateOnShow(false);
        m_layerShell->setExclusiveZone(-1);
        m_layerShell->setScreen(qApp->primaryScreen());
        updateGeometry();
    }
    // setScreen(qApp->primaryScreen());
    // updateGeometry();
    show();

    m_delayedMouseTimer.setSingleShot(true);
    m_delayedMouseTimer.setInterval(50);
    connect(&m_delayedMouseTimer, &QTimer::timeout, this, [this]() {
        if (m_delayedContainsMouse) {
            setContainsMouse(true);
        } else {
            setContainsMouse(false);
        }
    });

    connect(DockSettings::self(), &DockSettings::directionChanged, this, &FakeWindow::updateGeometry);
}

bool FakeWindow::containsMouse() const
{
    return m_containsMouse;
}

bool FakeWindow::event(QEvent *e)
{
    if (e->type() == QEvent::DragEnter || e->type() == QEvent::DragMove) {
        if (!m_containsMouse) {
            m_delayedContainsMouse = false;
            m_delayedMouseTimer.stop();
            setContainsMouse(true);
            emit dragEntered();
        }
    } else if (e->type() == QEvent::Enter) {
        m_delayedContainsMouse = true;
        if (!m_delayedMouseTimer.isActive()) {
            m_delayedMouseTimer.start();
        }
    } else if (e->type() == QEvent::Leave || e->type() == QEvent::DragLeave) {
        m_delayedContainsMouse = false;
        if (!m_delayedMouseTimer.isActive()) {
            m_delayedMouseTimer.start();
        }
    } else if (e->type() == QEvent::Show) {
        if (KWindowSystem::isPlatformX11())
            KX11Extras::setState(winId(), NET::SkipTaskbar | NET::SkipPager | NET::SkipSwitcher);
    }

    return QQuickView::event(e);
}

void FakeWindow::setContainsMouse(bool contains)
{
    if (m_containsMouse != contains) {
        m_containsMouse = contains;
        emit containsMouseChanged(contains);
    }
}

void FakeWindow::updateGeometry()
{
    int length = 5;
    const QRect screenRect = qApp->primaryScreen()->geometry();
    QRect newRect;

    switch (DockSettings::self()->direction())
    {
        case DockSettings::Left:
            newRect = QRect(screenRect.x() - (length * 2),
                            (screenRect.height() + length) / 2,
                            length, screenRect.height());
            break;
        case DockSettings::Bottom:
            newRect = QRect(screenRect.x(),
                            screenRect.y() + screenRect.height() - length,
                            screenRect.width(), length);
            break;
        case DockSettings::Right:
            newRect = QRect(screenRect.x() + screenRect.width() - length,
                            screenRect.y(),
                            length, screenRect.height());
            break;
    }

    setGeometry(newRect);

    if (m_layerShell) {
        using LS = LayerShellQt::Window;
        LS::Anchors anchors;
        switch (DockSettings::self()->direction()) {
        case DockSettings::Left:
            anchors = LS::Anchors(LS::AnchorLeft | LS::AnchorTop | LS::AnchorBottom);
            break;
        case DockSettings::Right:
            anchors = LS::Anchors(LS::AnchorRight | LS::AnchorTop | LS::AnchorBottom);
            break;
        case DockSettings::Bottom:
        default:
            anchors = LS::Anchors(LS::AnchorBottom | LS::AnchorLeft | LS::AnchorRight);
            break;
        }
        m_layerShell->setAnchors(anchors);
        if (screen() && m_layerShell->screen() != screen()) {
            // The output of a layer surface is fixed when it is created
            const bool visible = isVisible();
            setVisible(false);
            m_layerShell->setScreen(screen());
            setVisible(visible);
        }
    }
}
