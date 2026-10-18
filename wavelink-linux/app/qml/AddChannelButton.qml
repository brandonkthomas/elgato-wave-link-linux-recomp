// AddInputChannelButton: quiet 60 px card "+ Create channel" opening the list of channels that can be
// added (free software channels and capture devices).
import QtQuick
import QtQuick.Controls.Basic
import WaveLink.Backend

AbstractButton {
    id: button
    height: Theme.mixingTableElementHeight
    hoverEnabled: true
    onClicked: menu.popup(button, 0, button.height + 4)

    readonly property var softwareNames: ["Music", "Browser", "Voice", "SFX", "Game", "Aux 1", "Aux 2", "System"]
    readonly property var usedNames: Engine.inputs.map(i => i.name)
    readonly property var usedDevices: Engine.inputs.filter(i => i.kind === "hardware").map(i => i.device)

    background: Rectangle {
        radius: 4
        color: button.down ? Theme.subtleFillColorTertiary : button.hovered ? Theme.subtleFillColorSecondary
                                                                         : Theme.mixingTableElementQuietBackgroundBrush
        border.width: 1
        border.color: Theme.mixingTableElementBorderBrush
    }
    contentItem: Item {
        Row {
            anchors.centerIn: parent
            spacing: 8
            FontIcon {
                anchors.verticalCenter: parent.verticalCenter
                glyph: Icons.addOutlinedM
                size: "S"
                color: Theme.textFillColorSecondaryBrush
            }
            WText {
                anchors.verticalCenter: parent.verticalCenter
                variant: "body"
                text: Assets.text("MixingTable_AddChannel_Button")
                color: Theme.textFillColorSecondaryBrush
            }
        }
    }

    Menu {
        id: menu
        Repeater {
            model: button.softwareNames.filter(n => button.usedNames.indexOf(n) < 0)
            MenuItem {
                required property string modelData
                text: modelData
                onTriggered: Engine.addSoftwareInput(modelData)
            }
        }
        MenuSeparator { visible: appRepeater.count > 0 }
        Repeater {
            id: appRepeater
            model: {
                const seen = {}
                return Engine.apps.filter(a => a.key !== "" && a.rule === "" && !seen[a.key] && (seen[a.key] = true))
            }
            MenuItem {
                required property var modelData
                text: modelData.app !== "" ? modelData.app : modelData.binary
                onTriggered: Engine.addAppInput(modelData.key, text)
            }
        }
        MenuSeparator { visible: Engine.inputDevices.length > 0 }
        Repeater {
            model: Engine.inputDevices.filter(d => button.usedDevices.indexOf(d.name) < 0)
            MenuItem {
                required property var modelData
                text: modelData.description
                onTriggered: Engine.addHardwareInput(modelData.name, modelData.description)
            }
        }
    }
}
