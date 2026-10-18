// MainWindowTopBar (32 px): pane toggle (48×32), 16 px app icon and caption title, then Windows 11
// style caption buttons. Dragging moves the window; double-click maximizes.
import QtQuick
import QtQuick.Window
import QtQuick.Controls.Basic
import QtQuick.Shapes
import WaveLink.Backend

Item {
    id: bar
    required property var window
    signal paneToggleClicked()
    height: 32

    TapHandler {
        onDoubleTapped: bar.window.visibility === Window.Maximized ? bar.window.showNormal() : bar.window.showMaximized()
        gesturePolicy: TapHandler.DragThreshold
    }
    DragHandler {
        target: null
        onActiveChanged: if (active) bar.window.startSystemMove()
    }

    QuietButton {
        id: paneToggle
        x: 8 + 1
        width: 48
        height: 32
        glyph: Icons.menuOutlinedM
        glyphSize: "S"
        onClicked: bar.paneToggleClicked()
    }
    Row {
        anchors.left: paneToggle.right
        anchors.leftMargin: 0
        anchors.verticalCenter: parent.verticalCenter
        spacing: 16
        Image {
            anchors.verticalCenter: parent.verticalCenter
            width: 16
            height: 16
            source: Assets.image("StoreLogo.png")
            fillMode: Image.PreserveAspectFit
            smooth: true
            opacity: bar.window.active ? 1 : 0.5
        }
        WText {
            anchors.verticalCenter: parent.verticalCenter
            variant: "caption"
            text: "Wave Link"
            color: bar.window.active ? Theme.textFillColorPrimary : Theme.textFillColorSecondaryBrush
        }
    }

    Row {  // caption buttons, 46×32 each
        anchors.right: parent.right
        anchors.top: parent.top
        component CaptionButton: AbstractButton {
            id: cb
            property bool danger: false
            property string kind
            width: 46
            height: 32
            hoverEnabled: true
            background: Rectangle {
                color: cb.danger && (cb.hovered || cb.down) ? (cb.down ? "#c7c42b1c" : "#c42b1c")
                     : cb.down ? Theme.subtleFillColorTertiary : cb.hovered ? Theme.subtleFillColorSecondary : "transparent"
            }
            contentItem: Item {
                Shape {
                    anchors.centerIn: parent
                    width: 10
                    height: 10
                    opacity: bar.window.active || cb.hovered ? 1 : 0.5
                    ShapePath {
                        strokeWidth: 1
                        strokeColor: cb.danger && cb.hovered ? "white" : Theme.textFillColorPrimary
                        fillColor: "transparent"
                        PathSvg {
                            path: cb.kind === "min" ? "M 0 5 L 10 5"
                                : cb.kind === "max" ? (bar.window.visibility === Window.Maximized
                                                       ? "M 2 0 L 10 0 L 10 8 M 0 2 L 8 2 L 8 10 L 0 10 Z"
                                                       : "M 0 0 L 10 0 L 10 10 L 0 10 Z")
                                : "M 0 0 L 10 10 M 10 0 L 0 10"
                        }
                    }
                }
            }
        }
        CaptionButton { kind: "min"; onClicked: bar.window.showMinimized() }
        CaptionButton {
            kind: "max"
            onClicked: bar.window.visibility === Window.Maximized ? bar.window.showNormal() : bar.window.showMaximized()
        }
        CaptionButton { kind: "close"; danger: true; onClicked: bar.window.close() }
    }
}
