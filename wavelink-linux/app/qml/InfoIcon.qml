// InfoButton: accent info glyph whose tip (a TeachingTip in Wave Link) shows on hover.
import QtQuick
import QtQuick.Controls.Basic

FontIcon {
    property string tip
    glyph: Icons.infoOutlinedM
    size: "S"
    color: Style.accentTextFillPrimary
    HoverHandler { id: infoHover }
    ToolTip.visible: infoHover.hovered && tip !== ""
    ToolTip.text: tip
}
