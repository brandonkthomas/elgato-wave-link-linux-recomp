// WinUI 3 ToggleSwitch: 40×20 pill; off = outlined with a secondary-text knob, on = accent fill with a
// knob in TextOnAccentFillColorPrimary. The 12 px knob grows to 14 px on hover. Label 8 px to the right.
import QtQuick
import QtQuick.Controls.Basic

AbstractButton {
    id: control
    checkable: true
    hoverEnabled: true
    implicitHeight: 32
    implicitWidth: 40 + (label.text !== "" ? 8 + label.implicitWidth : 0)
    property alias label: label.text

    Rectangle {
        id: track
        anchors.verticalCenter: parent.verticalCenter
        width: 40
        height: 20
        radius: 10
        color: control.checked ? (control.down ? Qt.darker(Style.accentFillDefault, 1.1) : Style.accentFillDefault)
                               : (control.hovered ? Theme.controlAltFillColorTertiary : Theme.controlAltFillColorSecondary)
        border.width: control.checked ? 0 : 1
        border.color: Theme.controlStrongStrokeColorDefault
        Behavior on color { ColorAnimation { duration: 83 } }

        Rectangle {
            readonly property real size: control.down ? 14 : control.hovered ? 14 : 12
            width: control.down ? 17 : size
            height: size
            radius: size / 2
            anchors.verticalCenter: parent.verticalCenter
            x: control.checked ? parent.width - width - 4 : 4
            color: control.checked ? Theme.textOnAccentFillColorPrimary : Theme.textFillColorSecondaryBrush
            Behavior on x { NumberAnimation { duration: 167; easing.type: Easing.OutCubic } }
            Behavior on width { NumberAnimation { duration: 83 } }
        }
    }
    WText {
        id: label
        anchors.left: track.right
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        variant: "body"
    }
}
