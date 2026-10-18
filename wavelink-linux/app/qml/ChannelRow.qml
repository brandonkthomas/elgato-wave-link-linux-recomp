// ChannelItemControl inside MixingTableInputChannelView: 320×60 card with icon + name, mute toggle,
// an 80 px level slider and the effects button (green while any effect is on). Hovering swaps the
// icon for drag/edit handles and tints the name with the accent text color; a muted channel turns the
// card red. Clicking the icon or name opens the channel dialog, the effects button its effects tab.
import QtQuick
import QtQuick.Controls.Basic
import WaveLink.Backend

Rectangle {
    id: row
    property var input              // {id, name, kind, ...}
    readonly property var level: { Engine.inputRevision; return input ? Engine.inputLevel(input.id) : ({ volume: 1, muted: false }) }
    readonly property real volume: level.volume
    readonly property bool muted: level.muted
    readonly property bool effectsActive: {
        Engine.effectRevision
        return input ? Engine.effects(input.id).some(e => !e.bypass) : false
    }
    signal removeRequested()
    signal editRequested(string tab)

    // Running apps that can be routed here (software channels only), one entry per app key.
    readonly property var routableApps: {
        if (!input || input.kind !== "software") return []
        const seen = {}
        return Engine.apps.filter(a => a.key !== "" && !seen[a.key] && (seen[a.key] = true))
    }

    width: 320
    height: Theme.mixingTableElementHeight
    radius: 4
    color: muted ? Theme.mixingTableElementMuteBackground : Theme.mixingTableElementBackground
    border.width: 1
    border.color: Theme.mixingTableElementBorderBrush
    Behavior on color { ColorAnimation { duration: 200 } }

    HoverHandler { id: hover }
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: contextMenu.popup()
    }
    Menu {
        id: contextMenu
        Menu {
            title: Assets.text("MixingTable_Header_Apps")
            enabled: row.routableApps.length > 0
            Repeater {
                model: row.routableApps
                MenuItem {
                    required property var modelData
                    text: modelData.app !== "" ? modelData.app : modelData.binary
                    checkable: true
                    checked: modelData.rule === row.input.id
                    onTriggered: Engine.routeApp(modelData.key, checked ? row.input.id : "")
                }
            }
        }
        MenuItem {
            text: Assets.text("MixingTable_RemoveChannel_Button")
            onTriggered: row.removeRequested()
        }
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.spaceControlMGap
        width: parent.width - 16 - right.width - 8
        TapHandler { onTapped: row.editRequested("") }

        Item {
            width: 40
            height: 40
            ChannelIcon {
                anchors.centerIn: parent
                name: row.input ? row.input.name : ""
                kind: row.input ? row.input.kind : "software"
                opacity: hover.hovered ? 0 : 1
                Behavior on opacity { NumberAnimation { duration: 200 } }
            }
            FontIcon {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                glyph: Icons.dragOutlinedM
                font.pixelSize: 18
                color: Style.accentTextFillPrimary
                opacity: hover.hovered ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 200 } }
            }
            FontIcon {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                glyph: Icons.editOutlinedL
                font.pixelSize: 18
                color: Style.accentTextFillPrimary
                opacity: hover.hovered ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 200 } }
            }
        }
        Column {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 40 - parent.spacing
            WText {
                width: parent.width
                variant: "bodyStrong"
                text: row.input ? row.input.name : ""
                maximumLineCount: 2
                wrapMode: Text.Wrap
                color: hover.hovered ? Style.accentTextFillPrimary : Theme.textFillColorPrimary
                Behavior on color { ColorAnimation { duration: 200 } }
            }
            WText {
                visible: row.muted
                topPadding: Theme.spaceControlSGap
                variant: "body"
                font.pixelSize: Theme.fontSizeS
                text: Assets.text("InputChannel_ElgatoDeviceHardwareInputMute_Title")
                color: Theme.systemFillColorCriticalBrush
            }
        }
    }

    Row {
        id: right
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8
        MuteToggle {
            anchors.verticalCenter: parent.verticalCenter
            variant: "danger"
            volume: row.volume
            checked: row.muted
            onToggled: Engine.setInputMuted(row.input.id, checked)
        }
        LevelSlider {
            anchors.verticalCenter: parent.verticalCenter
            width: 80
            value: row.volume
            muted: row.muted
            peakLeft: { Engine.meterRevision; return row.input ? Engine.inputPeak(row.input.id).x : 0 }
            peakRight: { Engine.meterRevision; return row.input ? Engine.inputPeak(row.input.id).y : 0 }
            onMoved: (v) => Engine.setInputVolume(row.input.id, v)
        }
        QuietButton {
            anchors.verticalCenter: parent.verticalCenter
            glyph: Icons.soundEffectOutlinedM
            glyphColor: row.effectsActive ? Theme.colorGreen50 : Theme.textFillColorPrimary
            tip: Assets.text("MixingTable_EditChannelEffectsButtonText")
            onClicked: row.editRequested("effects")
        }
    }
}
