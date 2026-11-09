/*
 * SPDX-FileCopyrightText: 2026 LingmoOS Team
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "waylandwindowinterface.h"
#include "utils.h"

#include <QDebug>
#include <QFile>
#include <QGuiApplication>
#include <QScreen>
#include <QStandardPaths>

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/plasmawindowmanagement.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/surface.h>

#include <LayerShellQt/Window>

using namespace KWayland::Client;

// The shell's own windows never go on the dock
static const QStringList s_ownAppIds = {
    QStringLiteral("lingmo-dock"),
    QStringLiteral("lingmo-statusbar"),
    QStringLiteral("lingmo-launcher"),
};

WaylandWindowInterface::WaylandWindowInterface(QObject *parent)
    : WindowInterface(parent)
{
    ConnectionThread *connection = ConnectionThread::fromApplication(this);
    if (!connection) {
        qWarning() << "dock: no Wayland connection, no window list";
        return;
    }

    m_registry = new Registry(this);
    m_registry->create(connection);
    connect(m_registry, &Registry::plasmaWindowManagementAnnounced, this, [this](quint32 name, quint32 version) {
        setManagement(m_registry->createPlasmaWindowManagement(name, version, this));
    });
    connect(m_registry, &Registry::interfacesAnnounced, this, [this] {
        if (!m_wm)
            qWarning() << "dock: KWin did not offer org_kde_plasma_window_management"
                       << "(X-KDE-Wayland-Interfaces in lingmo-dock.desktop?)";
    });
    m_registry->setup();
    connection->roundtrip();
}

void WaylandWindowInterface::setManagement(PlasmaWindowManagement *wm)
{
    m_wm = wm;
    connect(m_wm, &PlasmaWindowManagement::windowCreated, this, &WaylandWindowInterface::onWindowCreated);
    connect(m_wm, &PlasmaWindowManagement::activeWindowChanged, this, [this] {
        PlasmaWindow *w = m_wm->activeWindow();
        const quint64 id = w ? m_ids.value(w, 0) : 0;
        if (id != m_active) {
            m_active = id;
            emit activeChanged(id);
        }
    });
    connect(m_wm, &PlasmaWindowManagement::removed, this, [this] {
        const QList<quint64> ids = m_windows.keys();
        for (quint64 id : ids)
            removeWindow(id);
        m_wm = nullptr;
    });
    for (PlasmaWindow *w : m_wm->windows())
        onWindowCreated(w);
}

void WaylandWindowInterface::onWindowCreated(PlasmaWindow *w)
{
    if (!w || m_ids.contains(w))
        return;

    const quint64 id = m_nextId++;
    m_windows.insert(id, w);
    m_ids.insert(w, id);

    connect(w, &PlasmaWindow::unmapped, this, [this, id] { removeWindow(id); });
    connect(w, &QObject::destroyed, this, [this, id] { removeWindow(id); });
    // Whether it goes on the dock, and under which app
    connect(w, &PlasmaWindow::skipTaskbarChanged, this, [this, id] { evaluate(id); });
    connect(w, &PlasmaWindow::appIdChanged, this, [this, id] { evaluate(id); });
    connect(w, &PlasmaWindow::parentWindowChanged, this, [this, id] { evaluate(id); });
    // What the previews and the auto-hide follow
    connect(w, &PlasmaWindow::titleChanged, this, [this, id] { emit windowChanged(id); });
    connect(w, &PlasmaWindow::minimizedChanged, this, [this, id] { emit windowChanged(id); });
    connect(w, &PlasmaWindow::maximizedChanged, this, [this, id] { emit windowChanged(id); });
    connect(w, &PlasmaWindow::onAllDesktopsChanged, this, [this, id] { emit windowChanged(id); });
    connect(w, &PlasmaWindow::plasmaVirtualDesktopEntered, this, [this, id] { emit windowChanged(id); });
    connect(w, &PlasmaWindow::plasmaVirtualDesktopLeft, this, [this, id] { emit windowChanged(id); });

    if (w->isActive() && m_active != id) {
        m_active = id;
        emit activeChanged(id);
    }

    evaluate(id);
}

void WaylandWindowInterface::evaluate(quint64 wid)
{
    // Until the model listens, windows wait for startInitWindows()
    if (!m_started)
        return;

    PlasmaWindow *w = window(wid);
    if (!w)
        return;

    const bool acceptable = isAcceptableWindow(wid);
    const auto shown = m_shownWindows.constFind(wid);

    if (shown != m_shownWindows.constEnd()) {
        if (!acceptable) {
            m_shownWindows.remove(wid);
            emit windowRemoved(wid);
        } else if (shown.value() != w->appId()) {
            // Apps often set the app id right after mapping: regroup under the new one
            m_shownWindows.remove(wid);
            emit windowRemoved(wid);
            m_shownWindows.insert(wid, w->appId());
            emit windowAdded(wid);
        }
        return;
    }

    if (acceptable) {
        m_shownWindows.insert(wid, w->appId());
        emit windowAdded(wid);
    }
}

void WaylandWindowInterface::removeWindow(quint64 wid)
{
    PlasmaWindow *w = m_windows.value(wid);
    m_windows.remove(wid);
    if (w)
        m_ids.remove(w);
    else {
        for (auto it = m_ids.begin(); it != m_ids.end();) {
            if (it.value() == wid)
                it = m_ids.erase(it);
            else
                ++it;
        }
    }

    if (m_shownWindows.remove(wid))
        emit windowRemoved(wid);

    if (m_active == wid) {
        m_active = 0;
        emit activeChanged(0);
    }
}

PlasmaWindow *WaylandWindowInterface::window(quint64 wid) const
{
    PlasmaWindow *w = m_windows.value(wid);
    return (w && w->isValid()) ? w : nullptr;
}

WId WaylandWindowInterface::activeWindow()
{
    return m_active;
}

void WaylandWindowInterface::minimizeWindow(WId win)
{
    if (PlasmaWindow *w = window(win)) {
        if (!w->isMinimized())
            w->requestToggleMinimized();
    }
}

void WaylandWindowInterface::closeWindow(WId id)
{
    if (PlasmaWindow *w = window(id))
        w->requestClose();
}

void WaylandWindowInterface::forceActiveWindow(WId win)
{
    // KWin brings a minimized window back and switches to its desktop
    if (PlasmaWindow *w = window(win))
        w->requestActivate();
}

QMap<QString, QVariant> WaylandWindowInterface::requestInfo(quint64 wid)
{
    QMap<QString, QVariant> result;
    PlasmaWindow *w = window(wid);
    if (!w)
        return result;

    const QString appId = w->appId();
    result.insert("iconName", appId.toLower());
    result.insert("active", wid == m_active);
    result.insert("visibleName", w->title());
    result.insert("id", appId);
    return result;
}

QString WaylandWindowInterface::requestWindowClass(quint64 wid)
{
    PlasmaWindow *w = window(wid);
    return w ? w->appId() : QString();
}

bool WaylandWindowInterface::isAcceptableWindow(quint64 wid)
{
    PlasmaWindow *w = window(wid);
    if (!w)
        return false;

    // Like on X11: panels, menus, notifications... are skip-taskbar; dialogs of
    // another window (transients) go with that window
    if (w->skipTaskbar())
        return false;
    if (!w->parentWindow().isNull())
        return false;
    if (s_ownAppIds.contains(w->appId()))
        return false;

    return true;
}

void WaylandWindowInterface::setViewStruts(QWindow *view, DockSettings::Direction direction, const QRect &rect, bool compositing)
{
    LayerShellQt::Window *layer = LayerShellQt::Window::get(view);
    if (!layer)
        return;

    // Same room as the X11 strut (panel + edge margins), measured from the
    // screen edge. KWin adds the surface's margin on that edge to the zone.
    const bool isRound = DockSettings::self()->style() == DockSettings::Round;
    const int edgeMargins = compositing && isRound ? DockSettings::self()->edgeMargins() : 0;
    const QMargins margins = layer->margins();

    int zone = 0;
    switch (direction) {
    case DockSettings::Left:
        zone = rect.width() + edgeMargins - margins.left();
        break;
    case DockSettings::Right:
        zone = rect.width() + edgeMargins - margins.right();
        break;
    case DockSettings::Bottom:
    default:
        zone = rect.height() + edgeMargins - margins.bottom();
        break;
    }

    layer->setExclusiveZone(qMax(1, zone));
}

void WaylandWindowInterface::clearViewStruts(QWindow *view)
{
    if (LayerShellQt::Window *layer = LayerShellQt::Window::get(view))
        layer->setExclusiveZone(0);
}

void WaylandWindowInterface::startInitWindows()
{
    m_started = true;
    const QList<quint64> ids = m_windows.keys();
    QList<quint64> sorted = ids;
    std::sort(sorted.begin(), sorted.end());
    for (quint64 id : sorted)
        evaluate(id);
}

QString WaylandWindowInterface::desktopFilePath(quint64 wid)
{
    PlasmaWindow *w = window(wid);
    if (!w)
        return QString();

    // Wayland apps name their .desktop file in the app id (Qt: desktopFileName,
    // GTK: the application id)
    const QString appId = w->appId();
    if (!appId.isEmpty()) {
        for (const QString &name : {appId, appId.toLower()}) {
            const QString path = QStandardPaths::locate(QStandardPaths::ApplicationsLocation,
                                                        name + QStringLiteral(".desktop"));
            if (!path.isEmpty())
                return path;
        }
    }

    // Otherwise the same matching as on X11 (command line, StartupWMClass, ...)
    const QString className = w->resourceName().isEmpty() ? appId : w->resourceName();
    return Utils::instance()->desktopPathFromMetadata(appId, w->pid(), className);
}

void WaylandWindowInterface::setIconGeometry(quint64 wid, const QRect &rect)
{
    PlasmaWindow *w = window(wid);
    if (!w)
        return;

    // Relative to the dock's surface
    for (QWindow *panel : QGuiApplication::topLevelWindows()) {
        if (!panel->inherits("MainWindow") || !panel->isVisible())
            continue;
        Surface *surface = Surface::fromWindow(panel);
        if (!surface)
            return;
        w->setMinimizedGeometry(surface, rect.translated(-panel->geometry().topLeft()));
        return;
    }
}

QString WaylandWindowInterface::windowTitle(quint64 wid)
{
    PlasmaWindow *w = window(wid);
    return w ? w->title() : QString();
}

bool WaylandWindowInterface::isMinimized(quint64 wid)
{
    PlasmaWindow *w = window(wid);
    return w && w->isMinimized();
}

bool WaylandWindowInterface::isOnCurrentDesktop(quint64 wid)
{
    // KWin's window list has no "current desktop" of its own: windows on all
    // desktops, or on any, count as here (virtual desktops come later)
    Q_UNUSED(wid)
    return true;
}

bool WaylandWindowInterface::existsWindowMaximized()
{
    for (auto it = m_windows.cbegin(); it != m_windows.cend(); ++it) {
        PlasmaWindow *w = it.value();
        if (!w || !w->isValid() || w->isMinimized() || w->skipTaskbar())
            continue;
        if (w->isMaximized())
            return true;
    }
    return false;
}
