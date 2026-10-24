import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12
import Qt5Compat.GraphicalEffects

import Lingmo.Dock 1.0
import LingmoUI.CompatibleModule 3.0 as LingmoUI

// The window is the dock's panel plus, when magnification needs it, a
// see-through strip on the screen side for the enlarged icons (input goes
// to the panel only, see MainWindow::updateInputShape)
Item {
    id: _frame
    visible: true

    // MainWindow reads it to size the window
    readonly property alias compositing: root.compositing

    Item {
        id: root
        x: Settings.direction === DockSettings.Right ? mainWindow.magnificationExtent : 0
        y: Settings.direction === DockSettings.Bottom ? mainWindow.magnificationExtent : 0
        width: _frame.width - (isHorizontal ? 0 : mainWindow.magnificationExtent)
        height: _frame.height - (isHorizontal ? mainWindow.magnificationExtent : 0)

        property bool isHorizontal: Settings.direction === DockSettings.Bottom
        property real windowRadius: isHorizontal ? root.height * 0.35 : root.width * 0.35
        property bool compositing: windowHelper.compositing
        // Settings › Dock › Background. Glass and none need a compositor (a see-through
        // window is black without one), so they fall back to the solid panel.
        readonly property bool glassBackground: Settings.background === DockSettings.Glass && windowHelper.compositing
        readonly property bool noBackground: Settings.background === DockSettings.NoBackground && windowHelper.compositing
        readonly property color panelColor: LingmoUI.Theme.darkMode ? "#2A2A2A" : "#DCDCDC"

        // Pointer position along the dock (-1 when outside), for the magnification
        readonly property real hoverPos: Settings.magnification && _dockHover.hovered
                                         ? (isHorizontal ? _dockHover.point.position.x : _dockHover.point.position.y)
                                         : -1

        HoverHandler {
            id: _dockHover
            enabled: Settings.magnification
        }

        property bool roundedEnabled: Settings.roundedWindowEnabled
                                     && Settings.style === DockSettings.Round

        onCompositingChanged: {
            mainWindow.updateSize()
        }

        DropArea {
            anchors.fill: parent
            enabled: true
        }

        readonly property real panelRadius: root.roundedEnabled ? root.windowRadius : 0

        // Background: the solid panel (Solid, and Glass without a compositor)
        Rectangle {
            id: _background

            property var borderColor: root.compositing ? LingmoUI.Theme.darkMode ? Qt.rgba(255, 255, 255, 0.3)
                                                                               : Qt.rgba(0, 0, 0, 0.2) : LingmoUI.Theme.darkMode ? Qt.rgba(255, 255, 255, 0.15)
                                                                                                                               : Qt.rgba(0, 0, 0, 0.15)

            anchors.fill: parent
            visible: !root.noBackground && !root.glassBackground
            radius: root.panelRadius
            opacity: windowHelper.compositing ? LingmoUI.Theme.darkMode ? 0.65 : 0.75
                                              : root.roundedEnabled ? LingmoUI.Theme.darkMode ? 0.9 : 0.95
                                                               : 1
            color: root.panelColor
            border.width: 1 / LingmoUI.Units.devicePixelRatio
            border.pixelAligned: LingmoUI.Units.devicePixelRatio > 1 ? false : true
            border.color: borderColor

            Behavior on color {
                ColorAnimation {
                    duration: 200
                    easing.type: Easing.Linear
                }
            }
        }

        // Glass: the "Vidro Lingmo" material (Settings > Appearance > Glass).
        // The window also holds the magnification strip, so the blur is asked
        // for the panel's rectangle only. With Glass off it is a solid panel.
        // Loaded only for Glass: a disabled WindowBlur clears the window's blur
        // region, which would take the solid panel's blur away
        Loader {
            anchors.fill: parent
            active: root.glassBackground
            sourceComponent: LingmoUI.GlassSurface {
                radius: root.panelRadius
                blurMode: LingmoUI.GlassSurface.Always
                blurWindow: mainWindow
            }
        }

        LingmoUI.WindowHelper {
            id: windowHelper
        }

        LingmoUI.WindowShadow {
            view: mainWindow
            geometry: Qt.rect(root.x, root.y, root.width, root.height)
            strength: root.noBackground ? 0 : 1
            radius: root.panelRadius
        }

        // Blur behind the panel (only the panel: the window also holds the
        // magnification strip). The glass asks for the same region.
        LingmoUI.WindowBlur {
            view: mainWindow
            geometry: Qt.rect(root.x, root.y, root.width, root.height)
            blurRect: Qt.rect(root.x, root.y, root.width, root.height)
            windowRadius: root.panelRadius
            enabled: !root.noBackground
        }

        LingmoUI.PopupTips {
            id: popupTips
            backgroundColor: root.panelColor
            blurEnabled: false
        }

        WindowPreview {
            id: windowPreview
            screenRect: Qt.rect(root.Screen.virtualX, root.Screen.virtualY,
                                root.Screen.width, root.Screen.height)
        }

        GridLayout {
            id: mainLayout
            anchors.fill: parent
            anchors.topMargin: Settings.style === 1
                               && (Settings.direction === 0 || Settings.direction === 2)
                               ? 28 : 0
            flow: root.isHorizontal ? Grid.LeftToRight : Grid.TopToBottom
            columnSpacing: 0
            rowSpacing: 0

            ListView {
                id: appItemView
                orientation: root.isHorizontal ? Qt.Horizontal : Qt.Vertical
                snapMode: ListView.SnapToItem
                interactive: false
                model: appModel
                // Magnified icons reach out of the panel into the window's spare room
                clip: mainWindow.magnificationExtent === 0

                Layout.fillHeight: true
                Layout.fillWidth: true

                delegate: AppItem {
                    id: appItemDelegate
                    implicitWidth: root.isHorizontal ? appItemView.height : appItemView.width
                    implicitHeight: root.isHorizontal ? appItemView.height : appItemView.width
                }

                moveDisplaced: Transition {
                    NumberAnimation {
                        properties: "x, y"
                        duration: 300
                        easing.type: Easing.InOutQuad
                    }
                }
            }

            DockItem {
                id: trashItem
                implicitWidth: root.isHorizontal ? root.height : root.width
                implicitHeight: root.isHorizontal ? root.height : root.width
                popupText: qsTr("Trash")
                enableActivateDot: false
                iconName: trash.count === 0 ? "user-trash" : "user-trash-full"
                onClicked: trash.openTrash()
                onRightClicked: trashMenu.popup()

                dropArea.enabled: true

                onDropped: {
                    if (drop.hasUrls) {
                        trash.moveToTrash(drop.urls)
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: LingmoUI.Units.smallSpacing / 2
                    color: "transparent"
                    border.color: LingmoUI.Theme.textColor
                    radius: height * 0.3
                    border.width: 1 / LingmoUI.Units.devicePixelRatio
                    border.pixelAligned: LingmoUI.Units.devicePixelRatio > 1 ? false : true
                    opacity: trashItem.dropArea.containsDrag ? 0.5 : 0

                    Behavior on opacity {
                        NumberAnimation {
                            duration: 200
                        }
                    }
                }

                LingmoUI.DesktopMenu {
                    id: trashMenu

                    MenuItem {
                        text: qsTr("Open")
                        icon.name: "folder-open"
                        onTriggered: trash.openTrash()
                    }

                    MenuItem {
                        text: qsTr("Empty Trash")
                        icon.name: "trash-empty"
                        onTriggered: trash.emptyTrash()
                        // visible: trash.count !== 0
                    }
                }
            }
        }

        Connections {
            target: Settings

            function onDirectionChanged() {
                popupTips.hide()
                windowPreview.hideNow()
            }
        }

        Connections {
            target: mainWindow

            function onVisibleChanged() {
                popupTips.hide()
                windowPreview.hideNow()
            }
        }
    }
}
