// NavigationPageMixingTable: "Mixes" header with the main output device, Stream Deck export and compact
// buttons, then the routing grid — mix headers across the top, channels down the left, cells between.
// Column width follows CalculateWidth(): (viewport - add-mix space) / mixes - 8, clamped to 200..400.
import QtQuick
import QtQuick.Controls.Basic
import WaveLink.Backend

Item {
    id: page
    property string openChannel  // input id whose dialog opens once it exists (--channel)
    property string openEffect   // effect id whose settings open once it exists (--effect)

    function openChannelDialog(id, tab) { channelDialog.openFor(id, tab) }
    ChannelDialog { id: channelDialog }
    EffectEditor { id: effectEditor }
    Connections {
        target: Engine
        enabled: page.openChannel !== "" || page.openEffect !== ""
        function onStateChanged() {
            if (Engine.inputs.some(i => i.id === page.openChannel)) {
                channelDialog.openFor(page.openChannel, "")
                page.openChannel = ""
            }
            const owner = Engine.inputs.find(i => (i.effects || []).some(e => e.id === page.openEffect))
            if (owner) {
                effectEditor.inputId = owner.id
                effectEditor.effectId = page.openEffect
                effectEditor.open()
                page.openEffect = ""
            }
        }
    }

    property string listeningMix: Engine.mixes.length > 0 ? Engine.mixes[0].id : ""
    readonly property var listening: Engine.mixes.find(m => m.id === listeningMix)
    readonly property bool canAddMix: Engine.mixes.length < 5
    readonly property real addMixSpace: canAddMix ? 40 + 24 : 8
    readonly property real columnWidth: {
        const n = Math.max(1, Engine.mixes.length)
        return Math.max(200, Math.min(400, (scroller.width - addMixSpace) / n - 8))
    }

    // Header row (RelativePanel, margin 24,16)
    Item {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        anchors.topMargin: 16
        height: Math.max(title.implicitHeight, 32)

        WText {
            id: title
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            variant: "title"
            text: Assets.text("MixingTable_Header")
        }
        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            OutputDeviceButton { mix: page.listening }
            QuietButton {
                width: Theme.mixingTableRightButtonWidth
                height: Theme.mixingTableHeaderButtonHeight
                glyph: Icons.shareOutlinedL
                tip: Assets.text("MixingTable_ExportToStreamDeckText")
                background: Rectangle {
                    radius: 4
                    color: parent.down ? Theme.controlFillColorTertiaryBrush
                         : parent.hovered ? Theme.controlFillColorSecondaryBrush : Theme.controlFillColorDefaultBrush
                    border.width: 1
                    border.color: Theme.controlStrokeColorDefaultBrush
                }
            }
            QuietButton {
                width: Theme.mixingTableRightButtonWidth
                height: Theme.mixingTableHeaderButtonHeight
                glyph: Icons.minimizeOutlinedL
                tip: Assets.text("MixingTable_ShrinkButtonText")
                background: Rectangle {
                    radius: 4
                    color: parent.down ? Theme.controlFillColorTertiaryBrush
                         : parent.hovered ? Theme.controlFillColorSecondaryBrush : Theme.controlFillColorDefaultBrush
                    border.width: 1
                    border.color: Theme.controlStrokeColorDefaultBrush
                }
            }
        }
    }

    // Routing grid
    Item {
        id: grid
        anchors.top: header.bottom
        anchors.topMargin: 16
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        // Mix headers (row 0, column 1): follow the cell scroller horizontally.
        Item {
            id: mixHeaderCanvas
            anchors.left: channels.right
            anchors.right: parent.right
            anchors.top: parent.top
            height: Theme.mixingTableElementHeight + 8
            clip: true
            Row {
                x: -scroller.contentX
                spacing: 8
                Repeater {
                    model: Engine.mixes
                    MixHeader {
                        required property var modelData
                        width: page.columnWidth
                        mix: modelData
                        listening: page.listeningMix === modelData.id
                        onListenToggled: page.listeningMix = modelData.id
                        onRemoveRequested: Engine.removeMix(modelData.id)
                    }
                }
                QuietButton {
                    visible: page.canAddMix
                    width: Theme.mixingTableRightButtonWidth
                    height: Theme.mixingTableElementHeight
                    glyph: Icons.addOutlinedM
                    glyphColor: Theme.textFillColorSecondaryBrush
                    tip: Assets.text("MixingTable_AddMixButtonText")
                    onClicked: Engine.addMix("")
                    background: Rectangle {
                        radius: 4
                        color: parent.down ? Theme.subtleFillColorTertiary : parent.hovered ? Theme.subtleFillColorSecondary
                                                                                         : Theme.mixingTableElementQuietBackgroundBrush
                        border.width: 1
                        border.color: Theme.mixingTableElementBorderBrush
                    }
                }
            }
        }

        // Channels (row 1, column 0, margin-left 16): follow the cell scroller vertically.
        Item {
            id: channels
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.top: mixHeaderCanvas.bottom
            anchors.bottom: parent.bottom
            width: 320 + 8
            clip: true
            Column {
                y: -scroller.contentY
                spacing: 8
                Repeater {
                    model: Engine.inputs
                    ChannelRow {
                        required property var modelData
                        input: modelData
                        onRemoveRequested: Engine.removeInput(modelData.id)
                        onEditRequested: (tab) => channelDialog.openFor(modelData.id, tab)
                    }
                }
                AddChannelButton { width: 320 }
            }
        }

        // Cells (row 1, column 1)
        Flickable {
            id: scroller
            anchors.left: channels.right
            anchors.right: parent.right
            anchors.top: mixHeaderCanvas.bottom
            anchors.bottom: parent.bottom
            contentWidth: Engine.mixes.length * (page.columnWidth + 8) + page.addMixSpace
            contentHeight: Engine.inputs.length * (Theme.mixingTableElementHeight + 8) + Theme.mixingTableElementHeight + 16
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            ScrollBar.horizontal: ScrollBar {}

            Column {
                spacing: 8
                Repeater {
                    model: Engine.inputs
                    Row {
                        id: cellRow
                        required property var modelData
                        spacing: 8
                        Repeater {
                            model: Engine.mixes
                            MixCell {
                                required property var modelData
                                width: page.columnWidth
                                inputId: cellRow.modelData.id
                                mixId: modelData.id
                            }
                        }
                    }
                }
            }
        }
    }
}
