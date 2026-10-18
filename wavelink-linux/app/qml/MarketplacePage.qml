// NavigationPageMarketplace hosts the Elgato Marketplace web store, which Linux builds cannot use (its
// plugins ship as Windows VST3 / macOS AU). The two Elgato plugins are built in instead.
import QtQuick
import QtQuick.Layouts
import WaveLink.Backend

Item {
    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 520)
        spacing: 16
        FontIcon {
            Layout.alignment: Qt.AlignHCenter
            glyph: Icons.logoMarketplaceOutlinedL
            font.pixelSize: 48
            width: 56
            height: 56
            color: Theme.textFillColorSecondaryBrush
        }
        WText {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            variant: "subtitle"
            text: Assets.text("NavigationBar_Marketplace")
        }
        WText {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            variant: "body"
            wrapMode: Text.Wrap
            color: Theme.textFillColorSecondaryBrush
            text: "The Elgato Marketplace store is not available on Linux. Elgato EQ and Elgato Noise Removal are built in: open a channel or your microphone's Effects tab and choose Add effect."
        }
        WButton {
            Layout.alignment: Qt.AlignHCenter
            text: "Open marketplace.elgato.com"
            onClicked: Qt.openUrlExternally("https://marketplace.elgato.com")
        }
    }
}
