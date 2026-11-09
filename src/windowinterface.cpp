/*
 * SPDX-FileCopyrightText: 2026 LingmoOS Team
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "windowinterface.h"
#include "waylandwindowinterface.h"
#include "xwindowinterface.h"

#include <KWindowEffects>
#include <KWindowSystem>

WindowInterface::WindowInterface(QObject *parent)
    : QObject(parent)
{
}

WindowInterface *WindowInterface::instance()
{
    static WindowInterface *instance = nullptr;
    if (!instance) {
        if (KWindowSystem::isPlatformWayland())
            instance = new WaylandWindowInterface;
        else
            instance = new XWindowInterface;
    }
    return instance;
}

void WindowInterface::enableBlurBehind(QWindow *view, bool enable, const QRegion &region)
{
    KWindowEffects::enableBlurBehind(view, enable, region);
}
