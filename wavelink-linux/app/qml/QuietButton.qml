// ButtonQuietMStyle / ButtonTransparentStyle: no fill until hovered, 32 px high, 4 px corners.
import QtQuick
import QtQuick.Controls.Basic

Button {
    id: control
    property string glyph
    property string glyphSize: "M"
    property color glyphColor: Theme.textFillColorPrimary
    property string tip
    property bool transparentHover: false

    implicitWidth: Theme.sizeControlM
    implicitHeight: Theme.sizeControlM
    padding: 0
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    ToolTip.visible: tip !== "" && hovered
    ToolTip.delay: 600
    ToolTip.text: tip

    background: Rectangle {
        radius: 4
        color: control.transparentHover ? "transparent"
             : control.down ? Theme.subtleFillColorTertiary
             : control.hovered ? Theme.subtleFillColorSecondary : "transparent"
        Behavior on color { ColorAnimation { duration: 83 } }
    }
    contentItem: Item {
        FontIcon {
            anchors.centerIn: parent
            glyph: control.glyph
            size: control.glyphSize
            color: control.glyphColor
            opacity: control.enabled ? (control.down ? 0.8 : 1) : 0.36
        }
    }
}
