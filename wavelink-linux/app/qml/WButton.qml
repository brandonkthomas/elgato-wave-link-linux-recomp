// WinUI 3 standard Button (ControlFill, 4 px corners, 32 px high) with ControlElevationBorderBrush: the
// stroke darkens to ControlStrokeColorSecondary along the bottom edge. Optional leading glyph.
import QtQuick
import QtQuick.Controls.Basic

AbstractButton {
    id: control
    property string glyph
    property bool accent: false
    property bool danger: false  // MuteToggleButtonMStyle while muted: red fill, white content
    hoverEnabled: true
    implicitHeight: 32
    implicitWidth: row.implicitWidth + 24

    background: Rectangle {
        radius: 4
        color: control.danger ? (control.down ? Theme.buttonDangerSurfacePressedBrush
                                              : control.hovered ? Theme.buttonDangerSurfaceHoverBrush : Theme.buttonDangerSurfaceDefaultBrush)
             : control.accent ? (control.down ? Qt.darker(Style.accentFillDefault, 1.15)
                                              : control.hovered ? Qt.darker(Style.accentFillDefault, 1.07) : Style.accentFillDefault)
             : control.down ? Theme.controlFillColorTertiaryBrush
             : control.hovered ? Theme.controlFillColorSecondaryBrush : Theme.controlFillColorDefaultBrush
        border.width: control.accent || control.danger ? 0 : 1
        border.color: Theme.controlStrokeColorDefaultBrush
        Item {  // bottom edge of the elevation border: the lowest pixels of a darker rounded outline
            visible: !control.accent && !control.danger
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 2
            clip: true
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: control.height
                radius: 4
                color: "transparent"
                border.width: 1
                border.color: Theme.controlStrokeColorSecondary
            }
        }
    }
    contentItem: Item {
        Row {
            id: row
            anchors.centerIn: parent
            spacing: 8
            FontIcon {
                visible: control.glyph !== ""
                anchors.verticalCenter: parent.verticalCenter
                glyph: control.glyph
                size: "S"
                color: control.danger ? Theme.buttonDangerContentColorBrush
                     : control.accent ? Theme.textOnAccentFillColorPrimary : Theme.textFillColorPrimary
            }
            WText {
                anchors.verticalCenter: parent.verticalCenter
                variant: "body"
                text: control.text
                color: control.danger ? Theme.buttonDangerContentColorBrush
                     : control.accent ? Theme.textOnAccentFillColorPrimary
                     : control.down ? Theme.textFillColorSecondaryBrush : Theme.textFillColorPrimary
            }
        }
    }
}
