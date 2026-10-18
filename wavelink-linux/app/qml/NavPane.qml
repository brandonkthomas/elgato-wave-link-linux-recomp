// NavigationView pane (Left mode, OpenPaneLength 200): section headers, 36 px items with 4,2 margins,
// a 3×16 accent selection pill, and Marketplace/Settings in the footer.
import QtQuick
import QtQuick.Controls.Basic
import WaveLink.Backend

Item {
    id: pane
    property string current: "mixes"
    property bool compact: false
    signal navigate(string page)

    width: compact ? 48 : 200

    component NavItem: AbstractButton {
        id: item
        property string page
        property string glyph
        property string label
        width: pane.width
        height: 36 + 4
        hoverEnabled: true
        onClicked: pane.navigate(page)
        background: Item {
            Rectangle {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                anchors.topMargin: 2
                anchors.bottomMargin: 2
                radius: 4
                color: item.down ? Theme.subtleFillColorTertiary
                     : (item.hovered || pane.current === item.page) ? Theme.subtleFillColorSecondary : "transparent"
                Rectangle {
                    visible: pane.current === item.page
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: 3
                    height: item.down ? 10 : 16
                    radius: 2
                    color: Style.accentFillDefault
                    Behavior on height { NumberAnimation { duration: 167 } }
                }
            }
        }
        contentItem: Row {
            leftPadding: 4 + 12
            spacing: 12
            FontIcon {
                anchors.verticalCenter: parent.verticalCenter
                glyph: item.glyph
                size: "S"
                width: 20
                height: 20
            }
            WText {
                visible: !pane.compact
                anchors.verticalCenter: parent.verticalCenter
                variant: "body"
                text: item.label
                width: pane.width - 60
            }
        }
    }
    component NavHeader: WText {
        visible: !pane.compact
        width: pane.width
        height: 40
        leftPadding: 16
        verticalAlignment: Text.AlignVCenter
        variant: "body"
    }

    Column {
        anchors.top: parent.top
        anchors.topMargin: 4
        width: parent.width

        NavHeader { text: Assets.text("NavigationBar_ElgatoDevices") }
        NavItem {
            visible: Engine.wave3 && Engine.wave3.connected === true
            height: visible ? 40 : 0
            page: "device"
            glyph: Icons.microphoneOutlinedM
            label: "Elgato Wave:3"
        }
        Rectangle {  // NavigationViewItemSeparator
            width: parent.width
            height: 1 + 3 + 4
            color: "transparent"
            Rectangle { y: 3; x: 0; width: parent.width; height: 1; color: Theme.dividerStrokeColorDefaultBrush }
        }
        NavHeader { text: Assets.text("NavigationBar_MixesAndEffects") }
        NavItem { page: "mixes"; glyph: Icons.volumeMixerOutlinedL; label: Assets.text("NavigationBar_Mixes") }
    }

    Column {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
        width: parent.width
        NavItem { page: "marketplace"; glyph: Icons.logoMarketplaceOutlinedL; label: Assets.text("NavigationBar_Marketplace") }
        NavItem {
            page: "settings"
            glyph: Icons.settingsOutlinedM
            label: Assets.text("Elgato.BaseClasses.WinUI/Resources/Settings_SettingsTitle")
        }
    }
}
