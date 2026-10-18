// WinUI 3 TextBox: 32 px, ControlFill with a 1 px stroke and a darker bottom edge; focused it turns
// ControlFillColorInputActive with a 2 px accent bottom edge.
import QtQuick
import QtQuick.Controls.Basic

TextField {
    id: control
    implicitHeight: 32
    leftPadding: 10
    rightPadding: 10
    font.family: Style.textFont
    font.pixelSize: Theme.bodyTextBlockFontSize
    color: Theme.textFillColorPrimary
    selectionColor: Style.accentFillDefault
    selectedTextColor: Theme.textOnAccentFillColorPrimary
    placeholderTextColor: Theme.textFillColorSecondaryBrush
    hoverEnabled: true
    background: Rectangle {
        radius: 4
        color: control.activeFocus ? Theme.controlFillColorInputActive
             : control.hovered ? Theme.controlFillColorSecondaryBrush : Theme.controlFillColorDefaultBrush
        border.width: 1
        border.color: Theme.controlStrokeColorDefaultBrush
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 1
            anchors.rightMargin: 1
            height: control.activeFocus ? 2 : 1
            color: control.activeFocus ? Style.accentFillDefault : Theme.controlStrongStrokeColorDefault
        }
    }
}
