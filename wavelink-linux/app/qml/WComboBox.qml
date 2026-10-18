// WinUI 3 ComboBox: button-like field with a chevron; popup list with 4 px corners and a selection pill.
import QtQuick
import QtQuick.Controls.Basic

ComboBox {
    id: control
    hoverEnabled: true
    implicitHeight: 32
    font.family: Style.textFont
    font.pixelSize: Theme.bodyTextBlockFontSize

    background: Rectangle {
        radius: 4
        color: control.down ? Theme.controlFillColorTertiaryBrush
             : control.hovered ? Theme.controlFillColorSecondaryBrush : Theme.controlFillColorDefaultBrush
        border.width: 1
        border.color: Theme.controlStrokeColorDefaultBrush
    }
    contentItem: WText {
        leftPadding: 11
        rightPadding: 32
        variant: "body"
        text: control.displayText
        verticalAlignment: Text.AlignVCenter
    }
    indicator: FontIcon {
        x: control.width - width - 10
        y: (control.height - height) / 2
        glyph: Icons.chevronDownOutlinedM
        size: "S"
        color: Theme.textFillColorSecondaryBrush
    }
    delegate: ItemDelegate {
        id: entry
        required property int index
        required property var modelData
        width: control.width - 8
        x: 4
        height: 36
        hoverEnabled: true
        background: Rectangle {
            radius: 4
            color: entry.hovered ? Theme.subtleFillColorSecondary
                 : control.currentIndex === entry.index ? Theme.subtleFillColorSecondary : "transparent"
            Rectangle {
                visible: control.currentIndex === entry.index
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                width: 3
                height: 16
                radius: 2
                color: Style.accentFillDefault
            }
        }
        contentItem: WText {
            leftPadding: 8
            variant: "body"
            verticalAlignment: Text.AlignVCenter
            text: control.textRole ? entry.modelData[control.textRole] : entry.modelData
        }
    }
    popup: Popup {
        y: control.height + 2
        width: control.width
        padding: 4
        implicitHeight: contentItem.implicitHeight + 8
        background: Rectangle {
            radius: 8
            color: Theme.solidBackgroundFillColorQuarternary
            border.width: 1
            border.color: Theme.surfaceStrokeColorFlyout
        }
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
        }
    }
}
