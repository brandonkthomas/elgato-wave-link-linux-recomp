// Glyph from the Elgato-Icons font. Sizes follow ElgatoFontIconS/M/L (16/20/24 in a 16/20/24 box).
import QtQuick

Text {
    property string glyph
    property string size: "M"

    text: glyph
    font.family: Style.iconFont
    font.pixelSize: size === "S" ? Theme.fontSizeL : size === "L" ? Theme.fontSize2Xl : Theme.fontSizeXl
    width: size === "S" ? Theme.fontLineHeightS : size === "L" ? Theme.fontLineHeightL : Theme.fontLineHeightM
    height: width
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
    color: Theme.textFillColorPrimary
    renderType: Text.NativeRendering
}
