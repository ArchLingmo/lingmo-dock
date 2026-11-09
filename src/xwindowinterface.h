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

#ifndef XWINDOWINTERFACE_H
#define XWINDOWINTERFACE_H

#include "applicationitem.h"
#include "docksettings.h"
#include "windowinterface.h"
#include <QObject>
#include <QSet>

// KLIB
#include <KWindowInfo>
#include <KWindowEffects>

// X11 session
class XWindowInterface : public WindowInterface
{
    Q_OBJECT

public:
    explicit XWindowInterface(QObject *parent = nullptr);

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

private:
    void onWindowadded(quint64 wid);
    void onWindowRemoved(quint64 wid);
    void onWindowChanged(WId wid, NET::Properties properties, NET::Properties2 properties2);

    // Windows the dock shows, so a window can come and go when its state changes
    QSet<quint64> m_shownWindows;
};

#endif // XWINDOWINTERFACE_H
