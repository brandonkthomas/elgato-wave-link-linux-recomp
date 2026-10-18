// MainOutputDeviceView: the ear icon, the device the listening mix plays on, and a chevron; picking a
// device routes the selected (listening) mix to it.
import QtQuick
import QtQuick.Controls.Basic
import WaveLink.Backend

AbstractButton {
    id: button
    property var mix  // the mix being listened to
    readonly property string current: mix && mix.outputs && mix.outputs.length > 0 ? mix.outputs[0] : ""
    readonly property string currentDescription: {
        for (const d of Engine.outputDevices) if (d.name === current) return d.description
        return current === "" ? "—" : current
    }

    implicitHeight: Theme.mixingTableHeaderButtonHeight
    implicitWidth: Math.min(400, content.implicitWidth + 24)
    hoverEnabled: true
    onClicked: menu.popup(button, 0, button.height + 4)

    background: Rectangle {  // DefaultButtonStyle (ControlFill + elevation border)
        radius: 4
        color: button.down ? Theme.controlFillColorTertiaryBrush
             : button.hovered ? Theme.controlFillColorSecondaryBrush : Theme.controlFillColorDefaultBrush
        border.width: 1
        border.color: Theme.controlStrokeColorDefaultBrush
    }
    contentItem: Item {
        Row {
            id: content
            anchors.centerIn: parent
            spacing: 8
            FontIcon { anchors.verticalCenter: parent.verticalCenter; glyph: Icons.earOutlinedL; size: "S" }
            WText {
                anchors.verticalCenter: parent.verticalCenter
                variant: "body"
                text: button.currentDescription
                width: Math.min(implicitWidth, 320)
            }
            FontIcon {
                anchors.verticalCenter: parent.verticalCenter
                glyph: Icons.chevronDownOutlinedM
                size: "S"
                color: Theme.textFillColorSecondaryBrush
            }
        }
    }

    Menu {
        id: menu
        Repeater {
            model: Engine.outputDevices
            MenuItem {
                required property var modelData
                text: modelData.description
                checkable: true
                checked: modelData.name === button.current
                onTriggered: if (button.mix) Engine.setMixOutputs(button.mix.id, [modelData.name])
            }
        }
    }
}
