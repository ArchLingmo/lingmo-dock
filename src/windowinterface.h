/*
 * SPDX-FileCopyrightText: 2026 LingmoOS Team
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef WINDOWINTERFACE_H
#define WINDOWINTERFACE_H

#include "docksettings.h"

#include <QMap>
#include <QObject>
#include <QRect>
#include <QRegion>
#include <QVariant>
#include <QWindow>

// The windows the dock shows and what it does with them. One implementation per
// session, same API: X11 (xwindowinterface.cpp, KX11Extras/NETWM) and Wayland
// (waylandwindowinterface.cpp, org_kde_plasma_window_management). A window is a
// quint64: the X11 window id, or a number the Wayland interface hands out.
class WindowInterface : public QObject
{
    Q_OBJECT

public:
    static WindowInterface *instance();

    void enableBlurBehind(QWindow *view, bool enable = true, const QRegion &region = QRegion());

    virtual WId activeWindow() = 0;
    virtual void minimizeWindow(WId win) = 0;
    virtual void closeWindow(WId id) = 0;
    virtual void forceActiveWindow(WId win) = 0;

    // iconName, active, visibleName, id (X11: WM_CLASS class; Wayland: app id)
    virtual QMap<QString, QVariant> requestInfo(quint64 wid) = 0;
    virtual QString requestWindowClass(quint64 wid) = 0;
    virtual bool isAcceptableWindow(quint64 wid) = 0;

    // Room the dock keeps for itself (X11 strut, Wayland exclusive zone)
    virtual void setViewStruts(QWindow *view, DockSettings::Direction direction, const QRect &rect, bool compositing = false) = 0;
    virtual void clearViewStruts(QWindow *view) = 0;

    virtual void startInitWindows() = 0;

    virtual QString desktopFilePath(quint64 wid) = 0;

    // Where the window minimizes to (the dock item), in global coordinates
    virtual void setIconGeometry(quint64 wid, const QRect &rect) = 0;

    // Window state for the previews and the auto-hide, on Wayland (the X11 code
    // reads KWindowInfo itself, as before)
    virtual QString windowTitle(quint64 wid) { Q_UNUSED(wid) return QString(); }
    virtual bool isMinimized(quint64 wid) { Q_UNUSED(wid) return false; }
    virtual bool isOnCurrentDesktop(quint64 wid) { Q_UNUSED(wid) return true; }
    virtual bool existsWindowMaximized() { return false; }

signals:
    void windowAdded(quint64 wid);
    void windowRemoved(quint64 wid);
    void activeChanged(quint64 wid);
    // Title, minimized, maximized or desktop of a window changed (Wayland)
    void windowChanged(quint64 wid);

protected:
    explicit WindowInterface(QObject *parent = nullptr);
};

#endif // WINDOWINTERFACE_H
