// Values Wave Link takes from the platform rather than its own tokens: the system accent color
// (WinUI derives AccentFill/AccentTextFill from the Windows accent) and the bundled fonts.
pragma Singleton
import QtQuick
import WaveLink.Backend

QtObject {
    readonly property SystemPalette palette: SystemPalette { colorGroup: SystemPalette.Active }
    readonly property color systemAccent: palette.accent.valid ? palette.accent : "#0078d4"

    // WinUI uses SystemAccentColorLight2 / Light3 in dark mode and Dark1 / Dark2 in light mode.
    readonly property color accentFillDefault: Theme.dark ? Qt.lighter(systemAccent, 1.55) : Qt.darker(systemAccent, 1.2)
    readonly property color accentFillSecondary: Qt.alpha(accentFillDefault, 0.9)
    readonly property color accentTextFillPrimary: Theme.dark ? Qt.lighter(systemAccent, 1.8) : Qt.darker(systemAccent, 1.45)

    readonly property string textFont: Assets.textFont !== "" ? Assets.textFont : "Segoe UI Variable Text"
    readonly property string iconFont: Assets.iconFont
}
