// Segmented with SegmentedTabHeaderStyle: 8 px padding over a 1 px ControlStrokeColorDefault bottom
// border; items are 32 px with 12 px side padding, the selected one on a raised pill with a short accent
// underline.
import QtQuick
import QtQuick.Controls.Basic

Item {
    id: strip
    property var tabs: []  // labels
    property int current: 0
    implicitWidth: row.implicitWidth + 16
    implicitHeight: 48

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.controlStrokeColorDefaultBrush
    }
    Row {
        id: row
        x: 8
        y: 8
        spacing: 2
        Repeater {
            model: strip.tabs
            AbstractButton {
                id: tab
                required property int index
                required property string modelData
                hoverEnabled: true
                height: 32
                width: label.implicitWidth + 24
                onClicked: strip.current = index
                background: Rectangle {
                    radius: 4
                    color: strip.current === tab.index ? Theme.controlFillColorDefaultBrush
                         : tab.hovered ? Theme.subtleFillColorSecondary : "transparent"
                    border.width: strip.current === tab.index ? 1 : 0
                    border.color: Theme.controlStrokeColorDefaultBrush
                    Rectangle {
                        visible: strip.current === tab.index
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 2
                        width: 16
                        height: 3
                        radius: 1.5
                        color: Style.accentFillDefault
                    }
                }
                contentItem: WText {
                    id: label
                    variant: "body"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: tab.modelData
                }
            }
        }
    }
}
