// MainWindow + MainView: custom 32 px title bar over a Mica-like base, the navigation pane, and the
// content area (LayerFillColorDefault with an 8 px top-left corner, as WinUI's NavigationView draws it).
import QtQuick
import QtQuick.Window
import QtQuick.Controls.Basic
import WaveLink.Backend

ApplicationWindow {
    id: window
    width: 1600
    height: 900
    minimumWidth: 760
    minimumHeight: 480
    visible: true
    title: "Wave Link"
    flags: Qt.Window | Qt.FramelessWindowHint
    color: Theme.solidBackgroundFillColorBase
    font.family: Style.textFont

    property string page: "mixes"
    property int deviceTab: 0
    property string openChannel
    property string openEffect

    SettingsDialog { id: settingsDialog }

    // Color theme setting: follow the system, or force light or dark.
    function applyColorTheme() {
        Theme.dark = AppSettings.colorTheme === 0 ? Qt.binding(() => Qt.styleHints.colorScheme !== Qt.ColorScheme.Light)
                                                  : AppSettings.colorTheme === 2
    }
    Component.onCompleted: {
        applyColorTheme()
        if (page === "settings") {  // --page settings
            page = "mixes"
            settingsDialog.open()
        }
    }
    Connections {
        target: AppSettings
        function onColorThemeChanged() { window.applyColorTheme() }
    }

    TitleBar {
        id: titleBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        window: window
        onPaneToggleClicked: nav.compact = !nav.compact
    }

    NavPane {
        id: nav
        anchors.left: parent.left
        anchors.top: titleBar.bottom
        anchors.bottom: parent.bottom
        current: window.page
        onNavigate: (p) => {
            if (p === "settings") settingsDialog.open()  // a dialog in Wave Link, not a page
            else window.page = p
        }
    }

    Rectangle {
        id: content
        anchors.left: nav.right
        anchors.right: parent.right
        anchors.top: titleBar.bottom
        anchors.bottom: parent.bottom
        color: Theme.layerFillColorDefaultBrush
        topLeftRadius: 8
        border.width: 1
        border.color: Theme.cardStrokeColorDefaultBrush

        MixesPage {
            anchors.fill: parent
            visible: window.page === "mixes"
            openChannel: window.openChannel
            openEffect: window.openEffect
        }
        DevicePage {
            anchors.fill: parent
            visible: window.page === "device"
            initialTab: window.deviceTab
        }
        MarketplacePage {
            anchors.fill: parent
            visible: window.page === "marketplace"
        }

        WText {
            anchors.centerIn: parent
            visible: !Engine.connected
            variant: "subtitle"
            color: Theme.textFillColorSecondaryBrush
            text: "Waiting for the Wave Link engine (wavelinkd)…"
        }
    }

    // Window edges for resizing a frameless window.
    component Edge: MouseArea {
        property int edges
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        onPressed: window.startSystemResize(edges)
    }
    Edge { edges: Qt.LeftEdge; anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 4; cursorShape: Qt.SizeHorCursor }
    Edge { edges: Qt.RightEdge; anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 4; cursorShape: Qt.SizeHorCursor }
    Edge { edges: Qt.BottomEdge; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 4; cursorShape: Qt.SizeVerCursor }
    Edge { edges: Qt.TopEdge; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; height: 3; cursorShape: Qt.SizeVerCursor }
    Edge { edges: Qt.RightEdge | Qt.BottomEdge; anchors.right: parent.right; anchors.bottom: parent.bottom; width: 8; height: 8; cursorShape: Qt.SizeFDiagCursor }
    Edge { edges: Qt.LeftEdge | Qt.BottomEdge; anchors.left: parent.left; anchors.bottom: parent.bottom; width: 8; height: 8; cursorShape: Qt.SizeBDiagCursor }
}
