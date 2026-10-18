// HorizontalLevelSlider.xaml: a 14 px rounded frame holding left/right meters (5 px each) with a
// transparent-track slider on top whose 14 px thumb uses the accent fill (LevelmeterSliderResources).
import QtQuick
import QtQuick.Controls.Basic

Item {
    id: root
    property real value: 1
    property real peakLeft: 0
    property real peakRight: 0
    property bool muted: false
    signal moved(real value)
    readonly property bool pressed: slider.pressed

    implicitWidth: 80
    implicitHeight: 22  // 14 px frame + 4 px padding on both sides

    Rectangle {
        id: frame
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 4 + 3
        anchors.rightMargin: 4 + 3
        anchors.verticalCenter: parent.verticalCenter
        height: 14
        radius: 5
        color: "transparent"
        border.width: 1
        border.color: Theme.controlStrokeColorSecondaryBrush

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: 5
            color: Theme.controlFillColorDefaultBrush
            clip: true
            Column {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                spacing: 2
                LevelMeter { width: parent.width; height: 5; peak: root.muted ? 0 : root.peakLeft }
                LevelMeter { width: parent.width; height: 5; peak: root.muted ? 0 : root.peakRight }
            }
        }
    }

    Slider {
        id: slider
        anchors.fill: parent
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        from: 0
        to: 1
        value: root.value
        live: true
        padding: 0
        hoverEnabled: true
        onMoved: root.moved(value)
        background: Item {}
        handle: Rectangle {
            x: slider.visualPosition * (slider.availableWidth - width)
            y: (slider.availableHeight - height) / 2
            width: 14
            height: 14
            radius: 7
            color: Style.accentFillDefault
            scale: slider.pressed ? 0.86 : slider.hovered ? 1.167 : 1
            Behavior on scale { NumberAnimation { duration: 167 } }
        }
    }
}
