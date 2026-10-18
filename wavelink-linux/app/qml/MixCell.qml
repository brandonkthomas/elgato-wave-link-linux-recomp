// MixingTableMixElement: either the quiet "add channel to mix" button, or the channel's level in the
// mix (mute toggle + level slider capped at 175 px, centered with 32 px side margins) with a delete
// button fading in on hover. A muted cell turns the card red (MixingTableElementMuteBackground).
import QtQuick
import QtQuick.Controls.Basic
import WaveLink.Backend

Item {
    id: cellItem
    property string inputId
    property string mixId
    readonly property var cell: { Engine.cellRevision; return Engine.cell(inputId, mixId) }
    readonly property bool active: cell.active === true
    readonly property bool muted: cell.muted === true

    height: Theme.mixingTableElementHeight
    HoverHandler { id: hover }

    // Not in the mix: AddToMixButtonStyle
    AbstractButton {
        anchors.fill: parent
        visible: !cellItem.active
        hoverEnabled: true
        onClicked: Engine.setCellActive(cellItem.inputId, cellItem.mixId, true)
        ToolTip.visible: hovered
        ToolTip.delay: 600
        ToolTip.text: Assets.text("MixingTable_AddChannelToMixButtonText")
        background: Rectangle {
            radius: 4
            color: parent.down ? Theme.subtleFillColorTertiary : parent.hovered ? Theme.subtleFillColorSecondary
                                                                               : Theme.mixingTableElementQuietBackgroundBrush
            border.width: 1
            border.color: Theme.mixingTableElementBorderBrush
        }
        contentItem: Item {
            FontIcon {
                anchors.centerIn: parent
                glyph: Icons.addCircleOutlinedM
                color: Theme.textFillColorSecondaryBrush
                opacity: parent.parent.hovered ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 200 } }
            }
        }
    }

    // In the mix: MixingTableElementStyle
    Rectangle {
        anchors.fill: parent
        visible: cellItem.active
        radius: 4
        color: cellItem.muted ? Theme.mixingTableElementMuteBackground : Theme.mixingTableElementBackground
        border.width: 1
        border.color: Theme.mixingTableElementBorderBrush
        Behavior on color { ColorAnimation { duration: 200 } }

        Row {
            anchors.verticalCenter: parent.verticalCenter
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(175, parent.width - 64)
            spacing: 8
            MuteToggle {
                id: mute
                anchors.verticalCenter: parent.verticalCenter
                variant: "redBorder"
                volume: cellItem.cell.volume
                checked: cellItem.muted
                onToggled: Engine.setCellMuted(cellItem.inputId, cellItem.mixId, checked)
            }
            LevelSlider {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - mute.width - parent.spacing
                value: cellItem.cell.volume
                muted: cellItem.muted
                peakLeft: { Engine.meterRevision; return Engine.cellPeak(cellItem.inputId, cellItem.mixId).x }
                peakRight: { Engine.meterRevision; return Engine.cellPeak(cellItem.inputId, cellItem.mixId).y }
                onMoved: (v) => Engine.setCellVolume(cellItem.inputId, cellItem.mixId, v)
            }
        }

        QuietButton {
            anchors.right: parent.right
            anchors.top: parent.top
            glyph: Icons.closeCircleOutlinedL
            glyphColor: Theme.textFillColorSecondaryBrush
            tip: Assets.text("MixingTable_RemoveChannelFromMixButtonText")
            opacity: hover.hovered ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 200 } }
            onClicked: Engine.setCellActive(cellItem.inputId, cellItem.mixId, false)
            contentItem: Item {
                FontIcon {
                    anchors.centerIn: parent
                    glyph: Icons.closeCircleOutlinedL
                    font.pixelSize: 18
                    color: Theme.textFillColorSecondaryBrush
                }
            }
        }
    }
}
