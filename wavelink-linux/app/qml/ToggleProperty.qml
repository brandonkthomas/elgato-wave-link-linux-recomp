// ToggleSwitchPropertyControl: switch, title 8 px to its right, optional extra content (a logo in Wave
// Link), and the info button.
import QtQuick
import QtQuick.Layouts

RowLayout {
    id: control
    property string title
    property string tip
    property bool checked
    default property alias extra: extraSlot.data
    signal toggled(bool checked)
    spacing: 0

    WToggleSwitch {  // SwitchWithoutOnOffAndReducedTopBottomMargin
        Layout.preferredHeight: 28
        checked: control.checked
        onToggled: control.toggled(checked)
    }
    WText {
        Layout.leftMargin: Theme.horizontalTextItemSpacing
        variant: "body"
        text: control.title
        color: control.enabled ? Theme.textFillColorPrimary : Theme.textFillColorDisabledBrush
    }
    Item {
        id: extraSlot
        Layout.leftMargin: children.length > 0 ? Theme.horizontalTextItemSpacing : 0
        implicitWidth: childrenRect.width
        implicitHeight: childrenRect.height
    }
    InfoIcon {
        visible: control.tip !== ""
        Layout.leftMargin: Theme.horizontalTextItemSpacing
        tip: control.tip
    }
    Item { Layout.fillWidth: true }
}
