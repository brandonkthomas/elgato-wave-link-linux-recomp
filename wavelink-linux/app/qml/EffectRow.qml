// AudioEffectControl: on/off toggle and the effect's name; hovering fades in "Edit effect settings" and
// "Remove effect". Right-click offers Rename and Remove; double-click opens the settings.
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import WaveLink.Backend

Item {
    id: row
    property string inputId
    property string effectId
    signal settingsRequested()

    readonly property var effect: { Engine.effectRevision; return Engine.effects(inputId).find(e => e.id === effectId) }
    readonly property string displayName: {
        if (!effect) return ""
        if (effect.name) return effect.name
        const t = Engine.effectTypes.find(t => t.type === effect.type)
        return t ? t.name : effect.type
    }

    implicitHeight: 48

    function startRename() {
        nameEditor.text = row.displayName
        nameEditor.visible = true
        nameEditor.forceActiveFocus()
        nameEditor.selectAll()
    }
    function finishRename(commit) {
        if (!nameEditor.visible) return
        nameEditor.visible = false
        const name = nameEditor.text.trim()
        if (commit && name !== "" && name !== row.displayName) Engine.renameEffect(row.inputId, row.effectId, name)
    }

    HoverHandler { id: hover }
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: menu.popup()
    }
    TapHandler { onDoubleTapped: row.settingsRequested() }

    Rectangle {  // ListViewItem hover fill, reaching 16 px past the content like PropertyListViewItemStyle
        anchors.fill: parent
        anchors.leftMargin: -16
        anchors.rightMargin: -16
        radius: 4
        color: hover.hovered ? Theme.subtleFillColorSecondary : "transparent"
        Behavior on color { ColorAnimation { duration: 83 } }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        WToggleSwitch {
            checked: row.effect ? !row.effect.bypass : false
            onToggled: Engine.setEffectBypass(row.inputId, row.effectId, !checked)
        }
        WText {
            visible: !nameEditor.visible
            Layout.leftMargin: Theme.horizontalTextItemSpacing
            Layout.fillWidth: true
            variant: "body"
            text: row.displayName
        }
        WTextField {
            id: nameEditor
            visible: false
            Layout.leftMargin: 6
            Layout.fillWidth: true
            onAccepted: row.finishRename(true)
            onActiveFocusChanged: if (!activeFocus) row.finishRename(true)
            Keys.onEscapePressed: row.finishRename(false)
        }
        Row {
            spacing: 8
            opacity: hover.hovered ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 83 } }
            QuietButton {
                glyph: Icons.adjustHorizontalOutlinedM
                tip: Assets.text("MixingTable_EditEffectSettingsButtonText")
                onClicked: row.settingsRequested()
            }
            QuietButton {
                glyph: Icons.trashOutlinedM
                tip: Assets.text("MixingTable_RemoveEffectButtonText")
                onClicked: Engine.removeEffect(row.inputId, row.effectId)
            }
        }
    }

    Menu {
        id: menu
        MenuItem {
            text: Assets.text("MixingTable_RenameEffectButtonText")
            onTriggered: row.startRename()
        }
        MenuSeparator {}
        MenuItem {
            text: Assets.text("MixingTable_RemoveEffectButtonText")
            onTriggered: Engine.removeEffect(row.inputId, row.effectId)
        }
    }
}
