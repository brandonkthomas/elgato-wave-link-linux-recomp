// AudioEffectsList: Voice Focus (Elgato mics), the channel's effects in processing order, and
// "Add effect" listing the installed effects. Voice Focus is not one of the listed effects in Wave Link:
// it is a fixed slot, only bypassed when off, that runs before the user's effects.
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import WaveLink.Backend

ColumnLayout {
    id: list
    property string inputId
    property bool showVoiceFocus: false
    property bool showAddButton: true
    spacing: 0

    readonly property var chain: { Engine.effectRevision; return inputId !== "" ? Engine.effects(inputId) : [] }
    readonly property var voiceFocus: chain.find(e => e.type === "VoiceFocus")
    // Rows are keyed by effect id so that parameter changes do not rebuild them.
    readonly property string rowKey: chain.filter(e => e.type !== "VoiceFocus").map(e => e.id).join(",")
    readonly property var rowIds: rowKey === "" ? [] : rowKey.split(",")
    readonly property var addable: Engine.effectTypes.filter(t => t.type !== "VoiceFocus")

    VoiceFocusControl {
        visible: list.showVoiceFocus
        Layout.fillWidth: true
        enabled: list.inputId !== ""
        effect: list.voiceFocus
        available: Engine.effectTypes.some(t => t.type === "VoiceFocus")
        onSwitched: (value) => {
            if (list.voiceFocus) Engine.setEffectBypass(list.inputId, list.voiceFocus.id, !value)
            else if (value) Engine.addEffect(list.inputId, "VoiceFocus", 0)
        }
        onEnhancementMoved: (value) => {
            if (list.voiceFocus) Engine.setEffectParam(list.inputId, list.voiceFocus.id, "Enhancement", value)
        }
    }

    Repeater {
        model: list.rowIds
        EffectRow {
            required property string modelData
            Layout.fillWidth: true
            inputId: list.inputId
            effectId: modelData
            onSettingsRequested: {
                editor.effectId = modelData
                editor.open()
            }
        }
    }

    WButton {
        id: addButton
        visible: list.showAddButton
        Layout.fillWidth: true
        Layout.topMargin: 16
        Layout.bottomMargin: 16
        Layout.preferredHeight: 40
        enabled: list.inputId !== ""
        glyph: Icons.addOutlinedM
        text: Assets.text("AddAudioEffect")
        onClicked: addMenu.popup(addButton, 0, addButton.height + 4)
    }
    Menu {
        id: addMenu
        Repeater {
            model: list.addable
            MenuItem {
                required property var modelData
                text: modelData.name
                onTriggered: Engine.addEffect(list.inputId, modelData.type)
            }
        }
        MenuItem {
            visible: list.addable.length === 0
            height: visible ? implicitHeight : 0
            enabled: false
            text: Assets.text("ElgatoDevicePage_Effects_NoneAvailable")
        }
    }

    EffectEditor {
        id: editor
        inputId: list.inputId
    }
}
