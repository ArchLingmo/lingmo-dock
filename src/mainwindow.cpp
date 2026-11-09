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

#include "mainwindow.h"
#include "processprovider.h"
#include "dockadaptor.h"

#include <QGuiApplication>
#include <QScreen>

#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQuickItem>
#include <QMetaEnum>
#include <QLibraryInfo>

#include <NETWM>
#include <KWindowSystem>
#include <KWindowEffects>
#include <KX11Extras>

#include <QtMath>
#include <xcb/xcb.h>
#include <xcb/shape.h>

#include <LayerShellQt/Window>

MainWindow::MainWindow(QQuickView *parent)
    : QQuickView(parent)
    , m_activity(Activity::self())
    , m_settings(DockSettings::self())
    , m_appModel(new ApplicationModel)
    , m_fakeWindow(nullptr)
    , m_trashManager(new TrashManager)
    , m_windowPreviews(new WindowPreviewModel(m_appModel, this))
    , m_hideBlocked(false)
    , m_previewVisible(false)
    , m_showTimer(new QTimer(this))
    , m_hideTimer(new QTimer(this))
{
    new DockAdaptor(this);

    installEventFilter(this);

    setDefaultAlphaBuffer(false);
    setColor(Qt::transparent);

    setFlags(Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    if (KWindowSystem::isPlatformWayland()) {
        // A panel in the layer above the windows; anchors and margins follow the
        // dock's edge (applyGeometry), the reserved room the visibility setting
        m_layerShell = LayerShellQt::Window::get(this);
        m_layerShell->setScope(QStringLiteral("lingmo-dock"));
        m_layerShell->setLayer(LayerShellQt::Window::LayerTop);
        m_layerShell->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
        m_layerShell->setActivateOnShow(false);
        m_layerShell->setScreen(qApp->primaryScreen());
    } else {
        // KWindowSystem::setOnDesktop(winId(), NET::OnAllDesktops);
        KX11Extras::setType(winId(), NET::Dock);
    }

    engine()->rootContext()->setContextProperty("appModel", m_appModel);
    engine()->rootContext()->setContextProperty("process", new ProcessProvider);
    engine()->rootContext()->setContextProperty("Settings", m_settings);
    engine()->rootContext()->setContextProperty("mainWindow", this);
    engine()->rootContext()->setContextProperty("trash", m_trashManager);
    engine()->rootContext()->setContextProperty("windowPreviews", m_windowPreviews);
    engine()->addImageProvider("windowthumbnail", new WindowThumbnailProvider(m_windowPreviews->store()));


    setSource(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    setScreen(qApp->primaryScreen());
    setResizeMode(QQuickView::SizeRootObjectToView);
    initScreens();

    initSlideWindow();
    resizeWindow();
    onVisibilityChanged();

    m_showTimer->setSingleShot(true);
    m_showTimer->setInterval(200);
    connect(m_showTimer, &QTimer::timeout, this, [=] { setVisible(true); });

    m_hideTimer->setSingleShot(true);
    m_hideTimer->setInterval(500);
    connect(m_hideTimer, &QTimer::timeout, this, &MainWindow::onHideTimeout);

    // When the current window changes.
    connect(m_activity, &Activity::launchPadChanged, this, &MainWindow::onVisibilityChanged);
    connect(m_activity, &Activity::existsWindowMaximizedChanged, this, &MainWindow::onVisibilityChanged);

    // Always on the primary screen. Monitors moved, rotated, plugged in or made primary
    // change several screens at once, and KWin may move the dock while they settle
    // (which makes Qt think it now lives on another screen), so every such change just
    // schedules one look at where the primary screen is now.
    m_relayout = new QTimer(this);
    m_relayout->setSingleShot(true);
    m_relayout->setInterval(250);
    connect(m_relayout, &QTimer::timeout, this, &MainWindow::followPrimaryScreen);
    auto watch = [this](QScreen *screen) {
        connect(screen, &QScreen::geometryChanged, m_relayout, qOverload<>(&QTimer::start));
        connect(screen, &QScreen::virtualGeometryChanged, m_relayout, qOverload<>(&QTimer::start));
    };
    for (QScreen *screen : qGuiApp->screens())
        watch(screen);
    connect(qGuiApp, &QGuiApplication::screenAdded, this, [this, watch](QScreen *screen) {
        watch(screen);
        m_relayout->start();
    });
    connect(qGuiApp, &QGuiApplication::screenRemoved, m_relayout, qOverload<>(&QTimer::start));
    connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, &MainWindow::onPrimaryScreenChanged);
    connect(this, &QWindow::screenChanged, m_relayout, qOverload<>(&QTimer::start));

    connect(m_appModel, &ApplicationModel::countChanged, this, &MainWindow::resizeWindow);
    connect(m_settings, &DockSettings::directionChanged, this, &MainWindow::onPositionChanged);
    connect(m_settings, &DockSettings::iconSizeChanged, this, &MainWindow::onIconSizeChanged);
    connect(m_settings, &DockSettings::visibilityChanged, this, &MainWindow::onVisibilityChanged);
    connect(m_settings, &DockSettings::styleChanged, this, &MainWindow::resizeWindow);
    connect(m_settings, &DockSettings::backgroundChanged, this, &MainWindow::backgroundChanged);
    connect(m_settings, &DockSettings::magnificationChanged, this, &MainWindow::magnificationChanged);
    connect(m_settings, &DockSettings::magnificationSizeChanged, this, &MainWindow::magnificationSizeChanged);
    // Bigger icons need more room above the panel
    connect(m_settings, &DockSettings::magnificationChanged, this, &MainWindow::resizeWindow);
    connect(m_settings, &DockSettings::magnificationSizeChanged, this, &MainWindow::resizeWindow);

    // The primary screen may have changed while the QML above was loading: the session
    // runs autostart entries (xrandr layouts, ...) as soon as the desktop is up
    followPrimaryScreen();
}

MainWindow::~MainWindow()
{
}

void MainWindow::add(const QString &desktop)
{
    m_appModel->addItem(desktop);
}

void MainWindow::remove(const QString &desktop)
{
    m_appModel->removeItem(desktop);
}

bool MainWindow::pinned(const QString &desktop)
{
    return m_appModel->isDesktopPinned(desktop);
}

QRect MainWindow::primaryGeometry() const
{
    // The panel, without the see-through room for magnified icons
    return m_panelRect.isValid() ? m_panelRect : geometry();
}

int MainWindow::direction() const
{
    return DockSettings::self()->direction();
}

int MainWindow::visibility() const
{
    return DockSettings::self()->visibility();
}

void MainWindow::setDirection(int direction)
{
    DockSettings::self()->setDirection(static_cast<DockSettings::Direction>(direction));
}

void MainWindow::setIconSize(int iconSize)
{
    DockSettings::self()->setIconSize(iconSize);
}

void MainWindow::setVisibility(int visibility)
{
    DockSettings::self()->setVisibility(static_cast<DockSettings::Visibility>(visibility));
}

int MainWindow::style() const
{
    return DockSettings::self()->style();
}

void MainWindow::setStyle(int style)
{
    DockSettings::self()->setStyle(static_cast<DockSettings::Style>(style));
}

int MainWindow::background() const
{
    return DockSettings::self()->background();
}

void MainWindow::setBackground(int background)
{
    DockSettings::self()->setBackground(static_cast<DockSettings::Background>(background));
}

bool MainWindow::magnification() const
{
    return DockSettings::self()->magnification();
}

void MainWindow::setMagnification(bool enabled)
{
    DockSettings::self()->setMagnification(enabled);
}

int MainWindow::magnificationSize() const
{
    return DockSettings::self()->magnificationSize();
}

void MainWindow::setMagnificationSize(int percent)
{
    DockSettings::self()->setMagnificationSize(percent);
}

void MainWindow::updateSize()
{
    resizeWindow();
}

void MainWindow::setPreviewVisible(bool visible)
{
    if (m_previewVisible == visible)
        return;

    m_previewVisible = visible;

    if (visible) {
        m_hideTimer->stop();
    } else if (m_fakeWindow && !m_hideBlocked) {
        // The pointer left through the popup, not the dock
        m_hideTimer->start();
    }
}

QRect MainWindow::windowRect() const
{
    // The primary screen's, not screen(): Qt moves that to whichever screen the window
    // happens to be over
    QScreen *primary = qApp->primaryScreen() ? qApp->primaryScreen() : screen();
    const QRect screenGeometry = primary->geometry();
    const QRect availableGeometry = primary->availableGeometry();

    bool isHorizontal = m_settings->direction() == DockSettings::Bottom;
    bool compositing = false;
    QQuickItem *item = qobject_cast<QQuickItem *>(rootObject());

    if (item) {
        compositing = item->property("compositing").toBool();
    }

    QSize newSize(0, 0);
    QPoint position(0, 0);
    int maxLength = isHorizontal ? screenGeometry.width() - m_settings->edgeMargins()
                                 : availableGeometry.height() - m_settings->edgeMargins();;

    // Add trash item.
    int appCount = m_appModel->rowCount() + 1;
    int iconSize = m_settings->iconSize();
    iconSize += iconSize * 0.1;
    int length = appCount * iconSize;
    int margins = compositing ? DockSettings::self()->edgeMargins() / 2 : 0;

    if (length >= maxLength) {
        iconSize = (maxLength - (maxLength % appCount)) / appCount;
        length = appCount * iconSize;
    }

    switch (m_settings->style()) {
    case DockSettings::Round: {
        switch (m_settings->direction()) {
        case DockSettings::Left:
            newSize = QSize(iconSize, length);
            position.setX(screenGeometry.x() + margins);
            // Handle the top statusbar.
            position.setY(availableGeometry.y() + (availableGeometry.height() - newSize.height()) / 2);
            break;
        case DockSettings::Bottom:
            newSize = QSize(length, iconSize);
            position.setX(screenGeometry.x() + (screenGeometry.width() - newSize.width()) / 2);
            position.setY(screenGeometry.y() + screenGeometry.height() - newSize.height() - margins);
            break;
        case DockSettings::Right:
            newSize = QSize(iconSize, length);
            position.setX(screenGeometry.x() + screenGeometry.width() - newSize.width() - margins);
            position.setY(availableGeometry.y() + (availableGeometry.height() - newSize.height()) / 2);
            break;
        default:
            break;
        }

        break;
    }
    case DockSettings::Straight: {
        switch (m_settings->direction()) {
        case DockSettings::Left:
            newSize = QSize(iconSize, screenGeometry.height());
            position.setX(screenGeometry.x());
            position.setY(screenGeometry.y());
            break;
        case DockSettings::Bottom:
            newSize = QSize(screenGeometry.width(), iconSize);
            position.setX(screenGeometry.x());
            position.setY(screenGeometry.y() + screenGeometry.height() - newSize.height());
            break;
        case DockSettings::Right:
            newSize = QSize(iconSize, screenGeometry.height());
            position.setX(screenGeometry.x() + screenGeometry.width() - newSize.width());
            position.setY(screenGeometry.y() + screenGeometry.height() - newSize.height());
            break;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }

    return QRect(position, newSize);
}

int MainWindow::extentFor(const QRect &panel) const
{
    QQuickItem *item = qobject_cast<QQuickItem *>(rootObject());
    const bool compositing = item && item->property("compositing").toBool();
    // Without a compositor the room would be painted black: the icons then
    // only grow inside the panel (see DockItem.qml)
    if (!m_settings->magnification() || !compositing)
        return 0;

    // DockItem: the icon is 0.8 of the cell and grows from the edge of the cell
    // nearest the screen edge, 0.1 cell away from it
    const int cell = m_settings->direction() == DockSettings::Bottom ? panel.height() : panel.width();
    const qreal zoom = m_settings->magnificationSize() / 100.0;
    return qMax(0, qCeil(cell * (0.8 * zoom - 0.9))) + 2;
}

void MainWindow::applyGeometry()
{
    const QRect panel = windowRect();
    const int extent = extentFor(panel);
    QRect rect = panel;

    switch (m_settings->direction()) {
    case DockSettings::Left:
        rect.setRight(rect.right() + extent);
        break;
    case DockSettings::Right:
        rect.setLeft(rect.left() - extent);
        break;
    case DockSettings::Bottom:
    default:
        rect.setTop(rect.top() - extent);
        break;
    }

    m_panelRect = panel;
    m_windowRect = rect;
    if (m_magnificationExtent != extent) {
        m_magnificationExtent = extent;
        emit magnificationExtentChanged();
    }
    setGeometry(rect);
    updateLayerPlacement(rect);
    updateInputShape();
}

void MainWindow::updateLayerPlacement(const QRect &rect)
{
    if (!m_layerShell)
        return;

    // KWin places a layer surface from its anchors and margins: the edge the dock
    // sits on, and the gap to that edge (windowRect() centres it along the edge,
    // and so does KWin within the room the other panels leave)
    using LS = LayerShellQt::Window;
    QScreen *primary = qApp->primaryScreen() ? qApp->primaryScreen() : screen();
    const QRect s = primary->geometry();
    const bool straight = m_settings->style() == DockSettings::Straight;
    LS::Anchors anchors;
    QMargins margins;

    switch (m_settings->direction()) {
    case DockSettings::Left:
        anchors = LS::AnchorLeft;
        if (straight)
            anchors |= LS::Anchors(LS::AnchorTop | LS::AnchorBottom);
        margins.setLeft(qMax(0, rect.left() - s.left()));
        break;
    case DockSettings::Right:
        anchors = LS::AnchorRight;
        if (straight)
            anchors |= LS::Anchors(LS::AnchorTop | LS::AnchorBottom);
        margins.setRight(qMax(0, s.right() - rect.right()));
        break;
    case DockSettings::Bottom:
    default:
        anchors = LS::AnchorBottom;
        margins.setBottom(qMax(0, s.bottom() - rect.bottom()));
        break;
    }

    m_layerShell->setAnchors(anchors);
    m_layerShell->setMargins(margins);
}

void MainWindow::updateInputShape()
{
    if (m_layerShell) {
        // Wayland: the mask is the surface's input region. From the rect we asked for:
        // a layer surface's geometry() doesn't hold its place on screen (KWin puts it
        // from the anchors), and a mask off by that misses every click
        if (m_magnificationExtent == 0)
            setMask(QRegion());
        else
            setMask(QRegion(QRect(m_panelRect.topLeft() - m_windowRect.topLeft(), m_panelRect.size())));
        return;
    }
    if (!KWindowSystem::isPlatformX11())
        return;
    auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    xcb_connection_t *c = x11 ? x11->connection() : nullptr;
    if (!c)
        return;

    const xcb_window_t win = winId();
    if (m_magnificationExtent == 0) {
        // Back to the default: the whole window takes input
        xcb_shape_mask(c, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_INPUT, win, 0, 0, XCB_PIXMAP_NONE);
    } else {
        // X11 works in device pixels
        const qreal dpr = devicePixelRatio();
        const QRect local(m_panelRect.topLeft() - geometry().topLeft(), m_panelRect.size());
        xcb_rectangle_t r;
        r.x = qRound(local.x() * dpr);
        r.y = qRound(local.y() * dpr);
        r.width = qRound(local.width() * dpr);
        r.height = qRound(local.height() * dpr);
        xcb_shape_rectangles(c, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_INPUT, XCB_CLIP_ORDERING_UNSORTED,
                             win, 0, 0, 1, &r);
    }
    xcb_flush(c);
}

void MainWindow::resizeWindow()
{
    applyGeometry();
    updateViewStruts();

    emit resizingFished();
}

void MainWindow::initScreens()
{
    switch (m_settings->direction()) {
// TODO
//    case DockSettings::Left:
//        setScreen(qGuiApp->screens().first());
//        break;
//    case DockSettings::Right:
//        setScreen(qGuiApp->screens().last());
//        break;
//    case DockSettings::Bottom:
//        setScreen(qGuiApp->primaryScreen());
//        break;
    default:
        setScreen(qGuiApp->primaryScreen());
        break;
    }

    if (m_fakeWindow) {
        m_fakeWindow->setScreen(screen());
        m_fakeWindow->updateGeometry();
    }
}

void MainWindow::initSlideWindow()
{
    KWindowEffects::SlideFromLocation location = KWindowEffects::NoEdge;

    if (m_settings->direction() == DockSettings::Left)
        location = KWindowEffects::LeftEdge;
    else if (m_settings->direction() == DockSettings::Right)
        location = KWindowEffects::RightEdge;
    else if (m_settings->direction() == DockSettings::Bottom)
        location = KWindowEffects::BottomEdge;

    // Wayland has no foreign windows: the dock's own QWindow
    KWindowEffects::slideWindow(m_layerShell ? static_cast<QWindow *>(this) : fromWinId(winId()), location);
}

void MainWindow::updateViewStruts()
{
    bool compositing = false;
    QQuickItem *item = qobject_cast<QQuickItem *>(rootObject());

    if (item) {
        compositing = item->property("compositing").toBool();
    }

    if (m_settings->visibility() == DockSettings::AlwaysShow || m_activity->launchPad()) {
        WindowInterface::instance()->setViewStruts(this, m_settings->direction(),
                                                    m_panelRect.isValid() ? m_panelRect : geometry(), compositing);
    } else {
        clearViewStruts();
    }
}

void MainWindow::clearViewStruts()
{
    WindowInterface::instance()->clearViewStruts(this);
}

void MainWindow::createFakeWindow()
{
    if (!m_fakeWindow) {
        m_fakeWindow = new FakeWindow;
        m_fakeWindow->setScreen(screen());
        m_fakeWindow->updateGeometry();

        connect(m_fakeWindow, &FakeWindow::containsMouseChanged, this, [=](bool contains) {
            switch (m_settings->visibility()) {
            case DockSettings::AlwaysHide:
            case DockSettings::IntellHide:{
                if (contains) {
                    m_hideTimer->stop();

                    // reionwong: The mouse is moved to fakewindow,
                    // if the dock is not displayed,
                    // it will start to display.
                    if (!isVisible() && !m_showTimer->isActive()) {
                        m_showTimer->start();
                    }
                } else {
                    if (!m_hideBlocked)
                        m_hideTimer->start();
                }

                break;
            }
            default:
                break;
            }
        });
    }
}

void MainWindow::deleteFakeWindow()
{
    if (m_fakeWindow) {
        // removeEventFilter(this);
        disconnect(m_fakeWindow);
        m_fakeWindow->deleteLater();
        m_fakeWindow = nullptr;
    }
}

void MainWindow::onPrimaryScreenChanged(QScreen *)
{
    m_relayout->start();
}

void MainWindow::followPrimaryScreen()
{
    QScreen *primary = qApp->primaryScreen();
    if (!primary)
        return;
    if (m_layerShell && m_layerShell->screen() != primary) {
        // The output of a layer surface is fixed when it is created
        const bool visible = isVisible();
        setVisible(false);
        setScreen(primary);
        m_layerShell->setScreen(primary);
        setVisible(visible);
    }
    initScreens();
    if (screen() != primary)
        setScreen(primary);
    resizeWindow();

    // The strip along the screen edge that brings a hidden dock back
    if (m_fakeWindow) {
        m_fakeWindow->setScreen(primary);
        m_fakeWindow->updateGeometry();
    }
}

void MainWindow::onPositionChanged()
{
    initScreens();

    if (m_settings->visibility() == DockSettings::AlwaysHide ||
            m_settings->visibility() == DockSettings::IntellHide) {
        setVisible(false);
        initSlideWindow();
        // Setting geometry needs to be displayed, otherwise it will be invalid.
        setVisible(true);
        applyGeometry();
        updateViewStruts();

        m_hideTimer->start();
    } else if (m_settings->visibility() == DockSettings::AlwaysShow) {
        setVisible(false);
        initSlideWindow();
        setVisible(true);
        applyGeometry();
        updateViewStruts();
    }

    emit directionChanged();
}

void MainWindow::onIconSizeChanged()
{
    applyGeometry();
    updateViewStruts();

    emit iconSizeChanged();
}

void MainWindow::onVisibilityChanged()
{
    emit visibilityChanged();

    if (m_activity->launchPad()) {
        m_hideTimer->stop();
        clearViewStruts();
        setVisible(true);
        return;
    }

    // Always show
    // Must remain displayed when launchpad is opened.
    if (m_settings->visibility() == DockSettings::AlwaysShow) {
        m_hideTimer->stop();

        applyGeometry();
        setVisible(true);
        updateViewStruts();

        // Delete fakewindow
        if (m_fakeWindow) {
            deleteFakeWindow();
        }
    }

    if (m_settings->visibility() == DockSettings::IntellHide) {
        clearViewStruts();
        applyGeometry();

        if (m_activity->existsWindowMaximized() && !m_hideBlocked && !m_previewVisible) {
            setVisible(false);
        } else {
            setVisible(true);
        }

        if (!m_fakeWindow)
            createFakeWindow();
    }

    // Always hide
    if (m_settings->visibility() == DockSettings::AlwaysHide) {
        clearViewStruts();
        applyGeometry();
        setVisible(m_hideBlocked);

        // Create
        if (!m_fakeWindow)
            createFakeWindow();
    }
}

void MainWindow::onHideTimeout()
{
    if (m_activity->launchPad() || m_previewVisible)
        return;

    if (m_settings->visibility() == DockSettings::IntellHide
            && !m_activity->existsWindowMaximized()) {
        return;
    }

    setVisible(false);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *e)
{
    switch (e->type()) {
    case QEvent::Enter:
        if (m_fakeWindow)
            m_hideTimer->stop();
        m_hideBlocked = true;
        break;
    case QEvent::Leave:
        if (m_fakeWindow && !m_previewVisible)
            m_hideTimer->start();
        m_hideBlocked = false;
        break;
    case QEvent::DragEnter:
    case QEvent::DragMove:
        if (m_fakeWindow)
            m_hideTimer->stop();
        break;
    case QEvent::DragLeave:
    case QEvent::Drop:
        if (m_fakeWindow)
            m_hideTimer->stop();
        break;
    default:
        break;
    }

    return QQuickView::eventFilter(obj, e);
}

void MainWindow::resizeEvent(QResizeEvent *e)
{
    emit primaryGeometryChanged();

    QQuickView::resizeEvent(e);
}
