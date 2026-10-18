// The "Apps" tab of a software channel: apps routed to it (ListViewInputItem: name, "(Unavailable)"
// while not running, remove button) and "Add app" listing running apps that play elsewhere.
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import WaveLink.Backend

ColumnLayout {
    id: routes
    property string inputId
    spacing: 0

    readonly property var routedKeys: Object.keys(Engine.appRoutes).filter(k => Engine.appRoutes[k] === inputId)
    readonly property var candidates: {
        const seen = {}
        return Engine.apps.filter(a => a.key !== "" && a.rule !== routes.inputId && !seen[a.key] && (seen[a.key] = true))
    }
    function appName(key) {
        const a = Engine.apps.find(a => a.key === key)
        return a ? (a.app !== "" ? a.app : a.binary) : key
    }

    Repeater {
        model: routes.routedKeys
        Item {
            id: appRow
            required property string modelData
            readonly property bool running: Engine.apps.some(a => a.key === modelData)
            Layout.fillWidth: true
            implicitHeight: 48
            HoverHandler { id: hover }
            Rectangle {
                anchors.fill: parent
                radius: 4
                color: hover.hovered ? Theme.subtleFillColorSecondary : "transparent"
            }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 8
                spacing: Theme.spaceXs
                FontIcon { glyph: Icons.windowOutlinedM }
                WText {
                    variant: "body"
                    text: routes.appName(appRow.modelData)
                }
                WText {
                    visible: !appRow.running
                    variant: "body"
                    color: Theme.textFillColorDisabledBrush
                    text: Assets.text("MixingTable_UnavailableInBraces")
                }
                Item { Layout.fillWidth: true }
                QuietButton {
                    glyph: Icons.trashOutlinedM
                    tip: Assets.text("MixingTable_RemoveChannel_Button")
                    onClicked: Engine.routeApp(appRow.modelData, "")
                }
            }
        }
    }

    WButton {
        id: addAppButton
        Layout.fillWidth: true
        Layout.margins: 16
        Layout.preferredHeight: 40
        glyph: Icons.addOutlinedM
        text: Assets.text("MixingTable_AddApp_Button")
        enabled: routes.candidates.length > 0
        onClicked: appMenu.popup(addAppButton, 0, addAppButton.height + 4)
    }
    Menu {
        id: appMenu
        Repeater {
            model: routes.candidates
            MenuItem {
                required property var modelData
                text: modelData.app !== "" ? modelData.app : modelData.binary
                onTriggered: Engine.routeApp(modelData.key, routes.inputId)
            }
        }
    }
}
