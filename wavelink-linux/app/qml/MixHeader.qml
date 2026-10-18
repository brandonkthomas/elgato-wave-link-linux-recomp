// MixingTableOutputMixView: 60 px card with the mix icon, name, output count, the "listen on main
// output" toggle (shown on hover or when selected) and a stereo meter hugging the bottom edge.
import QtQuick
import QtQuick.Controls.Basic
import WaveLink.Backend

Rectangle {
    id: header
    property var mix               // {id, name, slot, outputs}
    property bool listening: false
    signal listenToggled()
    signal removeRequested()

    height: Theme.mixingTableElementHeight
    radius: 4
    color: Theme.mixingTableElementBackground
    border.width: 1
    border.color: Theme.mixingTableElementBorderBrush
    clip: true

    HoverHandler { id: hover }
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: contextMenu.popup()
    }
    Menu {
        id: contextMenu
        MenuItem {
            text: Assets.text("MixingTable_DeleteMix_Button")
            onTriggered: header.removeRequested()
        }
    }

    Item {
        id: iconBox
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        width: 32
        height: 32
        MixIcon {
            anchors.centerIn: parent
            slot: header.mix ? header.mix.slot : 0
            opacity: hover.hovered ? 0 : 1
            Behavior on opacity { NumberAnimation { duration: 200 } }
        }
        FontIcon {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            glyph: Icons.dragOutlinedM
            font.pixelSize: 18
            width: 16
            color: Style.accentTextFillPrimary
            opacity: hover.hovered ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 200 } }
        }
        FontIcon {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            glyph: Icons.editOutlinedL
            font.pixelSize: 18
            width: 16
            color: Style.accentTextFillPrimary
            opacity: hover.hovered ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 200 } }
        }
    }

    Column {
        anchors.left: iconBox.right
        anchors.leftMargin: Theme.spaceControlMGap
        anchors.right: listen.left
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        WText {
            width: parent.width
            variant: "bodyStrong"
            text: header.mix ? header.mix.name : ""
            color: hover.hovered ? Style.accentTextFillPrimary : Theme.textFillColorPrimary
            Behavior on color { ColorAnimation { duration: 200 } }
        }
        WText {
            width: parent.width
            variant: "body"
            visible: text !== ""
            color: Theme.textFillColorSecondaryBrush
            text: {
                const n = header.mix && header.mix.outputs ? header.mix.outputs.length : 0
                return n === 0 ? "" : n === 1 ? Assets.text("MixingTable_Outputs_Single")
                                              : Assets.text("MixingTable_Outputs_Multiple").replace("{0}", n)
            }
        }
    }

    AbstractButton {
        id: listen
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        width: 32
        height: 32
        checkable: true
        checked: header.listening
        hoverEnabled: true
        opacity: hover.hovered || checked ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 200 } }
        onClicked: header.listenToggled()
        background: Rectangle {  // SubtleToggleButtonStyle
            radius: 4
            color: listen.down ? Theme.controlFillColorTertiaryBrush
                 : listen.hovered ? Theme.controlFillColorSecondaryBrush : Theme.controlFillColorDefaultBrush
            border.width: 1
            border.color: Theme.controlStrokeColorDefaultBrush
        }
        contentItem: Item {
            FontIcon {
                anchors.centerIn: parent
                glyph: Icons.earOutlinedL
                color: listen.checked ? Style.accentTextFillPrimary : Theme.textFillColorPrimary
            }
        }
    }

    Column {  // OutputMixLevelmeterStyleTransparent: two thin meters on the bottom edge
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 1
        anchors.rightMargin: 1
        LevelMeter {
            width: parent.width
            height: 2
            peakVisible: false
            peak: { Engine.meterRevision; return header.mix ? Engine.mixPeak(header.mix.id).x : 0 }
        }
        LevelMeter {
            width: parent.width
            height: 2
            peakVisible: false
            peak: { Engine.meterRevision; return header.mix ? Engine.mixPeak(header.mix.id).y : 0 }
        }
    }
}
