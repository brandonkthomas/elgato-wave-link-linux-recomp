// SettingsView (ContentDialogEx XL): categories on the left, the page on the right. General holds the
// color theme, start at login and "Keep app minimized"; Audio devices holds "Lock system devices",
// which here keeps Wave Link System as the system's default output.
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import WaveLink.Backend

Popup {
    id: dialog
    property int category: 0

    function text(key) { return Assets.text(key) }

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

    component SettingRow: RowLayout {  // title and description left, control right
        id: settingRow
        property string title
        property string description
        default property alias control: slot.data
        Layout.fillWidth: true
        spacing: 16
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            WText {
                Layout.fillWidth: true
                variant: "body"
                text: settingRow.title
            }
            WText {
                visible: text !== ""
                Layout.fillWidth: true
                variant: "caption"
                wrapMode: Text.Wrap
                color: Theme.textFillColorSecondaryBrush
                text: settingRow.description
            }
        }
        Item {
            id: slot
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
    }

    contentItem: ColumnLayout {
        spacing: 0
        Rectangle {
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
                spacing: 16
                WText {
                    variant: "subtitle"
                    text: dialog.text("Elgato.BaseClasses.WinUI/Resources/Settings_SettingsTitle")
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 16
                    Column {  // categories, NavigationView items
                        Layout.preferredWidth: 200
                        Layout.alignment: Qt.AlignTop
                        spacing: 4
                        Repeater {
                            model: [dialog.text("Elgato.BaseClasses.WinUI/Resources/Settings_GeneralCategoryTitle"),
                                    dialog.text("Settings_AudioDevicesCategory")]
                            AbstractButton {
                                id: item
                                required property int index
                                required property string modelData
                                width: 200
                                height: 36
                                hoverEnabled: true
                                onClicked: dialog.category = index
                                background: Rectangle {
                                    radius: 4
                                    color: dialog.category === item.index ? Theme.subtleFillColorSecondary
                                         : item.hovered ? Theme.subtleFillColorSecondary : "transparent"
                                    Rectangle {
                                        visible: dialog.category === item.index
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 3
                                        height: 16
                                        radius: 1.5
                                        color: Style.accentFillDefault
                                    }
                                }
                                contentItem: WText {
                                    leftPadding: 12
                                    verticalAlignment: Text.AlignVCenter
                                    variant: "body"
                                    text: item.modelData
                                }
                            }
                        }
                    }
                    Flickable {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        contentHeight: pages.implicitHeight
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar {}
                        ColumnLayout {
                            id: pages
                            width: parent.width - 16
                            spacing: 16

                            // ---- General ----
                            SectionHeader {
                                visible: dialog.category === 0
                                text: dialog.text("Elgato.BaseClasses.WinUI/Resources/Settings_General_Personalization")
                            }
                            SettingRow {
                                visible: dialog.category === 0
                                title: dialog.text("Elgato.BaseClasses.WinUI/Resources/Settings_General_ColorTheme")
                                WComboBox {
                                    width: 200
                                    model: [dialog.text("Elgato.BaseClasses.WinUI/Resources/ThemePreference_SameAsSystem"),
                                            dialog.text("Elgato.BaseClasses.WinUI/Resources/ThemePreference_Light"),
                                            dialog.text("Elgato.BaseClasses.WinUI/Resources/ThemePreference_Dark")]
                                    currentIndex: AppSettings.colorTheme
                                    onActivated: (i) => AppSettings.colorTheme = i
                                }
                            }
                            SectionHeader {
                                visible: dialog.category === 0
                                Layout.topMargin: 8
                                text: "System"
                            }
                            SettingRow {
                                visible: dialog.category === 0
                                title: dialog.text("Elgato.BaseClasses.WinUI/Resources/Settings_General_AutoStartupTitle")
                                WToggleSwitch {
                                    checked: AppSettings.startAtLogin
                                    onToggled: AppSettings.startAtLogin = checked
                                }
                            }
                            SettingRow {
                                visible: dialog.category === 0
                                title: dialog.text("Elgato.BaseClasses.WinUI/Resources/Settings_General_KeepAppMinimizedTitle")
                                description: dialog.text("Elgato.BaseClasses.WinUI/Resources/Settings_General_KeepAppMinimizedDesc")
                                WToggleSwitch {
                                    checked: AppSettings.keepMinimized
                                    onToggled: AppSettings.keepMinimized = checked
                                }
                            }

                            // ---- Audio devices ----
                            SettingRow {
                                visible: dialog.category === 1
                                title: dialog.text("Settings_AudioDevices_LockSystemDevices")
                                description: "Keep Wave Link System as the system's default output, so apps play into the mixer. Turning this off gives the default back to the device it had before."
                                WToggleSwitch {
                                    checked: Engine.manageDefaultSink
                                    onToggled: Engine.manageDefaultSink = checked
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
            Item { Layout.fillWidth: true }
            WButton {
                accent: true
                text: dialog.text("MixingTable_CloseWindow_Button")
                onClicked: dialog.close()
            }
        }
    }
}
