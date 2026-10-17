import QtQuick 2.12
import QtQuick.Controls 2.12
import Qt5Compat.GraphicalEffects
import Lingmo.Dock 1.0
import LingmoUI.CompatibleModule 3.0 as LingmoUI

Item {
    id: control

    property bool isLeft: Settings.direction === DockSettings.Left
    property bool isRight: Settings.direction === DockSettings.Right
    property bool isBottom: Settings.direction === DockSettings.Bottom

    property var iconSize: root.isHorizontal ? control.height * iconSizeRatio
                                             : control.width * iconSizeRatio

    property bool draggable: false
    property int dragItemIndex

    property alias icon: icon
    property alias mouseArea: iconArea
    property alias dropArea: iconDropArea

    property bool enableActivateDot: true
    property bool isActive: false

    property var popupText
    // Set when the item has windows to preview instead of the name tooltip
    property string previewAppId: ""

    property double iconSizeRatio: 0.8
    property var iconName

    property bool dragStarted: false

    // Magnification (Settings › Dock, off by default): the icon under the pointer
    // grows to Settings.magnificationSize percent and its neighbours a little
    // less, following the pointer along the dock. The window has room for that
    // on the screen side (mainWindow.magnificationExtent); without it (no
    // compositor) the icon only grows until it fills its cell.
    readonly property bool magnifyOutward: mainWindow.magnificationExtent > 0
    property real maxMagnification: magnifyOutward ? Settings.magnificationSize / 100
                                                   : Math.min(Settings.magnificationSize / 100, 1 / iconSizeRatio)
    readonly property real magnificationAmount: {
        if (!Settings.magnification || root.hoverPos < 0 || control.dragStarted)
            return 0
        const c = control.mapToItem(root, control.width / 2, control.height / 2)
        const cell = root.isHorizontal ? control.width : control.height
        const d = Math.abs((root.isHorizontal ? c.x : c.y) - root.hoverPos)
        const t = Math.max(0, 1 - d / (cell * 2))
        return t * t * (3 - 2 * t)      // smoothstep
    }
    property real magnificationScale: 1 + (maxMagnification - 1) * magnificationAmount
    // The bigger icon is drawn over its neighbours
    z: magnificationAmount
    property int animationDuration: 90

    signal positionChanged()
    signal released()
    signal pressed(var mouse)
    signal pressAndHold(var mouse)
    signal clicked(var mouse)
    signal rightClicked(var mouse)
    signal doubleClicked(var mouse)
    signal dropped(var drop)

    Drag.active: mouseArea.drag.active && control.draggable
    Drag.dragType: Drag.Automatic
    Drag.supportedActions: Qt.MoveAction
    Drag.hotSpot.x: icon.width / 2
    Drag.hotSpot.y: icon.height / 2

    Drag.onDragStarted:  {
        dragStarted = true
    }

    Drag.onDragFinished: {
        dragStarted = false
    }

    LingmoUI.IconItem {
        id: icon
        anchors.centerIn: parent
        width: control.iconSize
        height: control.iconSize
        source: iconName

        // Apply magnification scale
        scale: magnificationScale
        // Grows away from the screen edge, into the room above the panel
        transformOrigin: !control.magnifyOutward ? Item.Center
                       : isLeft ? Item.Left : isRight ? Item.Right : Item.Bottom

        visible: !dragStarted

        // Smooth scale animation
        Behavior on scale {
            NumberAnimation {
                duration: animationDuration
                easing.type: Easing.OutCubic
            }
        }

        ColorOverlay {
            id: iconColorize
            anchors.fill: icon
            source: icon
            color: "#000000"
            opacity: iconArea.pressed && !mouseArea.drag.active ? 0.4 : 0
        }
    }

    DropArea {
        id: iconDropArea
        anchors.centerIn: parent
        width: control.iconSize
        height: control.iconSize
        enabled: draggable
        onDropped: control.dropped(drop)
    }

    MouseArea {
        id: iconArea
        // Sized from iconSize rather than anchors.fill: icon. Items created while
        // the dock was still 0px wide (the launcher, the trash) kept a 0x0 mouse
        // area with the anchor, so they never received clicks.
        anchors.centerIn: parent
        width: control.iconSize
        height: control.iconSize
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        drag.axis: Drag.XAndYAxis

        onClicked: function(mouse) {
            if (mouse.button === Qt.RightButton)
                control.rightClicked(mouse)
            else
                control.clicked(mouse)
        }

        onPressed: function(mouse) {
            control.pressed(mouse)
            popupTips.hide()
            windowPreview.hideNow()
        }

        onPositionChanged: function(mouse) {
            if (pressed) {
                if (control.draggable && mouse.source !== Qt.MouseEventSynthesizedByQt) {
                    drag.target = icon
                    icon.grabToImage(function(result) {
                        control.Drag.imageSource = result.url
                    })
                } else {
                    drag.target = null
                }
            }

            control.positionChanged()
        }

        onPressAndHold : function(mouse) {control.pressAndHold(mouse)}
        onReleased: {
            drag.target = null
            control.released()
        }

        onContainsMouseChanged: {
            if (containsMouse && control.previewAppId !== "" && !control.dragStarted) {
                popupTips.hide()
                windowPreview.requestShow(control, control.previewAppId, control.iconName)
            } else if (containsMouse && control.popupText !== "") {
                windowPreview.hideNow()

                popupTips.popupText = control.popupText

                if (Settings.direction === DockSettings.Left)
                    popupTips.position = Qt.point(root.mapToGlobal(root.width, 0).x + mainWindow.magnificationExtent + LingmoUI.Units.largeSpacing,
                                                  control.mapToGlobal(0, 0).y + (control.height / 2 - popupTips.height / 2))
                else if (Settings.direction === DockSettings.Right)
                    popupTips.position = Qt.point(control.mapToGlobal(0, 0).x - mainWindow.magnificationExtent - popupTips.width - LingmoUI.Units.smallSpacing / 2,
                                                  control.mapToGlobal(0, 0).y + (control.height / 2 - popupTips.height / 2))
                else
                    popupTips.position = Qt.point(control.mapToGlobal(0, 0).x + (control.width / 2 - popupTips.width / 2),
                                                  control.mapToGlobal(0, 0).y - mainWindow.magnificationExtent - popupTips.height - LingmoUI.Units.smallSpacing)

                popupTips.show()
            } else {
                // The next icon may have been entered before this one was left:
                // only take down what this icon put up
                if (popupTips.popupText === control.popupText)
                    popupTips.hide()
                windowPreview.requestHide(control)
            }
        }
    }

    // Running indicator: a dot under the icon, a longer accent-coloured pill
    // for the active app (beside the icon when the dock is on a side)
    Rectangle {
        id: activeRect

        readonly property real cell: isBottom ? control.height : control.width
        readonly property real gap: (cell - control.iconSize) / 2      // space between icon and edge
        property real dotSize: Math.max(5, Math.round(control.iconSize * 0.1))
        property real pillLength: Math.round(control.iconSize * 0.4)
        readonly property real length: isActive ? pillLength : dotSize
        readonly property real offset: (gap - dotSize) / 2              // centred in that space

        width: isBottom ? length : dotSize
        height: isBottom ? dotSize : length
        radius: dotSize / 2
        visible: enableActivateDot && !dragStarted
        color: isActive ? LingmoUI.Theme.highlightColor
                        : Qt.rgba(LingmoUI.Theme.textColor.r, LingmoUI.Theme.textColor.g,
                                  LingmoUI.Theme.textColor.b, LingmoUI.Theme.darkMode ? 0.75 : 0.6)

        x: isBottom ? (control.width - width) / 2
                    : isLeft ? Math.max(1, offset) : control.width - Math.max(1, offset) - width
        y: isBottom ? control.height - Math.max(1, offset) - height
                    : (control.height - height) / 2

        Behavior on width {
            NumberAnimation { duration: isBottom ? 250 : 0; easing.type: Easing.InOutSine }
        }

        Behavior on height {
            NumberAnimation { duration: !isBottom ? 250 : 0; easing.type: Easing.InOutSine }
        }

        Behavior on color {
            ColorAnimation { duration: 200 }
        }
    }
}
