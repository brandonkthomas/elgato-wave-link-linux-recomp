// ElgatoDevicePropertySliderControl: icon and title (with optional content at the top right), then
// the slider with its value text (44 px, right-aligned) 8 px below.
import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: control
    property string glyph
    property string title
    property string valueText
    property alias from: slider.from
    property alias to: slider.to
    property alias stepSize: slider.stepSize
    property alias value: slider.value
    property alias slider: slider
    default property alias topRight: topRightSlot.data
    signal moved(real value)
    spacing: Theme.headerControlSpacing

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.horizontalTextItemSpacing
        FontIcon {
            visible: control.glyph !== ""
            glyph: control.glyph
        }
        WText {
            Layout.fillWidth: true
            variant: "body"
            text: control.title
        }
        Item {
            id: topRightSlot
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.horizontalTextItemSpacing
        WSlider {
            id: slider
            Layout.fillWidth: true
            onMoved: control.moved(value)
        }
        WText {
            visible: control.valueText !== ""
            Layout.preferredWidth: 44
            Layout.bottomMargin: 4
            horizontalAlignment: Text.AlignRight
            variant: "body"
            text: control.valueText
        }
    }
}
