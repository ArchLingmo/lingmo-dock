/*
 * Copyright (C) 2021 LingmoOS Team.
 *
 * Author:     Reion Wong <reionwong@gmail.com>
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QQuickView>
#include <QTimer>

#include "activity.h"
#include "docksettings.h"
#include "applicationmodel.h"
#include "fakewindow.h"
#include "trashmanager.h"
#include "windowpreviewmodel.h"

namespace LayerShellQt {
class Window;
}

class MainWindow : public QQuickView
{
    Q_OBJECT
    Q_PROPERTY(QRect primaryGeometry READ primaryGeometry NOTIFY primaryGeometryChanged)
    Q_PROPERTY(int direction READ direction NOTIFY directionChanged)
    Q_PROPERTY(int visibility READ visibility NOTIFY visibilityChanged)
    Q_PROPERTY(int style READ style NOTIFY styleChanged)
    Q_PROPERTY(int background READ background NOTIFY backgroundChanged)
    Q_PROPERTY(bool magnification READ magnification NOTIFY magnificationChanged)
    Q_PROPERTY(int magnificationSize READ magnificationSize NOTIFY magnificationSizeChanged)
    // Room above (or beside) the dock's panel for the magnified icons, in px
    Q_PROPERTY(int magnificationExtent READ magnificationExtent NOTIFY magnificationExtentChanged)

public:
    explicit MainWindow(QQuickView *parent = nullptr);
    ~MainWindow();

    // DBus interface
    void add(const QString &desktop);
    void remove(const QString &desktop);
    bool pinned(const QString &desktop);

    QRect primaryGeometry() const;
    int direction() const;

    int visibility() const;

    void setDirection(int direction);
    void setIconSize(int iconSize);
    void setVisibility(int visibility);

    int style() const;
    void setStyle(int style);

    // 0 solid, 1 glass, 2 no background (Lingmo Settings › Dock)
    int background() const;
    void setBackground(int background);

    // Magnify icons on hover (Lingmo Settings › Dock)
    bool magnification() const;
    void setMagnification(bool enabled);
    int magnificationSize() const;
    void setMagnificationSize(int percent);
    int magnificationExtent() const { return m_magnificationExtent; }

    Q_INVOKABLE void updateSize();

    // The window preview popup is open: an auto-hiding dock stays up meanwhile
    Q_INVOKABLE void setPreviewVisible(bool visible);

signals:
    void resizingFished();
    void iconSizeChanged();
    void directionChanged();
    void primaryGeometryChanged();
    void visibilityChanged();
    void styleChanged();
    void backgroundChanged();
    void magnificationChanged();
    void magnificationSizeChanged();
    void magnificationExtentChanged();

private:
    // The panel itself; the window adds magnificationExtent on the screen side
    QRect windowRect() const;
    int extentFor(const QRect &panel) const;
    // Moves/resizes the window to the panel plus the magnification room
    void applyGeometry();
    // Only the panel takes clicks: the room above it stays click-through
    void updateInputShape();
    // Wayland: anchors and margins of the layer surface for `rect`
    void updateLayerPlacement(const QRect &rect);
    void resizeWindow();
    void initScreens();
    void initSlideWindow();
    void updateViewStruts();
    void clearViewStruts();

    void createFakeWindow();
    void deleteFakeWindow();

private slots:
    void onPrimaryScreenChanged(QScreen *screen);
    // Back on the primary screen, wherever it is now
    void followPrimaryScreen();
    void onPositionChanged();
    void onIconSizeChanged();
    void onVisibilityChanged();

    void onHideTimeout();

protected:
    bool eventFilter(QObject *obj, QEvent *e) override;
    void resizeEvent(QResizeEvent *) override;

private:
    Activity *m_activity;
    DockSettings *m_settings;
    ApplicationModel *m_appModel;
    FakeWindow *m_fakeWindow;
    TrashManager *m_trashManager;
    WindowPreviewModel *m_windowPreviews;

    bool m_hideBlocked;
    bool m_previewVisible;

    QTimer *m_showTimer;
    QTimer *m_relayout = nullptr;
    QTimer *m_hideTimer;

    QRect m_panelRect;
    // The whole window (panel + room for magnified icons), as last requested
    QRect m_windowRect;
    int m_magnificationExtent = 0;
    // Wayland: the dock's layer surface (null on X11)
    LayerShellQt::Window *m_layerShell = nullptr;
};

#endif // MAINWINDOW_H
