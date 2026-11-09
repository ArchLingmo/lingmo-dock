/*
 * SPDX-FileCopyrightText: 2026 LingmoOS Team
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef WAYLANDWINDOWINTERFACE_H
#define WAYLANDWINDOWINTERFACE_H

#include "windowinterface.h"

#include <QHash>
#include <QPointer>
#include <QSet>

namespace KWayland {
namespace Client {
class PlasmaWindow;
class PlasmaWindowManagement;
class Registry;
}
}

// Wayland session: the windows come from KWin's org_kde_plasma_window_management,
// which KWin only offers to programs whose .desktop file asks for it
// (X-KDE-Wayland-Interfaces in lingmo-dock.desktop). No thumbnails yet: the
// preview popup shows the app icon.
class WaylandWindowInterface : public WindowInterface
{
    Q_OBJECT

public:
    explicit WaylandWindowInterface(QObject *parent = nullptr);

    WId activeWindow() override;
    void minimizeWindow(WId win) override;
    void closeWindow(WId id) override;
    void forceActiveWindow(WId win) override;

    QMap<QString, QVariant> requestInfo(quint64 wid) override;
    QString requestWindowClass(quint64 wid) override;
    bool isAcceptableWindow(quint64 wid) override;

    void setViewStruts(QWindow *view, DockSettings::Direction direction, const QRect &rect, bool compositing = false) override;
    void clearViewStruts(QWindow *view) override;

    void startInitWindows() override;

    QString desktopFilePath(quint64 wid) override;

    void setIconGeometry(quint64 wid, const QRect &rect) override;

    QString windowTitle(quint64 wid) override;
    bool isMinimized(quint64 wid) override;
    bool isOnCurrentDesktop(quint64 wid) override;
    bool existsWindowMaximized() override;

private:
    void setManagement(KWayland::Client::PlasmaWindowManagement *wm);
    void onWindowCreated(KWayland::Client::PlasmaWindow *window);
    // Adds or removes the window from the dock after a state change
    void evaluate(quint64 wid);
    void removeWindow(quint64 wid);
    KWayland::Client::PlasmaWindow *window(quint64 wid) const;

    KWayland::Client::Registry *m_registry = nullptr;
    KWayland::Client::PlasmaWindowManagement *m_wm = nullptr;
    QHash<quint64, QPointer<KWayland::Client::PlasmaWindow>> m_windows;
    QHash<KWayland::Client::PlasmaWindow *, quint64> m_ids;
    quint64 m_nextId = 1;
    // The app id a shown window was added with (a new id regroups it)
    QHash<quint64, QString> m_shownWindows;
    quint64 m_active = 0;
    // startInitWindows() ran: the application model listens from then on
    bool m_started = false;
};

#endif // WAYLANDWINDOWINTERFACE_H
