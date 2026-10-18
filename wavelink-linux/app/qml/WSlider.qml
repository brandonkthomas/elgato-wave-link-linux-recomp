// WinUI 3 Slider: 4 px track (accent fill up to the value, ControlStrongFill after), 20 px thumb with a
// 12 px accent dot that grows on hover and shrinks while pressed.
import QtQuick
import QtQuick.Controls.Basic

Slider {
    id: control
    implicitHeight: 32
    hoverEnabled: true
    padding: 0

    background: Item {
        x: control.leftPadding
        width: control.availableWidth
        height: control.height
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
            height: 4
            radius: 2
            color: Theme.controlStrongFillColorDefault
        }
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: control.visualPosition * parent.width
            height: 4
            radius: 2
            color: control.enabled ? Style.accentFillDefault : Theme.accentFillColorDisabled
        }
    }
    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: (control.height - height) / 2
        width: 20
        height: 20
        radius: 10
        color: Theme.controlSolidFillColorDefault
        border.width: 1
        border.color: Theme.controlStrokeColorDefaultBrush
        Rectangle {
            anchors.centerIn: parent
            width: 12
            height: 12
            radius: 6
            color: Style.accentFillDefault
            scale: control.pressed ? 0.71 : control.hovered ? 1.167 : 0.86
            Behavior on scale { NumberAnimation { duration: 167 } }
        }
    }
}
