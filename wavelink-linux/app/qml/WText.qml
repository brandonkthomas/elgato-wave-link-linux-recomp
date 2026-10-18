// Text using Wave Link's type ramp (Elgato's TextBlock.xaml over WinUI's TextBlock styles):
// all styles use the bundled Univers Next Pro; BaseTextBlockStyle is Medium, Body/Caption Normal.
import QtQuick

Text {
    property string variant: "body"  // caption | body | bodyStrong | subtitle | title | titleLarge

    font.family: Style.textFont
    font.pixelSize: ({ caption: Theme.captionTextBlockFontSize, body: Theme.bodyTextBlockFontSize,
                       bodyStrong: Theme.bodyTextBlockFontSize, subtitle: Theme.subtitleTextBlockFontSize,
                       title: Theme.titleTextBlockFontSize, titleLarge: Theme.titleLargeTextBlockFontSize })[variant]
    font.weight: (variant === "body" || variant === "caption") ? Font.Normal : Font.Medium
    color: Theme.textFillColorPrimary
    elide: Text.ElideRight
    renderType: Text.NativeRendering
}
