// SampleRecorder ("Sound Check"): records up to 10 s of the mic after its effects and plays it back on
// the chosen output. "Rec" (red dot; white on red while recording) gives way to play/pause and delete
// once something is recorded; 26 bars show the recording (a row of dots before), played bars turn
// accent; the headphones button picks the output.
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import WaveLink.Backend

ColumnLayout {
    id: sc
    property string inputId
    readonly property var st: Engine.soundCheck
    readonly property bool recording: st.state === "recording"
    readonly property bool playing: st.state === "playing"
    readonly property bool hasRecording: st.hasRecording === true && st.input === inputId
    readonly property var bars: st.bars || []
    // Bars recorded and bars already played, out of 26.
    readonly property int recordedBars: recording || hasRecording ? Math.ceil((st.length || 0) * 26) : 0
    readonly property int playedBars: playing ? Math.floor((st.position || 0) * 26) : 0
    spacing: Theme.headerControlSpacing

    RowLayout {
        spacing: Theme.horizontalTextItemSpacing
        SectionHeader { text: Assets.text("ElgatoDevice_SampleRecorder_Title") }
        InfoIcon { tip: Assets.text("ElgatoDevice_SampleRecorder_Description") }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        AbstractButton {  // Rec (ToggleButtonMStyle with the red "checked" overrides)
            id: rec
            visible: !sc.hasRecording || sc.recording
            enabled: sc.inputId !== "" && !sc.playing
            hoverEnabled: true
            implicitHeight: 32
            implicitWidth: recRow.implicitWidth + 24
            onClicked: Engine.soundCheckAction(sc.recording ? "stop" : "record", sc.inputId)
            background: Rectangle {
                radius: 4
                color: sc.recording ? (rec.hovered ? Qt.darker(Theme.colorSurfaceNegativeStrongDefault, 1.1)
                                                   : Theme.colorSurfaceNegativeStrongDefault)
                     : rec.down ? Theme.controlFillColorTertiaryBrush
                     : rec.hovered ? Theme.controlFillColorSecondaryBrush : Theme.controlFillColorDefaultBrush
                border.width: sc.recording ? 0 : 1
                border.color: Theme.controlStrokeColorDefaultBrush
            }
            contentItem: Item {
                Row {
                    id: recRow
                    anchors.centerIn: parent
                    spacing: 8
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 16
                        height: 16
                        radius: 8
                        color: sc.recording ? "#ffffff" : Theme.colorSurfaceNegativeStrongDefault
                        opacity: rec.enabled ? 1 : 0.4
                    }
                    WText {
                        anchors.verticalCenter: parent.verticalCenter
                        variant: "body"
                        text: "Rec"
                        color: sc.recording ? "#ffffff" : rec.enabled ? Theme.textFillColorPrimary
                                                                      : Theme.textFillColorDisabledBrush
                    }
                }
            }
        }
        QuietButton {
            visible: sc.hasRecording && !sc.recording
            glyph: sc.playing ? Icons.pauseOutlinedM : Icons.playOutlinedM
            onClicked: Engine.soundCheckAction(sc.playing ? "stop" : "play")
        }
        QuietButton {
            visible: sc.hasRecording && !sc.recording
            glyph: Icons.trashOutlinedM
            onClicked: Engine.soundCheckAction("reset")
        }

        Rectangle {  // the recording, SolidBackgroundFillColorBase with 8,5 padding
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            radius: 4
            color: Theme.solidBackgroundFillColorBase
            Row {
                id: barRow
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                anchors.topMargin: 5
                anchors.bottomMargin: 5
                spacing: 3.5
                readonly property real barWidth: Math.max(2, (width - 25 * spacing) / 26)
                Repeater {
                    model: 26
                    Rectangle {
                        required property int index
                        readonly property real level: index < sc.recordedBars ? (sc.bars[index] || 0) : 0
                        anchors.bottom: parent.bottom
                        width: barRow.barWidth
                        height: Math.max(width, level * barRow.height)
                        radius: width / 2
                        color: index < sc.playedBars ? Style.accentFillDefault : Theme.textFillColorPrimary
                        Behavior on height { NumberAnimation { duration: 375; easing.type: Easing.OutCubic } }
                    }
                }
            }
        }

        AbstractButton {  // DropDownButton: output for playback
            id: outputButton
            hoverEnabled: true
            implicitHeight: 32
            implicitWidth: 64
            ToolTip.visible: hovered
            ToolTip.delay: 600
            ToolTip.text: Assets.text("ElgatoDevice_SampleRecorder_OutputDeviceButtonText")
            onClicked: outputMenu.popup(outputButton, 0, outputButton.height + 4)
            background: Rectangle {
                radius: 4
                color: outputButton.down ? Theme.controlFillColorTertiaryBrush
                     : outputButton.hovered ? Theme.controlFillColorSecondaryBrush : Theme.controlFillColorDefaultBrush
                border.width: 1
                border.color: Theme.controlStrokeColorDefaultBrush
            }
            contentItem: Item {
                Row {
                    anchors.centerIn: parent
                    spacing: 8
                    FontIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        glyph: Icons.headphonesOutlinedM
                    }
                    FontIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        glyph: Icons.chevronDownOutlinedM
                        size: "S"
                        font.pixelSize: 12
                    }
                }
            }
        }
        Menu {
            id: outputMenu
            Repeater {
                model: Engine.outputDevices
                MenuItem {
                    required property var modelData
                    text: modelData.description
                    checkable: true
                    checked: sc.st.output === modelData.name
                    onTriggered: Engine.setSoundCheckOutput(modelData.name)
                }
            }
        }
    }
}
