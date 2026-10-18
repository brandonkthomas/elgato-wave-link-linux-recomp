// AppBarToggleButton used for mute. "danger" (MuteIconToggleButtonMStyle) fills red when muted;
// "redBorder" (RedBorderIfCheckedToggleButtonMStyle) only outlines it. The speaker glyph follows the
// volume like MuteIconConverter: muted/0 -> VolumeMuted, <0.33 -> Volume1, <0.66 -> Volume2, else Volume.
import QtQuick
import QtQuick.Controls.Basic
import WaveLink.Backend

AbstractButton {
    id: control
    property real volume: 1
    property string variant: "danger"

    checkable: true
    hoverEnabled: true
    implicitWidth: Theme.sizeControlM
    implicitHeight: Theme.sizeControlM

    ToolTip.visible: hovered
    ToolTip.delay: 600
    ToolTip.text: Assets.text(checked ? "Elgato.BaseClasses.WinUI/Resources/UnmuteButtonText" : "Elgato.BaseClasses.WinUI/Resources/MuteButtonText")

    readonly property string glyph: checked || volume <= 0 ? Icons.volumeMutedOutlinedM
                                  : volume < 0.33 ? Icons.volume1OutlinedM
                                  : volume < 0.66 ? Icons.volume2OutlinedM : Icons.volumeOutlinedM

    background: Rectangle {
        radius: 4
        border.width: control.checked ? 1 : 0
        border.color: control.down ? Theme.buttonDangerSurfacePressedBrush : Theme.buttonDangerSurfaceHoverBrush
        color: {
            if (control.checked && control.variant === "danger")
                return control.down ? Theme.buttonDangerSurfacePressedBrush
                     : control.hovered ? Theme.buttonDangerSurfaceHoverBrush : Theme.buttonDangerSurfaceDefaultBrush
            return control.down ? Theme.subtleFillColorTertiary
                 : control.hovered ? Theme.subtleFillColorSecondary : "transparent"
        }
        Behavior on color { ColorAnimation { duration: 83 } }
    }
    contentItem: Item {
        FontIcon {
            anchors.centerIn: parent
            glyph: control.glyph
            color: control.checked && control.variant === "danger" ? Theme.buttonDangerContentColorBrush
                                                                   : Theme.textFillColorPrimary
        }
    }
}
