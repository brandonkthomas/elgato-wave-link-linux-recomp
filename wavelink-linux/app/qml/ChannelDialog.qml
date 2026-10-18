// MixingTableInputChannelDialog (ContentDialogEx XL×L: 850×630, at most the window less 80 px): the
// channel name as an editable title; on the left the channel icon, name and level meter; on the right
// the "Apps" (software channels) and "Audio effects" tabs; below, "Remove channel" and "Close window".
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import WaveLink.Backend

Popup {
    id: dialog
    property string inputId
    property string tab: "effects"   // "apps" | "effects"

    readonly property var input: Engine.inputs.find(i => i.id === inputId)
    readonly property bool software: input !== undefined && input.kind === "software"
    readonly property bool wave3: input !== undefined && input.kind === "hardware"
                                  && (input.device || "").indexOf("Elgato_Wave_3") >= 0
    readonly property bool available: !input || input.kind === "software"
                                      || Engine.inputDevices.some(d => d.name === input.device)
    readonly property var tabKeys: software ? ["apps", "effects"] : ["effects"]

    function openFor(id, initialTab) {
        inputId = id
        tab = initialTab || "effects"
        open()
    }
    function commitName() {
        const name = nameField.text.trim()
        if (input && name !== "" && name !== input.name) Engine.renameInput(inputId, name)
    }

    onInputChanged: if (visible && !input) close()  // channel removed meanwhile
    onAboutToShow: nameField.text = input ? input.name : ""
    onAboutToHide: commitName()

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.max(Math.min(Theme.contentDialogExWidthXL, parent.width - 80), 650)
    height: Math.max(Math.min(Theme.contentDialogExHeightL, parent.height - 80), 400)
    modal: true
    focus: true
    padding: 0
    Overlay.modal: Rectangle { color: Theme.contentDialogSmokeFill }
    background: Rectangle {
        radius: 8
        color: Theme.contentDialogBackground
        border.width: 1
        border.color: Theme.contentDialogBorderBrush
    }

    contentItem: ColumnLayout {
        spacing: 0

        Rectangle {  // ContentDialog top area
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.contentDialogTopOverlay
            topLeftRadius: 8
            topRightRadius: 8
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Theme.contentDialogSeparatorBorderBrush
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 12

                WTextField {
                    id: nameField
                    Layout.preferredWidth: 380
                    Layout.preferredHeight: 40
                    font.pixelSize: Theme.subtitleTextBlockFontSize
                    font.weight: Font.Medium
                    onAccepted: { dialog.commitName(); focus = false }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: Theme.spaceXl

                    ColumnLayout {  // icon, name, meter
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: 1
                        spacing: 8
                        Item { Layout.fillHeight: true }
                        ChannelIcon {
                            Layout.alignment: Qt.AlignHCenter
                            size: 160
                            name: dialog.input ? dialog.input.name : ""
                            kind: dialog.input ? dialog.input.kind : "software"
                        }
                        WText {
                            Layout.fillWidth: true
                            Layout.topMargin: 16
                            horizontalAlignment: Text.AlignHCenter
                            variant: "subtitle"
                            text: dialog.input ? dialog.input.name : ""
                        }
                        WText {
                            visible: !dialog.available
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            variant: "body"
                            color: Theme.textFillColorDisabledBrush
                            text: Assets.text("MixingTable_Unavailable")
                        }
                        Column {  // LevelmeterStereo, horizontal, margin 64,16
                            Layout.fillWidth: true
                            Layout.leftMargin: 64
                            Layout.rightMargin: 64
                            Layout.topMargin: 16
                            Layout.bottomMargin: 16
                            spacing: 2
                            Repeater {
                                model: 2
                                Rectangle {
                                    required property int index
                                    width: parent.width
                                    height: 5
                                    radius: 2
                                    color: Theme.levelmeterBackgroundBrush
                                    LevelMeter {
                                        anchors.fill: parent
                                        peak: {
                                            Engine.meterRevision
                                            if (!dialog.visible || !dialog.input) return 0
                                            const p = Engine.inputPeak(dialog.inputId)
                                            return parent.index === 0 ? p.x : p.y
                                        }
                                    }
                                }
                            }
                        }
                        Rectangle {  // SampleRecorder in an 80 px ContentPanel (Elgato mics)
                            visible: dialog.wave3
                            Layout.fillWidth: true
                            Layout.preferredHeight: 80
                            radius: 8
                            color: Theme.layerFillColorAlt
                            border.width: 1
                            border.color: Theme.cardStrokeColorDefaultBrush
                            SoundCheck {
                                anchors.fill: parent
                                anchors.margins: 8
                                anchors.topMargin: 6
                                inputId: dialog.wave3 ? dialog.inputId : ""
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }

                    ColumnLayout {  // tabs
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: 1
                        spacing: Theme.spaceM
                        TabStrip {
                            id: tabStrip
                            Layout.fillWidth: true
                            visible: dialog.tabKeys.length > 1
                            tabs: dialog.tabKeys.map(k => Assets.text(k === "apps" ? "MixingTable_Header_Apps"
                                                                                   : "MixingTable_Header_AudioEffects"))
                            current: Math.max(0, dialog.tabKeys.indexOf(dialog.tab))
                            onCurrentChanged: dialog.tab = dialog.tabKeys[current]
                        }
                        Flickable {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            contentHeight: tabContent.implicitHeight
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar {}
                            ColumnLayout {
                                id: tabContent
                                width: parent.width
                                spacing: 0
                                InputEffects {
                                    visible: dialog.tab === "effects"
                                    Layout.fillWidth: true
                                    inputId: dialog.inputId
                                    wave3: dialog.wave3
                                    sideMargin: dialog.wave3 ? 0 : 16
                                }
                                AppRoutes {
                                    visible: dialog.tab === "apps"
                                    Layout.fillWidth: true
                                    inputId: dialog.inputId
                                }
                            }
                        }
                    }
                }
            }
        }

        RowLayout {  // command area
            Layout.fillWidth: true
            Layout.margins: 24
            WButton {
                glyph: Icons.trashOutlinedM
                text: Assets.text("MixingTable_RemoveChannel_Button")
                onClicked: {
                    Engine.removeInput(dialog.inputId)
                    dialog.close()
                }
            }
            Item { Layout.fillWidth: true }
            WButton {
                accent: true
                text: Assets.text("MixingTable_CloseWindow_Button")
                onClicked: dialog.close()
            }
        }
    }
}
